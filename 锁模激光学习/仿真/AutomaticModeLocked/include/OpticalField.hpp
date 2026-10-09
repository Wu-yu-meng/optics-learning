#pragma once
#include <cstddef>
#include <complex>
#include <Eigen/Core>

//光场类
class OpticalField
{
public:
    //使用矩阵表示xy方向的偏振采样，第一行为x,第二行为y
    using Polarization = Eigen::Matrix<std::complex<double>,2,Eigen::Dynamic>;
    
    struct BasicInfo
    {
        double center_frequency;//中心频率THz
        double wave_length;//波长nm
        double sample_stride;//采样的步长fs
        size_t sample_size;//采样点数目
        double start_time;//第一个采样值对应的时刻fs，由采样数和采样步长计算
    };

    OpticalField(Polarization polarization, BasicInfo basic_info);//构造函数
    const Polarization& polarization() const;//返回偏振采样队列
    const BasicInfo& basic_info() const;//返回光场的基本参数
private:
    Polarization m_polarization;//偏振采样队列
    BasicInfo m_basic_info;//光场的基本参数

};


OpticalField init_gauss_optical_field();//创建一个高斯光场
