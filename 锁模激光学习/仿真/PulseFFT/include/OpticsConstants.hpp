#pragma once

namespace pulsefft::constants
{

// 数学常量。
inline constexpr double pi = 3.14159265358979323846;
inline constexpr double two_pi = 2.0 * pi;

// 真空光速。
inline constexpr double speed_of_light_m_per_s = 299792458.0;
inline constexpr double speed_of_light_um_per_fs = 0.299792458;

// 普朗克常量与约化普朗克常量。
inline constexpr double planck_constant_j_s = 6.62607015e-34;
inline constexpr double reduced_planck_constant_j_s = planck_constant_j_s / two_pi;

// 基础电磁学常量。
inline constexpr double vacuum_permittivity_f_per_m = 8.8541878128e-12;
inline constexpr double elementary_charge_c = 1.602176634e-19;

// 常用单位换算系数。
inline constexpr double thz_to_hz = 1e12;
inline constexpr double fs_to_s = 1e-15;
inline constexpr double ps_to_fs = 1e3;
inline constexpr double um_to_m = 1e-6;
inline constexpr double nm_to_m = 1e-9;
inline constexpr double nm_per_um = 1e3;

} // namespace pulsefft::constants
