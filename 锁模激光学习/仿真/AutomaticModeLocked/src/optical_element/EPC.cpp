#include "optical_element/EPC.hpp"
#include <algorithm>
#include <stdexcept>

EPC::EPC(const std::vector<Retarder> &retarders) : m_retarders(retarders)
{
    // 空队列中不存在需要检查的ID。
    if (m_retarders.empty())
    {
        throw std::runtime_error("没有设置EPC的retarder");
    }

    //按ID从小到大排列，确保先作用的延迟器位于队列前面。
    std::sort(m_retarders.begin(), m_retarders.end(),
              [](const Retarder &lhs, const Retarder &rhs)
              {
                  return lhs.ID < rhs.ID;
              });

    

    //ID约定从0开始。
    if (m_retarders.front().ID != 0)
    {
        throw std::invalid_argument("[EPC]的retarderID必须从0开始");
    }
    
    //不允许出现重复，且相邻数字间隔为1
    for (std::size_t i = 1; i < m_retarders.size(); ++i)
    {
        const int previous_id = m_retarders[i - 1].ID;
        const int current_id = m_retarders[i].ID;

        if (current_id == previous_id)
        {
            throw std::invalid_argument("[EPC]的retarderID存在重复项");
        }

        if (current_id - previous_id != 1)
        {
            throw std::invalid_argument("[EPC]的retarderID必须间隔为1");
        }
    }

}

OpticalOutputs EPC::propagate(const OpticalField &input_optical_field)
{
    //计算等效的琼斯矩阵
    Eigen::Matrix<std::complex<double>, 2, 2> total_jones_matrix = this->total_jones_matrix();
    
    //计算机经过EPC后的偏振
    OpticalField::Polarization polarization = total_jones_matrix * input_optical_field.polarization();

    //返回
    OpticalOutputs output_fields;
    output_fields.emplace("out",OpticalField{std::move(polarization), input_optical_field.basic_info()});
    return output_fields;
}

void EPC::set_retarder(int ID, double delta_phase_deg)
{
    //判断ID是否有效
    if (ID > m_retarders.size() - 1 || ID < 0)
    {
        throw std::runtime_error("[EPC]设置了无效的ID");
    }

    //设置相位差
    m_retarders[ID].delta_phase_deg = delta_phase_deg;
}

Eigen::Matrix<std::complex<double>, 2, 2> EPC::total_jones_matrix() const
{   
    //默认没有任何相位延迟
    Eigen::Matrix<std::complex<double>, 2, 2> total_jones_matrix;
    total_jones_matrix << 1.0, 0.0,
                          0.0, 1.0;

    for (const auto &retarder : m_retarders)
    {
        //计算retarder琼斯矩阵
        //转系矩阵
        const double orientation_rad = retarder.orientation_deg * std::numbers::pi_v<double> / 180.0;
        double cos_orientation = std::cos(orientation_rad);
        double sin_orientation = std::sin(orientation_rad);
        Eigen::Matrix2d tf_matrix;
        tf_matrix << cos_orientation, sin_orientation,
                    -sin_orientation, cos_orientation;

        //相位延迟矩阵
        const double delta_phase_rad = retarder.delta_phase_deg * std::numbers::pi_v<double> / 180.0;
        std::complex<double> delta_phase = std::polar(1.0, delta_phase_rad);
        Eigen::Matrix<std::complex<double>, 2, 2> delay_matrix;
        delay_matrix << 1.0, 0,
                        0.0, delta_phase;

        //累加到总琼斯矩阵
        total_jones_matrix = tf_matrix.transpose() *  delay_matrix * tf_matrix * total_jones_matrix;
    }
    
    return total_jones_matrix;

}
