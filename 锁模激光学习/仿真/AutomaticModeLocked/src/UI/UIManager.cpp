#include "UI/UIManager.hpp"
#include "json/ReJson.hpp"
#include "communication/messages/SetEPCPhaseMessage.hpp"
#include "communication/messages/AutoSizeMessage.hpp"
#include "communication/messages/SetRunningMessage.hpp"
#include "communication/messages/SingleStepMessage.hpp"
#include "communication/messages/StopMessage.hpp"
#include <cmath>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <utility>

UIManager::UIManager(const std::string &config_path,Channel<UIMessage> &ui_channel,Channel<CavityMessage> &cavity_channel)
    : m_publisher(ui_channel.create_publisher()),m_subscriber(cavity_channel.create_subscriber())
{
    ReJson config(config_path);
    m_feedback_status = static_cast<int>(config.config_["random_search"]["enabled"]) != 0
        ? "Random search: collecting measurements" : "Random search: disabled";

    //仪器名称只用于映射，OpenCV窗口和滑动条使用英文标识，避免Qt的Latin-1查找问题。
    size_t window_index = 0;
    for (const cv::FileNode &instrument_config : config.config_["instruments"])
    {
        const std::string instrument_name = static_cast<std::string>(instrument_config["name"]);
        Window window;
        window.window_id = "Instrument" + std::to_string(window_index++);
        window.window_name = instrument_name;
        window.image = cv::Mat(720,1280,CV_8UC3,cv::Scalar(28,28,28));
        cv::putText(window.image,"Waiting for measurement",{80,100},cv::FONT_HERSHEY_SIMPLEX,0.8,cv::Scalar(225,225,225),1,cv::LINE_AA);
        cv::namedWindow(window.window_id,cv::WINDOW_NORMAL);
        cv::resizeWindow(window.window_id,960,540);
        cv::setWindowTitle(window.window_id,window.window_name);
        cv::imshow(window.window_id,window.image);
        m_windows.emplace(instrument_name,std::move(window));
    }

    cv::namedWindow(m_control_window,cv::WINDOW_NORMAL);
    cv::resizeWindow(m_control_window,800,400);
    cv::setWindowTitle(m_control_window,"Cavity controls | Space: run/pause | D: step | A: AUTO | Q/Esc: quit");
    cv::imshow(m_control_window,cv::Mat(150,800,CV_8UC3,cv::Scalar(28,28,28)));

    //只读取EPC的界面配置，不持有器件实例。
    for (const cv::FileNode &element_config : config.config_["elements"])
    {
        if (static_cast<std::string>(element_config["type"]) != "EPC")
        {
            continue;
        }

        const std::string element_name = static_cast<std::string>(element_config["name"]);
        const std::string control_prefix = "EPC" + std::to_string(m_epc_names.size());
        m_epc_names.push_back(element_name);
        for (const cv::FileNode &retarder_config : element_config["parameters"]["retarders"])
        {
            EPCControl control;
            control.element_name = element_name;
            control.channel = static_cast<int>(retarder_config["ID"]);
            control.trackbar_name = control_prefix + " CH" + std::to_string(control.channel + 1) + " phase (deg)";
            control.last_phase_deg = static_cast<int>(retarder_config["delta_phase_deg"]);
            cv::createTrackbar(control.trackbar_name,m_control_window,nullptr,360);
            cv::setTrackbarPos(control.trackbar_name,m_control_window,control.last_phase_deg);
            m_epc_controls.push_back(std::move(control));
        }
    }
}

void UIManager::run()
{
    while (!m_stopped)
    {
        update();
        const int key = cv::waitKey(10);
        if (!m_stop_requested && !m_stopped)
        {
            handle_key(key);
        }
    }
    cv::destroyAllWindows();
}

