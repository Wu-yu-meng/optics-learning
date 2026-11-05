#pragma once
#include "communication/OpticalField.hpp"
#include <opencv2/core/persistence.hpp>

OpticalField init_gauss_optical_field(const cv::FileNode &basic_config);//使用指定配置创建一个高斯光场
