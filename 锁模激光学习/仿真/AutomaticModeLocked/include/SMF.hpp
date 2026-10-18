#pragma once
#include "OpticalField.hpp"

//当前仅考虑Kerr非线性
class SMF
{
public:
    SMF(double gamma_per_w_m, double length_m);//单位依次为W^-1*m^-1、m
    OpticalField propagate(const OpticalField& input_optical_field) const;//SMF传播函数
private:
    Eigen::Matrix2cd m_xy_to_uv_matrix;//将XY偏振分量转换为UV圆偏振分量
    double m_gamma_per_w_m;//非线性系数、W^-1*m^-1
    double m_length_m;//光纤长度、m
};
