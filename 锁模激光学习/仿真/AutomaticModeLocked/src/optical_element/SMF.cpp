#include "optical_element/SMF.hpp"
#include "config/config.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>
#include <utility>
#include "DFT.hpp"

SMF::SMF(double gamma_per_w_m, double length_m, double beta2_ps2_per_km)
    : m_gamma_per_w_m(gamma_per_w_m),
      m_length_m(length_m),
      m_beta2_ps2_per_km(beta2_ps2_per_km)
{
    if (m_gamma_per_w_m < 0.0)
    {
        throw std::invalid_argument("[SMF]非线性系数不能小于0");
    }

    if (m_length_m < 0.0)
    {
        throw std::invalid_argument("[SMF]光纤长度不能小于0");
    }

    if (!std::isfinite(m_beta2_ps2_per_km))
    {
        throw std::invalid_argument("[SMF]二阶色散系数必须为有限数，单位ps^2/km");
    }

    //xy->uv的转系矩阵
    const std::complex<double> imaginary_unit{0.0, 1.0};//i
    const double inverse_sqrt_two = 1.0 / std::sqrt(2.0); // 根号2的倒数
    m_xy_to_uv_matrix << 1.0, imaginary_unit,
                         1.0, -imaginary_unit;
    m_xy_to_uv_matrix *= inverse_sqrt_two;
}

OpticalOutputs SMF::propagate(const OpticalInputs &input_optical_fields)
{
    const OpticalField& input_optical_field = input_optical_fields.at("in").get();

    //创建输出的光场
    OpticalField output_optical_field  = input_optical_field;

    //读取SMF.step_length_m作为分段长度，单位m；最后不足一段时只计算剩余距离。
    double max_step_length_m = static_cast<double>(CONFIG.config_["SMF"]["step_length_m"]);
    
    //对称分步：半步色散->整步Kerr->半步色散
    double remaining_length_m = m_length_m;
    while (remaining_length_m > 0.0)
    {
        //计算步长
        double step_length_m = std::min(max_step_length_m, remaining_length_m);
        double half_step_length_m = step_length_m / 2.0;

        //色散和kerr
        dispersion2(output_optical_field, half_step_length_m);
        kerr(output_optical_field, step_length_m);
        dispersion2(output_optical_field, half_step_length_m);

        //写回
        remaining_length_m -= step_length_m;
    }

    //返回
    OpticalOutputs output_fields;
    output_fields.emplace("out",std::move(output_optical_field));
    return output_fields;
}

void SMF::kerr(OpticalField &optical_field, double step_length_m) const
{

    const auto &input_polarization = optical_field.polarization();//原偏振
    OpticalField::Polarization output_polarization(2, input_polarization.cols());//输出光场
    
    const Eigen::Matrix2cd uv_to_xy_matrix = m_xy_to_uv_matrix.adjoint();//转系矩阵
    for (Eigen::Index column = 0; column < input_polarization.cols(); ++column)
    {
        // 首先转化到UV系
        Eigen::Vector2cd uv_polarization = m_xy_to_uv_matrix * input_polarization.col(column);

        // 计算kerr效应带来的相位延迟
        // φu = (2γL/3)(Pu + 2Pv)
        // φv = (2γL/3)(Pv + 2Pu)
        const double power_u_w = std::norm(uv_polarization(0));
        const double power_v_w = std::norm(uv_polarization(1));
        const double phase_u_rad = (2.0 * m_gamma_per_w_m * step_length_m / 3.0) * (power_u_w + 2.0 * power_v_w);
        const double phase_v_rad = (2.0 * m_gamma_per_w_m * step_length_m / 3.0) * (power_v_w + 2.0 * power_u_w);

        // kerr效应的作用矩阵
        Eigen::Matrix2cd kerr_matrix;
        kerr_matrix << std::polar(1.0, phase_u_rad), 0.0,
                       0.0, std::polar(1.0, phase_v_rad);

        // 转化到xy坐标系并且给输出赋值
        output_polarization.col(column) = uv_to_xy_matrix * kerr_matrix * uv_polarization;
    }

    //写回传播后的光
    optical_field = OpticalField{std::move(output_polarization), optical_field.basic_info()};
}

void SMF::dispersion2(OpticalField &optical_field,double step_length_m) const
{
    //获取基础的光场信息
    const auto basic_info = optical_field.basic_info();
    const double sample_stride_fs = basic_info.sample_stride;


    //转化到频域
    DFT::Spectrum spectrum = DFT::forward(optical_field);
    const Eigen::Index sample_size = spectrum.cols();


    //二阶色散系数，转化为fs^2 / m
    const double beta2_fs2_per_m = m_beta2_ps2_per_km * 1000.0;
    
    //角频率步长：2π * （1 / (N * Δt)）
    const double angular_frequency_stride = 2.0 * std::numbers::pi_v<double> / static_cast<double>(sample_size) / sample_stride_fs;
    
    //对频率成分施加二阶色散
    for (Eigen::Index column = 0; column < sample_size; ++column)
    {
        //计算Δf的编号
        const Eigen::Index frequency_index = column < (sample_size + 1) / 2 ? column : column - sample_size;
        
        //计算Δ角频率
        const double omega_rad_per_fs = static_cast<double>(frequency_index) * angular_frequency_stride;

        //计算二阶色散带来的额外相位：H(Ω)=exp(i*beta2*L*Ω^2/2)
        const double phase_rad = 0.5 * beta2_fs2_per_m * step_length_m * omega_rad_per_fs * omega_rad_per_fs;

        //叠加到原本的相位上
        spectrum.col(column) *= std::polar(1.0, phase_rad);
    }

    //转回时域
    optical_field = OpticalField{DFT::inverse(spectrum), basic_info};
}
