#include "FourierTransform.hpp"
#include <opencv2/core.hpp>
#include <stdexcept>

namespace
{

void transform_in_place(OpticalField::Polarization& values, bool inverse)
{
    if (values.cols() == 0)
        return;

    cv::Mat cv_values(
        static_cast<int>(values.rows()),
        static_cast<int>(values.cols()),
        CV_64FC2);

    for (Eigen::Index row = 0; row < values.rows(); ++row)
    {
        for (Eigen::Index column = 0; column < values.cols(); ++column)
        {
            const std::complex<double> value = values(row, column);
            cv_values.at<cv::Vec2d>(
                static_cast<int>(row),
                static_cast<int>(column)) = {value.real(), value.imag()};
        }
    }

    int flags = cv::DFT_ROWS;
    if (inverse)
        flags |= cv::DFT_INVERSE | cv::DFT_SCALE;
    cv::dft(cv_values, cv_values, flags);

    for (Eigen::Index row = 0; row < values.rows(); ++row)
    {
        for (Eigen::Index column = 0; column < values.cols(); ++column)
        {
            const cv::Vec2d value = cv_values.at<cv::Vec2d>(
                static_cast<int>(row),
                static_cast<int>(column));
            values(row, column) = {value[0], value[1]};
        }
    }
}

}

namespace fourier
{

void forward_in_place(OpticalField::Polarization& samples)
{
    transform_in_place(samples, false);
}

void inverse_in_place(OpticalField::Polarization& spectrum)
{
    transform_in_place(spectrum, true);
}

double bin_frequency_thz(
    std::size_t bin_index,
    std::size_t sample_size,
    double sample_stride_fs)
{
    if (sample_size == 0)
        throw std::invalid_argument("[FFT]采样点数不能为0");

    if (sample_stride_fs <= 0.0)
        throw std::invalid_argument("[FFT]采样步长必须大于0");

    const long long sample_size_signed = static_cast<long long>(sample_size);
    const long long index_signed = static_cast<long long>(bin_index);
    const long long signed_bin =
        index_signed <= sample_size_signed / 2
            ? index_signed
            : index_signed - sample_size_signed;

    // signed_bin/(N*dt_fs)的单位是fs^-1，1 fs^-1 = 1000 THz。
    return static_cast<double>(signed_bin) * 1000.0 /
           (static_cast<double>(sample_size) * sample_stride_fs);
}

}
