#include "VirtualOscilloscope.hpp"
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>
#include <utility>

VirtualOscilloscope::VirtualOscilloscope(
    double responsivity_a_per_w,
    double load_resistance_ohm,
    double bandwidth_ghz,
    double optical_attenuation_db)
    : m_responsivity_a_per_w(responsivity_a_per_w),
      m_load_resistance_ohm(load_resistance_ohm),
      m_bandwidth_ghz(bandwidth_ghz),
      m_optical_attenuation_db(optical_attenuation_db)
{
    if (!std::isfinite(m_responsivity_a_per_w) || m_responsivity_a_per_w <= 0.0)
        throw std::invalid_argument("[VirtualOscilloscope]光电响应度必须大于0");
    if (!std::isfinite(m_load_resistance_ohm) || m_load_resistance_ohm <= 0.0)
        throw std::invalid_argument("[VirtualOscilloscope]负载电阻必须大于0");
    if (!std::isfinite(m_bandwidth_ghz) || m_bandwidth_ghz <= 0.0)
        throw std::invalid_argument("[VirtualOscilloscope]带宽必须大于0");
    if (!std::isfinite(m_optical_attenuation_db) || m_optical_attenuation_db < 0.0)
        throw std::invalid_argument("[VirtualOscilloscope]光衰减不能小于0 dB");
}

VirtualOscilloscope::VoltageSignal VirtualOscilloscope::detect(
    const OpticalField& input_optical_field) const
{
    const auto& polarization = input_optical_field.polarization();
    if (!std::isfinite(input_optical_field.basic_info().sample_stride) ||
        input_optical_field.basic_info().sample_stride <= 0.0)
        throw std::invalid_argument("[VirtualOscilloscope]采样步长必须为有限正数，单位fs");
    if (!polarization.allFinite())
        throw std::invalid_argument("[VirtualOscilloscope]光场含有NaN或无穷大，请检查传播模型");
    const std::size_t sample_size = input_optical_field.basic_info().sample_size;
    std::vector<double> samples_v(sample_size, 0.0);
    if (sample_size == 0)
    {
        return VoltageSignal{
            std::move(samples_v),
            input_optical_field.basic_info().sample_stride,
            input_optical_field.basic_info().start_time,
            m_responsivity_a_per_w,
            m_load_resistance_ohm,
            m_bandwidth_ghz,
            m_optical_attenuation_db};
    }

    //光衰减的dB按功率定义：P_after=P_before*10^(-attenuation_db/10)。
    const double optical_power_ratio = std::pow(
        10.0,
        -m_optical_attenuation_db / 10.0);

    //用一阶低通模拟光电探测器和示波器的综合有限带宽：
    //tau=1/(2*pi*B)，y[n]=y[n-1]+alpha*(x[n]-y[n-1])。
    const double sample_stride_s =
        input_optical_field.basic_info().sample_stride * 1.0e-15;
    const double bandwidth_hz = m_bandwidth_ghz * 1.0e9;
    const double time_constant_s =
        1.0 / (2.0 * std::numbers::pi_v<double> * bandwidth_hz);
    const double alpha = -std::expm1(-sample_stride_s / time_constant_s);

    const auto attenuated_power_w = [&](std::size_t index)
    {
        const Eigen::Index column = static_cast<Eigen::Index>(index);
        return optical_power_ratio *
            (std::norm(polarization(0, column)) +
             std::norm(polarization(1, column)));
    };

    //用窗口左端的功率作为滤波器初值。当左端不接近0时，边界检测会另外发出警告。
    double filtered_power_w = attenuated_power_w(0);
    for (std::size_t i = 0; i < sample_size; ++i)
    {
        const double input_power_w = attenuated_power_w(i);
        if (!std::isfinite(input_power_w))
            throw std::invalid_argument("[VirtualOscilloscope]光功率超出数值范围，请检查传播模型");
        filtered_power_w += alpha * (input_power_w - filtered_power_w);

        //I=responsivity*P，V=I*R_load。
        samples_v[i] =
            m_responsivity_a_per_w *
            m_load_resistance_ohm *
            filtered_power_w;
    }

    return VoltageSignal{
        std::move(samples_v),
        input_optical_field.basic_info().sample_stride,
        input_optical_field.basic_info().start_time,
        m_responsivity_a_per_w,
        m_load_resistance_ohm,
        m_bandwidth_ghz,
        m_optical_attenuation_db};
}
