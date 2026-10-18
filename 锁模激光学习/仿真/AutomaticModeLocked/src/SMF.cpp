#include "SMF.hpp"
#include <cmath>
#include <complex>
#include <stdexcept>
#include <utility>

SMF::SMF(double gamma_per_w_m, double length_m)
    : m_gamma_per_w_m(gamma_per_w_m),
      m_length_m(length_m)
{
    if (m_gamma_per_w_m < 0.0)
    {
        throw std::invalid_argument("[SMF]非线性系数不能小于0");
    }

    if (m_length_m < 0.0)
    {
        throw std::invalid_argument("[SMF]光纤长度不能小于0");
    }

    //xy->uv的转系矩阵
    const std::complex<double> imaginary_unit{0.0, 1.0};//i
    const double inverse_sqrt_two = 1.0 / std::sqrt(2.0); // 根号2的倒数
    m_xy_to_uv_matrix << 1.0, imaginary_unit,
                         1.0, -imaginary_unit;
    m_xy_to_uv_matrix *= inverse_sqrt_two;
}

OpticalField SMF::propagate(const OpticalField &input_optical_field) const
{

    //创建输出的偏振队列
    OpticalField::Polarization output_polarization = input_optical_field.polarization();

    //计算output_polarization
    const auto &input_polarization = input_optical_field.polarization();
    const Eigen::Matrix2cd uv_to_xy_matrix = m_xy_to_uv_matrix.adjoint();
    for (Eigen::Index column = 0; column < input_polarization.cols(); ++column)
    {
        //首先转化到UV系
        Eigen::Vector2cd uv_polarization = m_xy_to_uv_matrix * input_polarization.col(column);

        //计算kerr效应带来的相位延迟
        //φu = (2γL/3)(Pu + 2Pv)
        //φv = (2γL/3)(Pv + 2Pu)
        const double power_u_w = std::norm(uv_polarization(0));
        const double power_v_w = std::norm(uv_polarization(1));
        const double phase_u_rad = (2.0 * m_gamma_per_w_m * m_length_m / 3.0) * (power_u_w + 2.0 * power_v_w);
        const double phase_v_rad = (2.0 * m_gamma_per_w_m * m_length_m / 3.0) * (power_v_w + 2.0 * power_u_w);

        //kerr效应的作用矩阵
        Eigen::Matrix2cd kerr_matrix;
        kerr_matrix << std::polar(1.0, phase_u_rad), 0.0,
                       0.0, std::polar(1.0, phase_v_rad);

        //转化到xy坐标系并且给输出赋值
        output_polarization.col(column) = uv_to_xy_matrix * kerr_matrix * uv_polarization;
    }

    //返回
    return OpticalField{std::move(output_polarization),input_optical_field.basic_info()};
}
