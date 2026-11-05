#pragma once
#include "communication/CavityMessage.hpp"
#include <string>

//光学腔按指令的处理顺序返回执行结果
struct CommandResultMessage : public CavityMessage
{
    CommandResultMessage(bool success,const std::string &reason);

    bool success;//是否执行成功
    std::string reason;//失败原因，成功时为空
};
