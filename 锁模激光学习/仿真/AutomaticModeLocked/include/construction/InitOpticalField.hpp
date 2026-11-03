#pragma once
#include "OpticalField.hpp"

namespace cv
{
class FileNode;
}

OpticalField init_gauss_optical_field();//创建一个高斯光场
OpticalField init_gauss_optical_field(const cv::FileNode &basic_config);//使用指定配置创建一个高斯光场
