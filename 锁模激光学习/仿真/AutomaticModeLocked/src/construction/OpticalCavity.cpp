#include "construction/OpticalCavity.hpp"
#include "communication/messages/CommandResultMessage.hpp"
#include "communication/messages/InstrumentFrameMessage.hpp"
#include "communication/messages/CavityStoppedMessage.hpp"
#include "communication/messages/RandomSearchResultMessage.hpp"
#include "communication/messages/EPCSettingsMessage.hpp"
#include <chrono>
#include <functional>
#include <stdexcept>
#include <thread>
#include <utility>

OpticalCavity::OpticalCavity(Channel<CavityMessage> &cavity_channel, Channel<UIMessage> &ui_channel, const ReJson &config)
    : m_subscriber(ui_channel.create_subscriber()),
      m_publisher(cavity_channel.create_publisher()),
      m_random_search(config),
      m_feedback_enabled(static_cast<int>(config.config_["random_search"]["enabled"]) != 0)
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

void OpticalCavity::request_observations()
{
    m_observations_requested = true;
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
    m_observations_requested = false;
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

    //返回整批指令处理后的实际设置，避免在途的手动指令与较早的自动反馈使滑动条失去同步。
    if (!messages.empty() && !m_stopped)
    {
        std::vector<random_search::EPCSetting> epc_settings;
        for (const Element &element : m_elements)
        {
            const EPC *epc = dynamic_cast<const EPC *>(element.element_ptr.get());
            if (epc != nullptr)
            {
                for (const EPC::Retarder &retarder : epc->retarders())
                {
                    epc_settings.push_back({element.my_name,retarder.ID,retarder.delta_phase_deg});
                }
            }
        }
        m_publisher.publish(std::make_shared<EPCSettingsMessage>(epc_settings));
    }

    //刷新请求统一在指令处理后执行，暂停时也可以调整量程。
    if (m_observations_requested && !m_stopped)
    {
        publish_observations();
    }
}

// 腔内光运行一次
void OpticalCavity::run_once()
{
    calculate_once();
    publish_observations();
    apply_feedback();
}

void OpticalCavity::calculate_once()
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

void OpticalCavity::apply_feedback()
{
    if (!m_feedback_enabled)
    {
        return;
    }

    //调用算法获取反馈
    const Eigen::Matrix<double,3,Eigen::Dynamic> measurement = m_feedback_instrument->measure(m_optical_field_map.at(m_feedback_source));
    std::vector<random_search::EPCSetting> epc_settings;
    for (const FeedbackEPC &target : m_feedback_epcs)
    {
        for (const EPC::Retarder &retarder : target.epc->retarders())
        {
            epc_settings.push_back({target.element_name,retarder.ID,retarder.delta_phase_deg});
        }
    }
    const random_search::FeedbackInput feedback_input{measurement,std::move(epc_settings)};
    const std::optional<random_search::FeedbackOutput> feedback_output = m_random_search.feedback(feedback_input);
    if (!feedback_output.has_value())
    {
        return;
    }

    //处理反馈
    //本圈测量完成后统一修改，各通道的新设置从下一圈传播开始生效。
    for (const random_search::EPCSetting &setting : feedback_output->epc_settings)
    {
        EPC &epc = dynamic_cast<EPC &>(get_element(setting.element_name));
        epc.set_retarder(setting.channel,setting.phase_deg);
    }

    //用消息通知UI
    //is_locked属于修改前的测量历史，同时保留旧设置，避免将判据结果归给新随机设置。
    m_publisher.publish(std::make_shared<RandomSearchResultMessage>(m_loop,feedback_input.epc_settings,*feedback_output));
}
