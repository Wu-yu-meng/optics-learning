#pragma once
#include "OpticalField.hpp"
#include <cstddef>
#include <complex>
#include <vector>

// SMF使用对称分步傅里叶法联合计算二阶色散与Kerr非线性。
class SMF
{
public:
    // 单位依次为W^-1*m^-1、ps^2/km、m。
    SMF(
        double gamma_per_w_m,
        double beta_2_ps2_per_km,
        double length_m,
        double max_nonlinear_phase_per_step_rad,
        std::size_t max_split_step_count);
    OpticalField propagate(const OpticalField& input_optical_field) const;//SMF传播函数
private:
    void update_dispersion_transfer_cache(
        std::size_t sample_size,
        double sample_stride_fs,
        std::size_t split_step_count) const;
    void apply_second_order_dispersion(
        OpticalField::Polarization& polarization,
        const std::vector<std::complex<double>>& transfer) const;
    void apply_kerr(
        OpticalField::Polarization& polarization,
        double propagation_length_m) const;

    Eigen::Matrix2cd m_xy_to_uv_matrix;//将XY偏振分量转换为UV圆偏振分量
    double m_gamma_per_w_m;//非线性系数、W^-1*m^-1
    double m_beta_2_ps2_per_km;//二阶色散系数、ps^2/km
    double m_length_m;//光纤长度、m
    double m_max_nonlinear_phase_per_step_rad;//每个分步允许的最大Kerr非线性相移rad
    std::size_t m_max_split_step_count;//自适应分步数上限

    //稳定运行后分步数通常不变，缓存色散传递函数可避免每圈重复计算大量复指数。
    mutable std::size_t m_cached_sample_size = 0;
    mutable std::size_t m_cached_split_step_count = 0;
    mutable double m_cached_sample_stride_fs = 0.0;
    mutable std::vector<std::complex<double>> m_cached_half_step_dispersion;
    mutable std::vector<std::complex<double>> m_cached_full_step_dispersion;
};
