#include "construction/OpticalCavity.hpp"
#include "communication/messages/CommandResultMessage.hpp"
#include "communication/messages/InstrumentFrameMessage.hpp"
#include "communication/messages/CavityStoppedMessage.hpp"
#include <chrono>
#include <functional>
#include <stdexcept>
#include <thread>
#include <utility>

OpticalCavity::OpticalCavity(Channel<CavityMessage> &cavity_channel, Channel<UIMessage> &ui_channel)
    : m_subscriber(ui_channel.create_subscriber()),
      m_publisher(cavity_channel.create_publisher())
{
}

void OpticalCavity::run()
{
    publish_observations();//发布已经初始化的观测点画面。
    while (!m_stopped)
    {

        apply_command();//处理UI的指令
        if (m_stopped)
        {
            break;
        }
        if (m_running)
        {
            run_once();//运行一次计算
            publish_observations();//发布观测
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));//暂停时仍接收指令。
        }
    }
    m_publisher.publish(std::make_shared<CavityStoppedMessage>());
}

void OpticalCavity::stop()
{
    m_stopped = true;
}

void OpticalCavity::set_running(bool running)
{
    m_running = running;
}

void OpticalCavity::publish_observations()
{
    for (Observation &observation : m_observations)
    {
        const std::unordered_map<g_output_port_name,OpticalField>::const_iterator source = m_optical_field_map.find(observation.source);
        //未参与反馈初始化的输出端口，等第一圈传播产生光场后再观测。
        if (source == m_optical_field_map.end())
        {
            continue;
        }

        cv::Mat image = observation.instrument_ptr->measure(m_loop,source->second);
        m_publisher.publish(std::make_shared<InstrumentFrameMessage>(observation.my_name,observation.instrument_ptr->name(),observation.source,m_loop,std::move(image)));
    }
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

Instrument &OpticalCavity::get_instrument(const std::string &instrument_name)
{
    for (Observation &observation : m_observations)
    {
        if (observation.my_name == instrument_name)
        {
            return *observation.instrument_ptr;
        }
    }

    throw std::invalid_argument("[OpticalCavity]没有找到仪器：" + instrument_name);
}

void OpticalCavity::apply_command()
{
    //只处理本次取出的FIFO消息，处理期间新入队的指令留到下一轮。
    const Channel<UIMessage>::MessageQueue messages = m_subscriber.receive();
    for (const Channel<UIMessage>::DataPtr &message : messages)
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
        if (m_stopped)
        {
            break;//终止指令之后不再执行本批次剩余操作。
        }
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
