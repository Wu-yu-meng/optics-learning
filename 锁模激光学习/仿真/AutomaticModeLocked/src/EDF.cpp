#include "EDF.hpp"

EDF::EDF(double g_0, double E_sat, double length)
    : m_g_0(g_0),
      m_E_sat(E_sat),
      m_length(length)
{
}

OpticalField EDF::propagate(const OpticalField &input_optical_field) const
{
    
    //按照g= g_0 * (1 + E/E_t)^(-1)的模型进行增益系数计算
    //然后计算功率开根，乘到电场上

    //计算光场的能量
    double energy_j = 0;//光场能量J
    double sample_stride_s = input_optical_field.basic_info().sample_stride * 1.0e-15;//采样步长s
    const OpticalField::Polarization &polarization = input_optical_field.polarization();//偏振
    for (size_t i = 0; i < input_optical_field.basic_info().sample_size; i++)
    {
        //瞬时功率w P(t)=|Ax(t)|²+|Ay(t)|²
        double power_w = std::norm(polarization(0,i)) + std::norm(polarization(1,i));
        
        //求和(离散积分)得到能量
        energy_j += power_w * sample_stride_s;
    }

    //计算增益系数
    double g = m_g_0 / (1 + energy_j / (m_E_sat * 1.0e-12));

    //计算电场的增益 E_gain = (e^gL)^0.5 = e^(0.5gL)
    double amplitude_gain = std::exp(0.5 * g * m_length);

    //计算偏振
    OpticalField::Polarization output_polarization = amplitude_gain * polarization;

    //返回
    return OpticalField(std::move(output_polarization),input_optical_field.basic_info());

}