void UIManager::update()
{
    const Channel<CavityMessage>::MessageQueue messages = m_subscriber.receive();
    for (const Channel<CavityMessage>::DataPtr &message : messages)
    {
        message->apply(*this);
    }
    if (m_stop_requested || m_stopped)
    {
        return;
    }

    //关闭控制面板或仪器窗口时，通知计算线程结束。
    if (cv::getWindowProperty(m_control_window,cv::WND_PROP_VISIBLE) < 1.0)
    {
        request_stop();
        return;
    }
    for (const std::pair<const std::string,Window> &window : m_windows)
    {
        if (cv::getWindowProperty(window.second.window_id,cv::WND_PROP_VISIBLE) < 1.0)
        {
            request_stop();
            return;
        }
    }

    //只在数值变化时发送指令，UI不直接修改计算线程的EPC。
    for (EPCControl &control : m_epc_controls)
    {
        const int phase_deg = cv::getTrackbarPos(control.trackbar_name,m_control_window);
        if (phase_deg != control.last_phase_deg)
        {
            m_publisher.publish(std::make_shared<SetEPCPhaseMessage>(control.element_name,control.channel,static_cast<double>(phase_deg)));
            control.last_phase_deg = phase_deg;
        }
    }

    //整批接收后，每台仪器只显示最新的一帧，cv::Mat持有图片缓冲区。
    for (const std::pair<const std::string,Window> &window : m_windows)
    {
        cv::setWindowTitle(window.second.window_id,window.second.window_name);
        cv::imshow(window.second.window_id,window.second.image);
    }

    cv::Mat control_image(175 + static_cast<int>(m_epc_names.size()) * 25,800,CV_8UC3,cv::Scalar(28,28,28));
    const cv::Scalar foreground(225,225,225);
    cv::putText(control_image,m_running ? "RUNNING" : "PAUSED",{25,35},cv::FONT_HERSHEY_SIMPLEX,0.7,foreground,1,cv::LINE_AA);
    cv::putText(control_image,"Space: run/pause    D: single step",{25,70},cv::FONT_HERSHEY_SIMPLEX,0.55,foreground,1,cv::LINE_AA);
    cv::putText(control_image,"A: AUTO all instruments    Q/Esc: quit",{25,105},cv::FONT_HERSHEY_SIMPLEX,0.55,foreground,1,cv::LINE_AA);
    cv::putText(control_image,m_feedback_status,{25,140},cv::FONT_HERSHEY_SIMPLEX,0.5,foreground,1,cv::LINE_AA);
    for (size_t index = 0; index < m_epc_names.size(); ++index)
    {
        cv::putText(control_image,"EPC" + std::to_string(index) + ": " + m_epc_names[index],
                    {25,175 + static_cast<int>(index) * 25},cv::FONT_HERSHEY_SIMPLEX,0.5,foreground,1,cv::LINE_AA);
    }
    cv::imshow(m_control_window,control_image);
}

void UIManager::set_image(const std::string &instrument_name,const std::string &window_name,const cv::Mat &image)
{
    Window &window = m_windows.at(instrument_name);
    window.window_name = window_name;
    window.image = image;
}

void UIManager::report_result(bool success,const std::string &reason)
{
    if (!success)
    {
        std::cerr << "[UI]指令执行失败：" << reason << std::endl;
    }
}

void UIManager::set_epc_settings(const std::vector<random_search::EPCSetting> &epc_settings)
{
    for (const random_search::EPCSetting &setting : epc_settings)
    {
        for (EPCControl &control : m_epc_controls)
        {
            if (control.element_name == setting.element_name && control.channel == setting.channel)
            {
                //保留尚未发出的手动拖动；程序同步滑动条时同时更新记录，避免反向发送指令。
                if (cv::getTrackbarPos(control.trackbar_name,m_control_window) == control.last_phase_deg)
                {
                    control.last_phase_deg = static_cast<int>(std::lround(setting.phase_deg));
                    cv::setTrackbarPos(control.trackbar_name,m_control_window,control.last_phase_deg);
                }
                break;
            }
        }
    }
}

void UIManager::report_feedback(size_t loop_count,bool is_locked,const std::vector<random_search::EPCSetting> &measured_epc_settings)
{
    m_feedback_status = "Search RT " + std::to_string(loop_count) + ": previous settings "
        + (is_locked ? "PASS" : "FAIL") + "; applied new random settings";
    std::cout << "[RandomSearch] loop=" << loop_count << " pulse_criterion=" << (is_locked ? "PASS" : "FAIL")
              << " measured settings:";
    for (const random_search::EPCSetting &setting : measured_epc_settings)
    {
        std::cout << " " << setting.element_name << ".CH" << setting.channel + 1 << "=" << setting.phase_deg;
    }
    std::cout << std::endl;
}

void UIManager::stop()
{
    m_stopped = true;
}

void UIManager::request_stop()
{
    m_publisher.publish(std::make_shared<StopMessage>());
    m_stop_requested = true;
}

void UIManager::handle_key(int key)
{
    if (key == 'q' || key == 'Q' || key == 27)
    {
        request_stop();
    }
    else if (key == ' ')
    {
        m_running = !m_running;
        m_publisher.publish(std::make_shared<SetRunningMessage>(m_running));
    }
    else if (key == 'd' || key == 'D')
    {
        m_running = false;
        m_publisher.publish(std::make_shared<SingleStepMessage>());
    }
    else if (key == 'a' || key == 'A')
    {
        for (const std::pair<const std::string,Window> &window : m_windows)
        {
            m_publisher.publish(std::make_shared<AutoSizeMessage>(window.first));
        }
    }
}
