#include "communication/messages/EPCSettingsMessage.hpp"
#include "UI/UIManager.hpp"

EPCSettingsMessage::EPCSettingsMessage(const std::vector<random_search::EPCSetting> &epc_settings)
    : epc_settings(epc_settings)
{
}

void EPCSettingsMessage::apply(UIManager &ui) const
{
    ui.set_epc_settings(epc_settings);
}
