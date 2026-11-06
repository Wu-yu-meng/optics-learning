#include "communication/messages/InstrumentFrameMessage.hpp"
#include "UI/UIManager.hpp"
#include <utility>

InstrumentFrameMessage::InstrumentFrameMessage(const std::string &instrument_name,const std::string &window_name,
                                             const std::string &source,size_t loop_count,cv::Mat image)
    : instrument_name(instrument_name),window_name(window_name),source(source),loop_count(loop_count),image(std::move(image))
{
}

void InstrumentFrameMessage::apply(UIManager &ui) const
{
    ui.set_image(instrument_name,window_name,image);
}
