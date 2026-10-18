#pragma once

#include "OpticalField.hpp"
#include <cstddef>

namespace fourier
{

// 对光场的两个偏振分量分别执行FFT。正变换不归一化，逆变换自动除以采样点数。
void forward_in_place(OpticalField::Polarization& samples);
void inverse_in_place(OpticalField::Polarization& spectrum);

// 返回未经fftshift的FFT第bin_index个频率点，单位THz。
// 这里的频率是相对于光场中心频率的偏移量，不包含约193 THz的光载波。
double bin_frequency_thz(
    std::size_t bin_index,
    std::size_t sample_size,
    double sample_stride_fs);

}
