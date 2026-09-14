#pragma once

#include "time/time.hpp"

namespace pulsefft
{

// 将真实程序的经过时间映射为仿真时间。
// 默认映射：真实经过 1 us，对应仿真经过 1e-5 fs。
// 对当前 375 THz 的载波，该倍率约为每真实秒播放 3.75 个光学周期。
// 该映射仅用于控制可视化播放进度，不改变光波公式中的物理单位。
inline constexpr double default_simulation_fs_per_real_us = 1e-4;

inline double map2simulation_time_fs(
    timetool::Timestamp now,
    timetool::Timestamp reference_time,
    double simulation_fs_per_real_us = default_simulation_fs_per_real_us)
{
    const double elapsed_real_time_us =timetool::minus<std::chrono::microseconds>(now, reference_time);
    return elapsed_real_time_us * simulation_fs_per_real_us;
}

} // namespace pulsefft
