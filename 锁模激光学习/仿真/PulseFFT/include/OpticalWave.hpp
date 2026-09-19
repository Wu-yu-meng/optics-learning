#pragma once
#include "time/time.hpp"
#include <complex>

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
    double m_init_phase;//初始化相位
    double m_amplitude;//电场最大振幅，单位：相对单位（a.u.）
    timetool::Timestamp m_reference_time;//仿真的参考时间零点，单位：fs
    // 默认光波从位置 0 um 开始定义相位。
    // 暂时不考虑偏振。
};


//脉冲类(使用载波和包络来表示)
class Pluse
{
public:

Pluse(double carrier_frequency, double init_phase, double amplitude, timetool::Timestamp reference_time)
    : m_init_phase(init_phase),
      m_reference_time(reference_time),
      m_carrier_frequency(carrier_frequency),
      m_amplitude(amplitude)
{
}

    double amplitude(timetool::Timestamp now,double position) const;//返回电场强度

protected:
    double m_init_phase;                                                                       // 初始化相位
    timetool::Timestamp m_reference_time;                                                      // 仿真的参考时间零点，单位：fs
    double m_carrier_frequency;                                                                // 载波的频率（单位为thz）
    double m_amplitude;                                                                        // 电场最大振幅，单位：相对单位（a.u.）
    virtual std::complex<double> envelope(timetool::Timestamp now, double position) const = 0; // 返回包络
};

class GaussianPluse : public Pluse
{
public:
    GaussianPluse(double carrier_frequency, double init_phase, double amplitude, double pulse_width_fs, timetool::Timestamp reference_time)
        : Pluse(carrier_frequency, init_phase, amplitude, reference_time),
          m_pulse_width_fs(pulse_width_fs)
    {
    }

private:
    double m_pulse_width_fs;// 光强包络的半峰全宽（FWHM），单位：fs
    std::complex<double> envelope(timetool::Timestamp now,double position) const override;
};
