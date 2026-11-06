#pragma once
#include "communication/UIMessage.hpp"

//设置光学腔连续运行或暂停
class SetRunningMessage : public UIMessage
{
public:
    explicit SetRunningMessage(bool running);
    void apply(OpticalCavity &cavity) const override;

private:
    bool m_running;//是否连续运行
};
