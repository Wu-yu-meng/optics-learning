#pragma once
#include "OpticalElement.hpp"
#include <cstdint>
#include <random>
//EDF只考虑其增益效果，不考虑其作为光纤的Kerr效应和二阶色散
//采用有限增益带宽的饱和增益模型，保留带限噪声

class EDF : public OpticalElement
{
public:
    //第四个参数为增益带宽THz；噪声能量单位pJ/次传播，设为0可关闭；末尾为独立的噪声带宽THz。
    EDF(double g_0, double E_sat, double length, double gain_bandwidth_thz = 10.0,
        double ase_noise_energy_pj_per_pass = 0.0, std::uint64_t random_seed = 20261018, double noise_bandwidth_thz = 10.0);
    OpticalOutputs propagate(const OpticalInputs& input_optical_fields) override;//传播时会推进随机数生成器状态
    void reset_noise();//恢复初始随机种子，便于重复实验

private:
    double m_g_0;//小信号增益m^-1
    double m_E_sat;//饱和脉冲能量pJ
    double m_length;//长度m
    double m_gain_bandwidth_thz;//增益系数谱的高斯半高全宽THz
    double m_noise_bandwidth_thz;//噪声功率谱的高斯半高全宽THz，不影响信号增益
    double m_ase_noise_energy_pj_per_pass;//每次传播加入的双偏振噪声总能量pJ
    std::uint64_t m_random_seed;//初始随机种子
    std::mt19937_64 m_random_engine;//持续生成噪声，每次传播使用新的随机样本
};
