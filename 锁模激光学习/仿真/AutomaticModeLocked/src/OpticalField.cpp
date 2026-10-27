#include "OpticalField.hpp"
#include "config/config.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

OpticalField::OpticalField(Polarization polarization, BasicInfo basic_info)
    : m_polarization(std::move(polarization)),
      m_basic_info(basic_info)
{
    //采样数由实际的光场矩阵决定，避免基本信息与矩阵不一致。
    m_basic_info.sample_size = static_cast<std::size_t>(m_polarization.cols());
}

const OpticalField::Polarization &OpticalField::polarization() const
{
    return m_polarization;
}

const OpticalField::BasicInfo &OpticalField::basic_info() const
{
    return m_basic_info;
}

OpticalField init_gauss_optical_field()
{
    //更新配置文件
    CONFIG.updateJson();

    //读取科学常数和高斯初始光场的配置。
    const cv::FileNode scientific_constants = CONFIG.config_["scientific_constants"];
    const cv::FileNode gaussian_config = CONFIG.config_["init_gauss_optical_field"];

    //生成高斯光场所需的参数配置
    const double light_speed_m_per_s = static_cast<double>(scientific_constants["light_speed"]);//光速m/s
    const double wave_length_nm = static_cast<double>(gaussian_config["wave_length_nm"]);//波长nm
    const double sample_stride_fs = static_cast<double>(gaussian_config["sample_stride_fs"]);//采样步长fs
    const std::size_t sample_size = static_cast<std::size_t>(static_cast<int>(gaussian_config["sample_size"]));//采样数目
    const double fwhm_time_fs = static_cast<double>(gaussian_config["fwhm_time_fs"]);//fwhm的时间跨度
    const double peak_power_w = static_cast<double>(gaussian_config["peak_power_w"]);//峰值功率W
    const double polarization_angle_rad = static_cast<double>(gaussian_config["polarization_angle_deg"]) * std::numbers::pi_v<double> / 180.0;//x、y偏振分量的振幅分配角
    const double relative_phase_rad = static_cast<double>(gaussian_config["relative_phase_deg"]) * std::numbers::pi_v<double> / 180.0;//y分量相对x分量的相位差
    const double start_time_fs = -static_cast<double>(sample_size / 2) * sample_stride_fs;//第一个采样点的时间（时间窗口以t=0为中心：对于4096点、1 fs步长，起点为-2048 fs）。
    const double center_frequency_thz = light_speed_m_per_s / wave_length_nm * 1.0e-3;//中心频率f[THz] = c[m/s] / lambda[nm] * 10^-3。

    
    //计算偏振队列
    OpticalField::Polarization polarization(2, static_cast<Eigen::Index>(sample_size));
    const double x_projection = std::cos(polarization_angle_rad);//x分量投影系数
    const double y_projection = std::sin(polarization_angle_rad);//y分量投影系数
    const std::complex<double> y_relative_phase = std::polar(1.0, relative_phase_rad);//y与x的相位差
    const double peak_amplitude_sqrt_w = std::sqrt(peak_power_w);//|A(t)|^2=P(t)，复包络振幅单位为sqrt(W)

    for (std::size_t i = 0; i < sample_size; i++)
    {
        //计算采样时刻
        const double time_fs = start_time_fs + static_cast<double>(i) * sample_stride_fs;

        //A(t)=A0*exp[-2*ln(2)*t^2/tau^2]，tau是光强的半高全宽。
        //计算采样时刻的包络值
        const double envelope_sqrt_w = peak_amplitude_sqrt_w * std::exp(-2.0 * std::log(2.0) * time_fs * time_fs / (fwhm_time_fs * fwhm_time_fs));

        //包络值分解到两个方向，同时以x分量为0基准，为y方向添加相位。
        const Eigen::Index column = static_cast<Eigen::Index>(i);
        polarization(0, column) = envelope_sqrt_w * x_projection;
        polarization(1, column) = envelope_sqrt_w * y_projection * y_relative_phase;
    }

    //光场的基础信息
    OpticalField::BasicInfo basic_info{
        .center_frequency = center_frequency_thz,
        .wave_length = wave_length_nm,
        .sample_stride = sample_stride_fs,
        .sample_size = sample_size,
        .start_time = start_time_fs
    };

    return OpticalField(std::move(polarization), basic_info);
}
