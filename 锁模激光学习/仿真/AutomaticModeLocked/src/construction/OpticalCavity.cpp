#include "construction/OpticalCavity.hpp"
#include "communication/messages/CommandResultMessage.hpp"
#include <functional>
#include <stdexcept>
#include <utility>

OpticalCavity::OpticalCavity(Channel<CavityMessage> &cavity_channel, Channel<UIMessage> &ui_channel)
    : m_subscriber(ui_channel.create_subscriber()),
      m_publisher(cavity_channel.create_publisher())
{
}

void OpticalCavity::run()
{
    


}

OpticalElement &OpticalCavity::get_element(const std::string &element_name)
{
    //只负责按名称查找器件，具体操作由消息实现。
    for (Element &element : m_elements)
    {
        if (element.my_name == element_name)
        {
            return *element.element_ptr;
        }
    }

    throw std::invalid_argument("[OpticalCavity]没有找到器件：" + element_name);
}

void OpticalCavity::apply_command()
{
    //按FIFO顺序执行，不需要判断消息的具体子类型。
    while (Channel<UIMessage>::DataPtr message = m_subscriber.receive())
    {
        bool success = true;
        std::string reason;
        try
        {
            message->apply(*this);
        }
        catch (const std::exception &error)
        {
            success = false;
            reason = error.what();
        }

        //单条指令失败后仍继续处理下一条，结果交给UI读取。
        m_publisher.publish(std::make_shared<CommandResultMessage>(success,reason));
    }
}

// 腔内光运行一次
void OpticalCavity::run_once()
{

    //按照顺序开始
    for (const auto &element : m_elements)
    {
        //构建输入光场
        OpticalInputs optical_inputs;
        for (const auto &connection : element.connections)
        {
            optical_inputs.emplace(connection.first,std::cref(m_optical_field_map.at(connection.second)));
        }

        //得到输出光场
        OpticalOutputs optical_outputs = element.element_ptr->propagate(optical_inputs);

        //放入m_optical_field_map
        for (OpticalOutputs::value_type &optical_output : optical_outputs)
        {
            m_optical_field_map.insert_or_assign(element.my_name + "-" + optical_output.first,std::move(optical_output.second));
        }
    }
    
    m_loop++;
}
