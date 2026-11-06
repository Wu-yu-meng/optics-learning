#include "communication/messages/CavityStoppedMessage.hpp"
#include "UI/UIManager.hpp"

void CavityStoppedMessage::apply(UIManager &ui) const
{
    ui.stop();
}
