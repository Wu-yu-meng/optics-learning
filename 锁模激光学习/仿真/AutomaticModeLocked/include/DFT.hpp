#pragma once

#include "OpticalField.hpp"
#include <complex>
#include <Eigen/Core>

namespace DFT
{
//第一行为x偏振频谱，第二行为y偏振频谱，列数与输入的时域采样点数相同。
using Spectrum = Eigen::Matrix<std::complex<double>, 2, Eigen::Dynamic>;

//正变换采用负指数，不归一化；频点保持原始DFT顺序，不进行fftshift。
Spectrum forward(const OpticalField &optical_field);

//逆变换采用正指数，并除以频谱点数，保证inverse(forward(A))恢复A。
OpticalField::Polarization inverse(const Spectrum &input);

//复用传入的Eigen矩阵存储结果；内部仍需OpenCV工作矩阵，不是零复制。
void forward_in_place(OpticalField::Polarization &polarization);
void inverse_in_place(Spectrum &spectrum);

}//namespace DFT
