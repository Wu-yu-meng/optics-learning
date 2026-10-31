#pragma once
#include "OpticalElement.hpp"

//考虑Kerr非线性、二阶色散
//按固定距离分段，每段采用半步色散->整步Kerr->半步色散
class SMF : public OpticalElement
{
public:
    SMF(double gamma_per_w_m, double length_m, double beta2_ps2_per_km);//单位依次为W^-1*m^-1、m、ps^2/km
    OpticalOutputs propagate(const OpticalInputs& input_optical_fields) override;//SMF传播函数
private:
    void kerr(OpticalField &optical_field, double step_length_m) const;//原地计算Kerr非线性，不修改SMF参数
    void dispersion2(OpticalField &optical_field, double step_length_m) const;//二阶色散,需要传入作用距离
    Eigen::Matrix2cd m_xy_to_uv_matrix;//将XY偏振分量转换为UV圆偏振分量
    double m_gamma_per_w_m;//非线性系数、W^-1*m^-1
    double m_length_m;//光纤长度、m
    double m_beta2_ps2_per_km;//二阶色散系数、ps^2/km，可正可负
};
