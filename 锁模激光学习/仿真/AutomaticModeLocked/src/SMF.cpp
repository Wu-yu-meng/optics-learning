#include "SMF.hpp"
#include "FourierTransform.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>
#include <utility>

SMF::SMF(
    double gamma_per_w_m,
    double beta_2_ps2_per_km,
    double length_m,
    double max_nonlinear_phase_per_step_rad,
    std::size_t max_split_step_count)
    : m_gamma_per_w_m(gamma_per_w_m),
      m_beta_2_ps2_per_km(beta_2_ps2_per_km),
      m_length_m(length_m),
      m_max_nonlinear_phase_per_step_rad(max_nonlinear_phase_per_step_rad),
      m_max_split_step_count(max_split_step_count)
{
    if (m_gamma_per_w_m < 0.0)
    {
        throw std::invalid_argument("[SMF]非线性系数不能小于0");
    }

    if (m_length_m < 0.0)
    {
        throw std::invalid_argument("[SMF]光纤长度不能小于0");
    }

    if (m_max_nonlinear_phase_per_step_rad <= 0.0)
    {
        throw std::invalid_argument("[SMF]单步最大非线性相移必须大于0");
    }

    if (m_max_split_step_count == 0)
    {
        throw std::invalid_argument("[SMF]最大分步数不能为0");
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
    //对称分步：半步色散 -> 一步Kerr -> 半步色散。
    //色散和Kerr不可交换，把5 m光纤分成多段可以降低一次性近似带来的误差。
    OpticalField::Polarization output_polarization = input_optical_field.polarization();
    if (m_length_m == 0.0)
        return OpticalField{std::move(output_polarization), input_optical_field.basic_info()};

    //根据当前峰值功率自动选择分步数。在UV模型中，任一分量的非线性相移上界为
    //(4/3)*gamma*L*P_total。低功率时用1步即可，只有强非线性时才增加FFT次数。
    double peak_power_w = 0.0;
    for (Eigen::Index column = 0; column < output_polarization.cols(); ++column)
    {
        const double power_w =
            std::norm(output_polarization(0, column)) +
            std::norm(output_polarization(1, column));
        peak_power_w = std::max(peak_power_w, power_w);
    }

    const double estimated_max_nonlinear_phase_rad =
        (4.0 / 3.0) * m_gamma_per_w_m * m_length_m * peak_power_w;
    const std::size_t required_step_count = std::max<std::size_t>(
        1,
        static_cast<std::size_t>(std::ceil(
            estimated_max_nonlinear_phase_rad /
            m_max_nonlinear_phase_per_step_rad)));
    const std::size_t split_step_count =
        std::min(required_step_count, m_max_split_step_count);

    const double step_length_m = m_length_m / static_cast<double>(split_step_count);
    update_dispersion_transfer_cache(
        input_optical_field.basic_info().sample_size,
        input_optical_field.basic_info().sample_stride,
        split_step_count);
    apply_second_order_dispersion(
        output_polarization,
        m_cached_half_step_dispersion);

    for (std::size_t step = 0; step < split_step_count; ++step)
    {
        apply_kerr(output_polarization, step_length_m);

        //相邻两段的半步色散可以合并成一个整步，数学上与D/2-N-D/2完全等价，
        //但每次传播只需要split_step_count+1次色散操作。
        apply_second_order_dispersion(
            output_polarization,
            step + 1 == split_step_count
                ? m_cached_half_step_dispersion
                : m_cached_full_step_dispersion);
    }

    return OpticalField{std::move(output_polarization),input_optical_field.basic_info()};
}

void SMF::update_dispersion_transfer_cache(
    std::size_t sample_size,
    double sample_stride_fs,
    std::size_t split_step_count) const
{
    if (m_cached_sample_size == sample_size &&
        m_cached_sample_stride_fs == sample_stride_fs &&
        m_cached_split_step_count == split_step_count)
    {
        return;
    }

    m_cached_sample_size = sample_size;
    m_cached_sample_stride_fs = sample_stride_fs;
    m_cached_split_step_count = split_step_count;
    m_cached_half_step_dispersion.assign(sample_size, {1.0, 0.0});
    m_cached_full_step_dispersion.assign(sample_size, {1.0, 0.0});

    if (m_beta_2_ps2_per_km == 0.0)
        return;

    // 1 ps^2/km = 1000 fs^2/m。全部换成fs和m后，可以直接与rad/fs的角频率相乘。
    const double beta_2_fs2_per_m = m_beta_2_ps2_per_km * 1000.0;
    const double full_step_length_m =
        m_length_m / static_cast<double>(split_step_count);

    for (std::size_t i = 0; i < sample_size; ++i)
    {
        const double frequency_thz = fourier::bin_frequency_thz(
            i,
            sample_size,
            sample_stride_fs);
        const double omega_rad_per_fs =
            2.0 * std::numbers::pi_v<double> * frequency_thz * 1.0e-3;

        //采用A(t)<->A~(Omega)的常见约定，二阶色散传递函数为
        //H(Omega)=exp[i*beta2*L*Omega^2/2]。
        const double full_step_phase_rad =
            0.5 * beta_2_fs2_per_m * full_step_length_m *
            omega_rad_per_fs * omega_rad_per_fs;
        m_cached_full_step_dispersion[i] =
            std::polar(1.0, full_step_phase_rad);
        m_cached_half_step_dispersion[i] =
            std::polar(1.0, 0.5 * full_step_phase_rad);
    }
}

void SMF::apply_second_order_dispersion(
    OpticalField::Polarization& polarization,
    const std::vector<std::complex<double>>& transfer) const
{
    if (m_beta_2_ps2_per_km == 0.0)
        return;

    fourier::forward_in_place(polarization);

    for (std::size_t i = 0; i < transfer.size(); ++i)
    {
        polarization(0, static_cast<Eigen::Index>(i)) *= transfer[i];
        polarization(1, static_cast<Eigen::Index>(i)) *= transfer[i];
    }

    fourier::inverse_in_place(polarization);
}

void SMF::apply_kerr(
    OpticalField::Polarization& polarization,
    double propagation_length_m) const
{
    const std::complex<double> imaginary_unit{0.0, 1.0};
    const double inverse_sqrt_two = 1.0 / std::sqrt(2.0);
    const double nonlinear_coefficient =
        2.0 * m_gamma_per_w_m * propagation_length_m / 3.0;

    for (Eigen::Index column = 0; column < polarization.cols(); ++column)
    {
        //与原Jones矩阵完全等价，但避免在每个时间采样点构造2x2临时矩阵。
        const std::complex<double> x = polarization(0, column);
        const std::complex<double> y = polarization(1, column);
        std::complex<double> u =
            (x + imaginary_unit * y) * inverse_sqrt_two;
        std::complex<double> v =
            (x - imaginary_unit * y) * inverse_sqrt_two;

        //计算kerr效应带来的相位延迟
        //φu = (2γL/3)(Pu + 2Pv)
        //φv = (2γL/3)(Pv + 2Pu)
        const double power_u_w = std::norm(u);
        const double power_v_w = std::norm(v);
        const double phase_u_rad =
            nonlinear_coefficient * (power_u_w + 2.0 * power_v_w);
        const double phase_v_rad =
            nonlinear_coefficient * (power_v_w + 2.0 * power_u_w);

        u *= std::polar(1.0, phase_u_rad);
        v *= std::polar(1.0, phase_v_rad);

        //U/V -> X/Y：C^{-1}=C^†。
        polarization(0, column) = (u + v) * inverse_sqrt_two;
        polarization(1, column) =
            imaginary_unit * (v - u) * inverse_sqrt_two;
    }
}
