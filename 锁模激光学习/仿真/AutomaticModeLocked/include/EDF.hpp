#pragma once
#include "OpticalField.hpp"
#include <cstdint>
#include <random>
//EDF只考虑其增益效果，不考虑其作为光纤的Kerr效应和二阶色散
//采用饱和增益模型，增益系数随频偏呈高斯分布

class EDF
{
public:
    //单位依次为m^-1、pJ、m、THz、pJ/次传播，最后一个参数为随机种子；噪声能量为0时关闭噪声。
    EDF(double g_0, double E_sat, double length, double gain_bandwidth_thz = 4.0,
        double ase_noise_energy_pj_per_pass = 0.0, std::uint64_t random_seed = 20261018);
    OpticalField propagate(const OpticalField& input_optical_field);//噪声会推进随机数生成器的状态，因此不再是const
    void reset_noise();//将随机数生成器恢复到初始种子，便于重复实验

private:
    double m_g_0;//小信号增益m^-1
    double m_E_sat;//饱和脉冲能量pJ
    double m_length;//长度m
    double m_gain_bandwidth_thz;//增益系数谱g(f)的高斯半高全宽THz，不是振幅增益的半高全宽
    double m_ase_noise_energy_pj_per_pass;//每次传播后加入的双偏振噪声总能量pJ
    std::uint64_t m_random_seed;//随机数种子
    std::mt19937_64 m_random_engine;//噪声随机数生成器
};
