#pragma once
#include "OpticalField.hpp"

//考虑分光和元器件的损耗
class OC
{
public:
    struct OutputField
    {
        OpticalField cavity;//腔内光场  
        OpticalField extracted;//输出光场
    };
    OC(double cavity_ratio,double power_transmittance);
    OutputField propagate(const OpticalField& input_optical_field) const;//OC的传播函数
private:

    //留在腔内的光场的功率占比
    double m_cavity_ratio;

    //光强透过率
    double m_power_transmittance;
};