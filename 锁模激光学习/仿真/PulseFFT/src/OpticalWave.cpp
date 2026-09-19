#include "OpticalWave.hpp"
#include "TimeMapping.hpp"
#include "OpticsConstants.hpp"
#include <cmath>

double OpticalWave::amplitude(timetool::Timestamp now, double position) const
{
    //默认传播方向都是为正
    //相位的计算公式:phase = m_init_phase + w(x/c - (t - t0))
    double time_interval = pulsefft::map2simulation_time_fs(now,m_reference_time);
    double phase = m_init_phase + 2 * 1e-3 * pulsefft::constants::pi * m_frequency * (position / pulsefft::constants::speed_of_light_um_per_fs  - time_interval);

    //计算幅值:A * cos(phase)
    double amplitude = m_amplitude * std::cos(phase);
    return amplitude;
}

double OpticalWave::amplitude(double phase) const
{
    return m_amplitude * std::cos(phase);
}

double OpticalWave::phase(timetool::Timestamp now, double position) const
{
    //默认传播方向都是为正
    //相位的计算公式:phase = m_init_phase + w(x/c - (t - t0))
    double time_interval = pulsefft::map2simulation_time_fs(now,m_reference_time);
    double phase = m_init_phase + 2 * 1e-3 * pulsefft::constants::pi * m_frequency * (position / pulsefft::constants::speed_of_light_um_per_fs  - time_interval);
    return phase;
}

double Pluse::amplitude(timetool::Timestamp now, double position) const
{
    //计算公式exp(i * w_0 * t) * envelope(time,position)
    
    //计算时间
    double time_fs = pulsefft::map2simulation_time_fs(now,m_reference_time);

    //计算角频率
    double angular_frequency_rad_per_fs = 2.0
        * pulsefft::constants::pi
        * 1e-3
        * m_carrier_frequency;
    
    //计算相位
    double carrier_phase_rad = m_init_phase + angular_frequency_rad_per_fs * (position / pulsefft::constants::speed_of_light_um_per_fs - time_fs);

    //计算载波
    std::complex<double> carrier =std::polar(1.0, carrier_phase_rad);

    //计算电场：最大振幅 * 载波 * 归一化包络
    std::complex<double> E_field = m_amplitude * carrier * envelope(now,position);

    //返回电场
    return std::real(E_field);

}

std::complex<double> GaussianPluse::envelope(timetool::Timestamp now, double position) const
{
    //计算时间
    double time_fs = pulsefft::map2simulation_time_fs(now,m_reference_time);

    //计算距离带来的时间
    double position_retarded_time = position / pulsefft::constants::speed_of_light_um_per_fs;

    //计算真正代入计算的时间
    time_fs -= position_retarded_time;

    //高斯电场包络计算；m_pulse_width_fs 表示光强的半峰全宽
    double normalized_time = time_fs / m_pulse_width_fs;
    double envelope_amplitude = std::exp(
        -2.0 * std::log(2.0) * normalized_time * normalized_time);
    return std::complex<double>(envelope_amplitude,0.0);//高斯包络是没有额外的相位的
}
