#include "DFT.hpp"
#include <opencv2/core.hpp>
#include <limits>
#include <stdexcept>

namespace DFT
{
namespace
{
void transform_in_place(Spectrum &values, bool inverse)
{
    if (values.cols() == 0 || values.cols() > std::numeric_limits<int>::max())
        throw std::invalid_argument("[DFT]采样点数必须大于0且不能超过OpenCV支持的范围");
    const int sample_size = static_cast<int>(values.cols());
    cv::Mat data(2, sample_size, CV_64FC2);
    //Eigen按列存储，OpenCV按行存储，仍需复制到工作矩阵。
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < sample_size; ++column)
        {
            const auto value = values(row, column);
            data.at<cv::Vec2d>(row, column) = {value.real(), value.imag()};
        }
    //逆变换除以N；保持点数和原始频点顺序不变。
    const int flags = cv::DFT_ROWS | (inverse ? cv::DFT_INVERSE | cv::DFT_SCALE : 0);
    cv::dft(data, data, flags);
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < sample_size; ++column)
        {
            const auto value = data.at<cv::Vec2d>(row, column);
            values(row, column) = {value[0], value[1]};
        }
}
}
void forward_in_place(OpticalField::Polarization &polarization)
{
    transform_in_place(polarization, false);
}
void inverse_in_place(Spectrum &spectrum)
{
    transform_in_place(spectrum, true);
}
Spectrum forward(const OpticalField &optical_field)
{
    Spectrum spectrum = optical_field.polarization();
    forward_in_place(spectrum);
    return spectrum;
}
OpticalField::Polarization inverse(const Spectrum &input)
{
    OpticalField::Polarization polarization = input;
    inverse_in_place(polarization);
    return polarization;
}
}//namespace DFT
