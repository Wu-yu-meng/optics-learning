#include "SMF.hpp"
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

}

OpticalField SMF::propagate(const OpticalField &input_optical_field) const
{
    //创建输出的光场
    auto output_polarization = input_optical_field.polarization();
    if (m_length_m == 0.0)
        return OpticalField{std::move(output_polarization), input_optical_field.basic_info()};

    //沿用test的分段规则：用入口峰值功率估计Kerr相移，目标每步不超过0.05 rad，最多256步。
    constexpr double max_nonlinear_phase_per_step_rad = 0.05;
    constexpr std::size_t max_split_step_count = 256;
    double peak_power_w = 0.0;
    const auto &polarization = input_optical_field.polarization();
    for (Eigen::Index column = 0; column < polarization.cols(); ++column)
    {
        const double power_w = std::norm(polarization(0, column)) + std::norm(polarization(1, column));
        peak_power_w = std::max(peak_power_w, power_w);
    }
    const double estimated_max_nonlinear_phase_rad = (4.0 / 3.0) * m_gamma_per_w_m * m_length_m * peak_power_w;
    if (!std::isfinite(estimated_max_nonlinear_phase_rad))
        throw std::invalid_argument("[SMF]非线性相移估计超出数值范围，请检查光功率、非线性系数和长度");
    const double required_step_count = std::ceil(estimated_max_nonlinear_phase_rad / max_nonlinear_phase_per_step_rad);
    const std::size_t split_step_count = static_cast<std::size_t>(std::clamp(required_step_count, 1.0, static_cast<double>(max_split_step_count)));

    const double step_length_m = m_length_m / static_cast<double>(split_step_count);
    if (m_beta2_ps2_per_km != 0.0)
    {
        update_dispersion_cache(output_polarization.cols(), input_optical_field.basic_info().sample_stride, split_step_count);
        dispersion2(output_polarization, m_half_step_transfer);
    }
    for (std::size_t step = 0; step < split_step_count; ++step)
    {
        kerr(output_polarization, step_length_m);
        //相邻半步合并为整步，最后只补半步；与原分步规则相同。
        if (m_beta2_ps2_per_km != 0.0)
            dispersion2(output_polarization, step + 1 == split_step_count ? m_half_step_transfer : m_full_step_transfer);
    }
    return OpticalField{std::move(output_polarization), input_optical_field.basic_info()};
}

void SMF::kerr(OpticalField::Polarization &polarization, double length_m) const
{
    const std::complex<double> imaginary_unit{0.0, 1.0};
    const double inverse_sqrt_two = 1.0 / std::sqrt(2.0);
    const double phase_coefficient = 2.0 * m_gamma_per_w_m * length_m / 3.0;
    for (Eigen::Index column = 0; column < polarization.cols(); ++column)
    {
        //展开C和C的共轭转置，等价于原Jones矩阵乘法，不创建逐点临时矩阵。
        const auto x = polarization(0, column);
        const auto y = polarization(1, column);
        auto u = (x + imaginary_unit * y) * inverse_sqrt_two;
        auto v = (x - imaginary_unit * y) * inverse_sqrt_two;
        const double power_u_w = std::norm(u);
        const double power_v_w = std::norm(v);
        u *= std::polar(1.0, phase_coefficient * (power_u_w + 2.0 * power_v_w));
        v *= std::polar(1.0, phase_coefficient * (power_v_w + 2.0 * power_u_w));
        polarization(0, column) = (u + v) * inverse_sqrt_two;
        polarization(1, column) = imaginary_unit * (v - u) * inverse_sqrt_two;
    }
}

void SMF::dispersion2(OpticalField::Polarization &polarization, const std::vector<std::complex<double>> &transfer) const
{
    DFT::forward_in_place(polarization);
    for (Eigen::Index column = 0; column < polarization.cols(); ++column)
        polarization.col(column) *= transfer[column];
    DFT::inverse_in_place(polarization);
}

void SMF::update_dispersion_cache(Eigen::Index sample_size, double sample_stride_fs, std::size_t steps) const
{
    if (sample_size <= 0 || !std::isfinite(sample_stride_fs) || sample_stride_fs <= 0.0)
        throw std::invalid_argument("[SMF]色散需要非空采样和有限的正采样步长");
    if (m_cached_sample_size == sample_size && m_cached_sample_stride_fs == sample_stride_fs && m_cached_steps == steps)
        return;

    std::vector<std::complex<double>> full(sample_size), half(sample_size);
    const double beta2_fs2_per_m = m_beta2_ps2_per_km * 1000.0;
    const double step_length_m = m_length_m / static_cast<double>(steps);
    const double angular_frequency_stride = 2.0 * std::numbers::pi_v<double> / static_cast<double>(sample_size) / sample_stride_fs;
    for (Eigen::Index column = 0; column < sample_size; ++column)
    {
        const Eigen::Index frequency_index = column < (sample_size + 1) / 2 ? column : column - sample_size;
        const double omega_rad_per_fs = static_cast<double>(frequency_index) * angular_frequency_stride;
        //H(Ω)=exp(i*beta2*dz*Ω²/2)，半步相位为整步的一半。
        const double phase_rad = 0.5 * beta2_fs2_per_m * step_length_m * omega_rad_per_fs * omega_rad_per_fs;
        if (!std::isfinite(phase_rad))
            throw std::invalid_argument("[SMF]二阶色散相位超出数值范围");
        full[column] = std::polar(1.0, phase_rad);
        half[column] = std::polar(1.0, phase_rad * 0.5);
    }
    //全部成功后更新缓存，避免异常留下不完整的传递函数。
    m_full_step_transfer = std::move(full);
    m_half_step_transfer = std::move(half);
    m_cached_sample_size = sample_size;
    m_cached_sample_stride_fs = sample_stride_fs;
    m_cached_steps = steps;
}
