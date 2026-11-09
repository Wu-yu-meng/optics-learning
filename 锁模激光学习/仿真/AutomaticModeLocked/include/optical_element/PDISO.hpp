#pragma once
#include "OpticalElement.hpp"


//PDISO等效为一个理想的检偏器
class PDISO : public OpticalElement
{
public:
    PDISO(double transmission_axis_angle_deg);
    OpticalOutputs propagate(const OpticalInputs& input_optical_fields) override;//PDISO的传播函数

private:
    double m_transmission_axis_angle_deg;//允许全部光透过的偏振方向，相对于x轴
};

