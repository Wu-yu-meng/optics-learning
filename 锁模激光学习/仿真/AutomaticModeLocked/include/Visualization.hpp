#pragma once
#include "OpticalField.hpp"
#include <atomic>
#include <string>
#include<opencv2/opencv.hpp>


//虚拟光功率器（虚拟器件）
//直接获取原始的光功率
class VirtualOpticalPowerVisualizer
{
public:
    explicit VirtualOpticalPowerVisualizer(const std::string& name);//保存“虚拟光器”加传入的名称。
    //直接返回电场的平方
    cv::Mat measure(size_t loop_count,const OpticalField &optical_field);
    Eigen::Matrix<double, 3, Eigen::Dynamic> measure(const OpticalField &optical_field);
    void auto_size();//自适应画幅
    const std::string& name() const;//获取仪器名称，用作窗口名称。

private:
    std::string m_name;//仪器名称。
    std::atomic<bool> m_auto_size = true;//是否自适应画幅
    double m_y_axis_max_w;//y轴
};

//示波器
class Oscilloscope
{
public:

    //返回光->电信信号结果
    cv::Mat measure();
};

//光谱仪
class OpticalSpectrumAnalyzer
{
    //返回DFT的结果？
    cv::Mat measure();
};




//可视化管理器，负责调用可视化
//分为两个部分，一个是仿真仪器的可视化，一个是虚拟仪器的可视化
class Visualization
{
public:
    //进行所有的可视化更新。需要如下的数据
    //光场数据
    void update(size_t loop_count,const OpticalField &optical_field);
private:

};
