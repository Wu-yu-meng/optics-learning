#include "communication/messages/CommandResultMessage.hpp"
#include "UI/UIManager.hpp"

CommandResultMessage::CommandResultMessage(bool success,const std::string &reason)
    : success(success),reason(reason)
{
}

void CommandResultMessage::apply(UIManager &ui) const
{
    ui.report_result(success,reason);
}
