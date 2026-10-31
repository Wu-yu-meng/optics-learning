#include "optical_element/PD.hpp"
#include <cmath>
#include <numbers>
#include <utility>

PD::PD(double transmission_axis_angle_deg)
    : m_transmission_axis_angle_deg(transmission_axis_angle_deg)
{
}

OpticalOutputs PD::propagate(const OpticalInputs & input_optical_fields)
{
    const OpticalField& input_optical_field = input_optical_fields.at("in").get();

    //构造传播用的矩阵
    double transmission_axis_angle_rad = m_transmission_axis_angle_deg * std::numbers::pi_v<double> / 180.0;
    Eigen::Matrix2d transmission_matrix;
    double cos_angle = std::cos(transmission_axis_angle_rad);
    double sin_angle = std::sin(transmission_axis_angle_rad);
    transmission_matrix << cos_angle * cos_angle, cos_angle * sin_angle,
                           cos_angle * sin_angle, sin_angle * sin_angle;

    //计算透过后的偏振
    OpticalField::Polarization output_polarization = transmission_matrix * input_optical_field.polarization();

    //返回
    OpticalOutputs output_fields;
    output_fields.emplace("out",OpticalField(std::move(output_polarization),input_optical_field.basic_info()));
    return output_fields;
}
