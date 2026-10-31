#pragma once
#include "OpticalField.hpp"
#include <functional>
#include <string>
#include <unordered_map>

//各输入端口对应的光场，只引用数据，不复制采样矩阵
using OpticalInputs = std::unordered_map<std::string,std::reference_wrapper<const OpticalField>>;

//各输出端口对应的光场
using OpticalOutputs = std::unordered_map<std::string, OpticalField>;

//光学器件基类
class OpticalElement
{
public:
    virtual ~OpticalElement();//虚析构函数，支持通过基类指针释放器件
    virtual OpticalOutputs propagate(const OpticalInputs& input_optical_fields) = 0;//器件的传播函数
};
