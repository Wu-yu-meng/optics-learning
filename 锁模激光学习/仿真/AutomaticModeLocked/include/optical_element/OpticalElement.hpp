#pragma once
#include "OpticalField.hpp"
#include <string>
#include <unordered_map>

//各输出端口对应的光场
using OpticalOutputs = std::unordered_map<std::string, OpticalField>;

//光学器件基类
class OpticalElement
{
public:
    virtual ~OpticalElement();//虚析构函数，支持通过基类指针释放器件
    virtual OpticalOutputs propagate(const OpticalField& input_optical_field) = 0;//器件的传播函数
};
