#pragma once
#include <opencv2/core.hpp>

//示波器
class Oscilloscope
{
public:

    //返回光->电信信号结果
    cv::Mat measure();
};
