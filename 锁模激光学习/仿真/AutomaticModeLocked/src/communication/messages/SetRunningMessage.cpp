#include "communication/messages/SetRunningMessage.hpp"
#include "construction/OpticalCavity.hpp"

SetRunningMessage::SetRunningMessage(bool running)
    : m_running(running)
{
}

void SetRunningMessage::apply(OpticalCavity &cavity) const
{
    cavity.set_running(m_running);
}
