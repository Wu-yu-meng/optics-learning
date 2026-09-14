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
