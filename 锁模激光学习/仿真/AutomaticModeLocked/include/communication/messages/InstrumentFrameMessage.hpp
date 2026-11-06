#pragma once
#include "communication/CavityMessage.hpp"
#include <cstddef>
#include <opencv2/core.hpp>
#include <string>

//仪器生成的观测画面，发布后图片按只读数据使用
struct InstrumentFrameMessage : public CavityMessage
{
    InstrumentFrameMessage(const std::string &instrument_name,const std::string &window_name, const std::string &source,size_t loop_count,cv::Mat image);
    void apply(UIManager &ui) const override;

    std::string instrument_name;//配置中的仪器唯一名称
    std::string window_name;//仪器的显示名称
    std::string source;//被观测的完整输出端口名
    size_t loop_count;//本次观测对应的圈数
    cv::Mat image;//测量图片
};
