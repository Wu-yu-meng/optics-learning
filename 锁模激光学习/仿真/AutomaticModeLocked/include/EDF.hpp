#pragma once
#include "OpticalField.hpp"
//EDF只考虑其增益效果，不考虑其作为光纤的Kerr效应和二阶色散
//采用的模型为小信号增益

class EDF
{
public:
    EDF(double g_0, double E_sat, double length);//构造函数，单位依次为m^-1、pJ、m
    OpticalField propagate(const OpticalField& input_optical_field) const;//EDF传播函数

private:
    double m_g_0;//小信号增益m^-1
    double m_E_sat;//饱和脉冲能量pJ
    double m_length;//长度m
};
