#pragma once
#include "communication/UIMessage.hpp"

//终止光学腔的运行循环
class StopMessage : public UIMessage
{
public:
    void apply(OpticalCavity &cavity) const override;
};
