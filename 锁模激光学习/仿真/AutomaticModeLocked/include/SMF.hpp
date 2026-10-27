#pragma once
#include "OpticalField.hpp"
#include <vector>

//考虑Kerr非线性、二阶色散
//采用自适应对称分步传播，每段为半步色散->整步Kerr->半步色散
class SMF
{
public:
    SMF(double gamma_per_w_m, double length_m, double beta2_ps2_per_km);//单位依次为W^-1*m^-1、m、ps^2/km
    OpticalField propagate(const OpticalField& input_optical_field) const;//SMF传播函数
private:
    void kerr(OpticalField::Polarization &polarization, double length_m) const;//等价的UV标量计算，原地写回
    void dispersion2(OpticalField::Polarization &polarization, const std::vector<std::complex<double>> &transfer) const;
    void update_dispersion_cache(Eigen::Index sample_size, double sample_stride_fs, std::size_t steps) const;
    double m_gamma_per_w_m;//非线性系数、W^-1*m^-1
    double m_length_m;//光纤长度、m
    double m_beta2_ps2_per_km;//二阶色散系数、ps^2/km，可正可负
    //采样网格或分段数变化才重建；同一个实例不能并发调用propagate。
    mutable Eigen::Index m_cached_sample_size = 0;
    mutable double m_cached_sample_stride_fs = 0.0;
    mutable std::size_t m_cached_steps = 0;
    mutable std::vector<std::complex<double>> m_half_step_transfer;
    mutable std::vector<std::complex<double>> m_full_step_transfer;
};
