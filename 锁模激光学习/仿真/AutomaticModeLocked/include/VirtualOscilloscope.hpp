#pragma once

#include "OpticalField.hpp"
#include <vector>

//虚拟示波器的观测链路：OC输出光功率->光衰减->光电转换与有限带宽->电压采样。
//仅用于观察，不把探测结果反馈到腔内光场。
class VirtualOscilloscope
{
public:
    struct VoltageSignal
    {
        std::vector<double> samples_v;//电压采样，单位V
        double sample_stride_fs;//采样间隔fs
        double start_time_fs;//第一个采样点时刻fs
        double responsivity_a_per_w;//光电响应度A/W
        double load_resistance_ohm;//示波器输入负载Ohm
        double bandwidth_ghz;//光电探测链路的3 dB带宽GHz
        double optical_attenuation_db;//进入探测器前的光功率衰减dB
    };

    VirtualOscilloscope(
        double responsivity_a_per_w,
        double load_resistance_ohm,
        double bandwidth_ghz,
        double optical_attenuation_db);

    VoltageSignal detect(const OpticalField& input_optical_field) const;

private:
    double m_responsivity_a_per_w;
    double m_load_resistance_ohm;
    double m_bandwidth_ghz;
    double m_optical_attenuation_db;
};
