#pragma once
#include "communication/CavityMessage.hpp"
#include "algorithm/RandomSearch.hpp"

//计算线程确认当前实际使用的EPC设置，供UI同步滑动条。
struct EPCSettingsMessage : public CavityMessage
{
    explicit EPCSettingsMessage(const std::vector<random_search::EPCSetting> &epc_settings);
    void apply(UIManager &ui) const override;

    std::vector<random_search::EPCSetting> epc_settings;
};
