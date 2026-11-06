#pragma once
#include "communication/UIMessage.hpp"
#include <string>

//调整指定仪器的量程，并重新发布当前画面
class AutoSizeMessage : public UIMessage
{
public:
    explicit AutoSizeMessage(const std::string &instrument_name);
    void apply(OpticalCavity &cavity) const override;

private:
    std::string m_instrument_name;//配置中的仪器唯一名称
};
