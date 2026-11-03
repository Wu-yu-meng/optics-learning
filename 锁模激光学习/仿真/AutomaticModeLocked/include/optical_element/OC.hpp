#pragma once
#include "OpticalElement.hpp"

//考虑分光和元器件的损耗
class OC : public OpticalElement
{
public:
    OC(double cavity_ratio,double power_transmittance);
    OpticalOutputs propagate(const OpticalInputs& input_optical_fields) override;//OC的传播函数
private:

    //留在腔内的光场的功率占比
    double m_cavity_ratio;

    //光强透过率
    double m_power_transmittance;
};
