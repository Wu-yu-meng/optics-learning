#pragma once
#include "time/time.hpp"


//光波类
class OpticalWave
{
public:
    OpticalWave(double frequency, double init_phase, double amplitude, timetool::Timestamp reference_time)
        : m_frequency(frequency),
          m_init_phase(init_phase),
          m_amplitude(amplitude),
          m_reference_time(reference_time)
    {
    }

    // 给定时刻和空间位置计算电场值；position 的单位为 um，返回值为相对电场单位。
    // 实现时应将 now 换算为相对于 m_reference_time 的时间，单位为 fs。
    double amplitude(timetool::Timestamp now,double position) const;

    //给定相位直接计算电场值
    double amplitude(double phase) const;
    
    // 给定时刻和空间位置计算相位；position 的单位为 um，返回值为未归一话的相位。
    // 实现时应将 now 换算为相对于 m_reference_time 的时间，单位为 fs。
    double phase(timetool::Timestamp now,double position) const;

private:
    double m_frequency;//频率，单位：THz
    double m_init_phase;//初始化时间戳
    double m_amplitude;//电场最大振幅，单位：相对单位（a.u.）
    timetool::Timestamp m_reference_time;//仿真的参考时间零点，单位：fs
    // 默认光波从位置 0 um 开始定义相位。
    // 暂时不考虑偏振。
};
