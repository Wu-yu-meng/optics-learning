#include "communication/OpticalField.hpp"
#include <utility>

OpticalField::OpticalField(Polarization polarization, BasicInfo basic_info)
    : m_polarization(std::move(polarization)),
      m_basic_info(basic_info)
{
    //采样数由实际的光场矩阵决定，避免基本信息与矩阵不一致。
    m_basic_info.sample_size = static_cast<std::size_t>(m_polarization.cols());
}

const OpticalField::Polarization &OpticalField::polarization() const
{
    return m_polarization;
}

const OpticalField::BasicInfo &OpticalField::basic_info() const
{
    return m_basic_info;
}
