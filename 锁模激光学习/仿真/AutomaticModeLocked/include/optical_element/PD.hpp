#pragma once
#include "OpticalElement.hpp"


//PD等效为一个理想的检偏器
class PD : public OpticalElement
{
public:
    PD(double transmission_axis_angle_deg);
    OpticalOutputs propagate(const OpticalField& input_optical_field) override;//PD的传播函数

private:
    double m_transmission_axis_angle_deg;//允许全部光透过的偏振方向，相对于x轴
};

