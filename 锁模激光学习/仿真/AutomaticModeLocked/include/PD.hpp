#pragma once
#include "OpticalField.hpp"


//PD等效为一个理想的检偏器
class PD
{
public:
    PD(double transmission_axis_angle_deg);
    OpticalField propagate(const OpticalField& input_optical_field) const;//PD的传播函数

private:
    double m_transmission_axis_angle_deg;//允许全部光透过的偏振方向，相对于x轴
};

