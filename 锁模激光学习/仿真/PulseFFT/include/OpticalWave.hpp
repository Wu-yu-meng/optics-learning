#pragma once
#include "time/time.hpp"


//光波类
class OpticalWave
{
public:
    OpticalWave(double frequency, double init_phase, double amplitude, double reference_time)
        : m_frequency(frequency),
          m_init_phase(init_phase),
          m_amplitude(amplitude),
          m_reference_time(reference_time)
    {
    }

    // 给定时刻和空间位置计算电场值；position 的单位为 um，返回值为相对电场单位。
    // 实现时应将 now 换算为相对于 m_reference_time 的时间，单位为 fs。
    double amplitude(timetool::Timestamp now,double position);

private:
    double m_frequency;//频率，单位：THz
    double m_init_phase;//在位置 0 um、参考时刻的初始相位，单位：rad
    double m_amplitude;//电场振幅，单位：相对单位（a.u.）
    double m_reference_time;//仿真的参考时间零点，单位：fs
    // 默认光波从位置 0 um 开始定义相位。
    // 暂时不考虑偏振。
};
