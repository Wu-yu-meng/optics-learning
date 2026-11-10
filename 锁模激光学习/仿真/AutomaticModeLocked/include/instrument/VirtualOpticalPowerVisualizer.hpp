#pragma once
#include "communication/OpticalField.hpp"
#include "instrument/Instrument.hpp"
#include <atomic>
#include <string>
#include<opencv2/opencv.hpp>


//虚拟光功率器（虚拟器件）
//直接获取原始的光功率
class VirtualOpticalPowerVisualizer : public Instrument
{
public:
    explicit VirtualOpticalPowerVisualizer(const std::string& name);//保存“虚拟光器”加传入的名称。
    
    //返回绘制好的图片
    cv::Mat measure(size_t loop_count,const OpticalField &optical_field) override;
    
    //直接返回电场的平方p_x,p_y,p_total
    Eigen::Matrix<double, 3, Eigen::Dynamic> measure(const OpticalField &optical_field);
    
    void auto_size() override;//自适应画幅
    const std::string& name() const override;//获取仪器名称，用作窗口名称。

private:
    std::string m_name;//仪器名称。
    std::atomic<bool> m_auto_size = true;//是否自适应画幅
    double m_y_axis_max_w;//y轴
};
