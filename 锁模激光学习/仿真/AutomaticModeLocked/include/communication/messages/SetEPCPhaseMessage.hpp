#pragma once
#include "communication/UIMessage.hpp"
#include <string>

//设置指定EPC通道的相位延迟
class SetEPCPhaseMessage : public UIMessage
{
public:
    SetEPCPhaseMessage(const std::string &element_name,int channel,double phase_deg);
    void apply(OpticalCavity &cavity) const override;

private:
    std::string m_element_name;//器件的唯一名称
    int m_channel;//通道ID，从0开始
    double m_phase_deg;//相位延迟，单位deg
};
