#pragma once
#include "communication/CavityMessage.hpp"
#include "algorithm/RandomSearch.hpp"
#include <cstddef>

//随机搜索结果：判据对应旧设置，输出中的随机设置已应用，下一圈生效。
struct RandomSearchResultMessage : public CavityMessage
{
    RandomSearchResultMessage(size_t loop_count,const std::vector<random_search::EPCSetting> &measured_epc_settings,
                              const random_search::FeedbackOutput &feedback_output);
    void apply(UIManager &ui) const override;

    size_t loop_count;
    std::vector<random_search::EPCSetting> measured_epc_settings;
    random_search::FeedbackOutput feedback_output;
};
