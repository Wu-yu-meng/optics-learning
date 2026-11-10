#include "communication/messages/RandomSearchResultMessage.hpp"
#include "UI/UIManager.hpp"

RandomSearchResultMessage::RandomSearchResultMessage(size_t loop_count,const std::vector<random_search::EPCSetting> &measured_epc_settings,
                                                   const random_search::FeedbackOutput &feedback_output)
    : loop_count(loop_count),measured_epc_settings(measured_epc_settings),feedback_output(feedback_output)
{
}

void RandomSearchResultMessage::apply(UIManager &ui) const
{
    ui.set_epc_settings(feedback_output.epc_settings);
    ui.report_feedback(loop_count,feedback_output.is_locked,measured_epc_settings);
}
