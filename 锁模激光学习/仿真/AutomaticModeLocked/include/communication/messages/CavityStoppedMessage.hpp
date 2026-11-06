#pragma once
#include "communication/CavityMessage.hpp"

//计算循环已结束，通知UI退出
class CavityStoppedMessage : public CavityMessage
{
public:
    void apply(UIManager &ui) const override;
};
