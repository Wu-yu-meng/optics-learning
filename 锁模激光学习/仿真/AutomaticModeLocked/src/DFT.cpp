#include "DFT.hpp"
#include <opencv2/core.hpp>
#include <limits>
#include <stdexcept>

namespace DFT
{

Spectrum forward(const OpticalField &optical_field)
{
    const auto &polarization = optical_field.polarization();
    if (polarization.cols() == 0)
    {
        throw std::invalid_argument("[DFT]正变换的采样点数必须大于0");
    }
    if (polarization.cols() > std::numeric_limits<int>::max())
    {
        throw std::invalid_argument("[DFT]采样点数超过OpenCV支持的范围");
    }

    const int sample_size = static_cast<int>(polarization.cols());
    cv::Mat data(2, sample_size, CV_64FC2);

    //将Eigen的两行复包络复制为OpenCV复数矩阵，每个元素保存实部和虚部。
    for (int row = 0; row < 2; ++row)
    {
        for (int i = 0; i < sample_size; ++i)
        {
            const auto value = polarization(row, i);
            data.at<cv::Vec2d>(row, i) = cv::Vec2d(value.real(), value.imag());
        }
    }

    //分别对x、y两行进行一维正变换
    cv::dft(data, data, cv::DFT_ROWS);

    //赋值
    Spectrum spectrum(2, sample_size);
    for (int row = 0; row < 2; ++row)
    {
        for (int i = 0; i < sample_size; ++i)
        {
            const auto value = data.at<cv::Vec2d>(row, i);
            spectrum(row, i) = std::complex<double>(value[0], value[1]);
        }
    }

    return spectrum;
}

OpticalField::Polarization inverse(const Spectrum &input)
{
    if (input.cols() == 0)
    {
        throw std::invalid_argument("[DFT]逆变换的频谱点数必须大于0");
    }
    if (input.cols() > std::numeric_limits<int>::max())
    {
        throw std::invalid_argument("[DFT]频谱点数超过OpenCV支持的范围");
    }
    //保持输入点数不变，完整频谱都参与逆变换。
    const int spectrum_size = static_cast<int>(input.cols());
    cv::Mat data(2, spectrum_size, CV_64FC2);
    for (int row = 0; row < 2; ++row)
    {
        for (int i = 0; i < spectrum_size; ++i)
        {
            const auto value = input(row, i);
            data.at<cv::Vec2d>(row, i) = cv::Vec2d(value.real(), value.imag());
        }
    }

    //DFT_SCALE按每行的完整频谱点数归一化，恢复原来的复包络振幅。
    cv::dft(data, data, cv::DFT_ROWS | cv::DFT_INVERSE | cv::DFT_SCALE);

    //赋值
    OpticalField::Polarization polarization(2, spectrum_size);
    for (int row = 0; row < 2; ++row)
    {
        for (int i = 0; i < spectrum_size; ++i)
        {
            const auto value = data.at<cv::Vec2d>(row, i);
            polarization(row, i) = std::complex<double>(value[0], value[1]);
        }
    }

    return polarization;
}

}//namespace DFT
