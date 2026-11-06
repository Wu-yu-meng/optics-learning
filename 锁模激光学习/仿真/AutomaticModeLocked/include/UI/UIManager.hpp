#pragma once
#include "communication/Channel.hpp"
#include "communication/UIMessage.hpp"
#include "communication/CavityMessage.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <unordered_map>
#include <vector>

//可视化管理器，负责调用可视化
//分为两个部分，一个是仿真仪器的可视化，一个是虚拟仪器的可视化
class UIManager
{
public:
    UIManager(const std::string &config_path,Channel<UIMessage> &ui_channel,Channel<CavityMessage> &cavity_channel);
    void run();//主线程中处理窗口、滑动条和按键

    void set_image(const std::string &instrument_name,const std::string &window_name,const cv::Mat &image);//由画面消息更新图片
    void report_result(bool success,const std::string &reason);//显示指令执行结果
    void stop();//收到计算线程结束通知后退出UI循环
private:
    void update();//接收整批消息，刷新各仪器的最新画面
    void handle_key(int key);//按键转化为指令
    void request_stop();//通知计算线程终止

    Channel<UIMessage>::Publisher m_publisher;//发布UI指令
    Channel<CavityMessage>::Subscriber m_subscriber;//接收计算结果

    struct Window
    {
        std::string window_id;//OpenCV内部使用英文窗口标识
        std::string window_name;//显示标题
        cv::Mat image;//保存该仪器最新的一帧
    };
    std::unordered_map<std::string,Window> m_windows;//配置中的仪器名称对应窗口

    struct EPCControl
    {
        std::string element_name;//配置中的EPC名称
        int channel;//通道ID，从0开始
        std::string trackbar_name;//滑动条名称
        int last_phase_deg;//上次发送的滑动条值
    };
    std::vector<EPCControl> m_epc_controls;//各EPC通道的滑动条
    std::vector<std::string> m_epc_names;//控制面板中的EPC名称
    std::string m_control_window = "CavityControls";//控制面板标识

    bool m_running = false;//UI请求的连续运行状态
    bool m_stop_requested = false;//已发送终止指令，等待计算线程结束
    bool m_stopped = false;//计算线程已经结束
};
