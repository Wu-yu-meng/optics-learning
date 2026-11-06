#pragma once
#include "communication/UIMessage.hpp"

//暂停连续运行，立即传播一圈并发布画面
class SingleStepMessage : public UIMessage
{
public:
    void apply(OpticalCavity &cavity) const override;
};
