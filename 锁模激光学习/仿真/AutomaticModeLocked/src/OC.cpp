#include "OC.hpp"

OC::OC(double cavity_ratio, double power_transmittance)
    : m_cavity_ratio(cavity_ratio),
      m_power_transmittance(power_transmittance)
{
}

OC::OutputField OC::propagate(const OpticalField &input_optical_field) const
{

    //获取光场
    const OpticalField::Polarization& polarization = input_optical_field.polarization();

    //准备输入
    OpticalField::Polarization cavity;
    OpticalField::Polarization extracted;

    //计算场强系数
    double cavity_ratio = std::sqrt(m_power_transmittance * m_cavity_ratio);
    double extracted_ratio = std::sqrt(m_power_transmittance * (1.0 - m_cavity_ratio));

    //计算光场偏振
    cavity = cavity_ratio * polarization;
    extracted = extracted_ratio * polarization;

    //返回
    return OutputField{
        OpticalField(std::move(cavity),input_optical_field.basic_info()),
        OpticalField(std::move(extracted),input_optical_field.basic_info()),
    };
}


