#include "communication/messages/CommandResultMessage.hpp"

CommandResultMessage::CommandResultMessage(bool success,const std::string &reason)
    : success(success),reason(reason)
{
}
