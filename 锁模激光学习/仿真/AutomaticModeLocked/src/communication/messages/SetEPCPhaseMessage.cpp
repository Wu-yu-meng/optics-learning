#include "communication/messages/SetEPCPhaseMessage.hpp"
#include "construction/OpticalCavity.hpp"
#include "optical_element/EPC.hpp"
#include <stdexcept>

SetEPCPhaseMessage::SetEPCPhaseMessage(const std::string &element_name,int channel,double phase_deg)
    : m_element_name(element_name),m_channel(channel),m_phase_deg(phase_deg)
{
}

void SetEPCPhaseMessage::apply(OpticalCavity &cavity) const
{
    //腔负责检查器件名称，消息负责检查类型并执行具体操作。
    EPC *epc = dynamic_cast<EPC *>(&cavity.get_element(m_element_name));
    if (epc == nullptr)
    {
        throw std::invalid_argument("[SetEPCPhaseMessage]目标器件不是EPC：" + m_element_name);
    }

    epc->set_retarder(m_channel,m_phase_deg);
}
