#pragma once
#include "communication/OpticalField.hpp"
#include <opencv2/core.hpp>
#include <string>

//测量仪器基类，只负责生成图片，不操作UI窗口
class Instrument
{
public:
    virtual ~Instrument();//虚析构函数，支持通过基类指针释放仪器
    virtual cv::Mat measure(size_t loop_count,const OpticalField &optical_field) = 0;//返回测量图片
    virtual const std::string &name() const = 0;//仪器的显示名称
    virtual void auto_size() = 0;//根据下一次测量结果重新设置量程
};
