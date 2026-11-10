#include "communication/messages/AutoSizeMessage.hpp"
#include "construction/OpticalCavity.hpp"

AutoSizeMessage::AutoSizeMessage(const std::string &instrument_name)
    : m_instrument_name(instrument_name)
{
}

void AutoSizeMessage::apply(OpticalCavity &cavity) const
{
    cavity.get_instrument(m_instrument_name).auto_size();
    cavity.request_observations();
}
