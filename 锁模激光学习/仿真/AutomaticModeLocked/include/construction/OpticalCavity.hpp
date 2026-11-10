#pragma once
#include "communication/Channel.hpp"
#include "communication/CavityMessage.hpp"
#include "communication/UIMessage.hpp"
#include "optical_element/OpticalElement.hpp"
#include "instrument/Instrument.hpp"
#include "algorithm/RandomSearch.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Builder;//前向声明

//OpticalInputs和OpticalOutputs的时间同步应该是在OpticalCavity中进行的
//同时OpticalCavity还要负责将下一个真正传入器件的OpticalInputs中的映射名称由器件+port的表达改为标准的名称，例如in1,in2
//当前没有考虑时序关系！！！
class OpticalCavity
{
friend Builder;

public:
    void run();//运行线程
    OpticalElement &get_element(const std::string &element_name);//检查名称并返回器件，由计算线程调用
    Instrument &get_instrument(const std::string &instrument_name);//检查名称并返回仪器，由计算线程调用
    void stop();//结束计算线程的运行循环
    void set_running(bool running);//设置连续运行状态，由计算线程调用
    void run_once();//计算一圈、发布观测并进行反馈，由计算线程调用
    void request_observations();//请求刷新当前画面，不推进计算圈数
private:

    OpticalCavity(Channel<CavityMessage> &cavity_channel, Channel<UIMessage> &ui_channel, const ReJson &config);//构造接收和发布端点

    void apply_command();//调度指令
    void calculate_once();//计算一圈光场传播
    void publish_observations();//测量各观测点并发布仪器画面
    void apply_feedback();//进行反馈

    Channel<UIMessage>::Subscriber m_subscriber;//接收UI发出的消息
    Channel<CavityMessage>::Publisher m_publisher;//发布光学腔发出的消息
    random_search::RandomSearch m_random_search;//随机搜索算法

    using g_output_port_name = std::string;//g_output_port_name = 唯一器件名称 + 器件端输出口名 
    using input_port_name = std::string;//input_port_name = 器件输入端口名


    //用于暂存光场：output_port_name - 光场
    std::unordered_map<g_output_port_name,OpticalField> m_optical_field_map;

    struct Element//元器件
    {
        std::string my_name;//唯一器件名称
        std::unordered_map<input_port_name,g_output_port_name> connections;//用于表示连接关系：output_port_name - input_port_name
        std::unique_ptr<OpticalElement> element_ptr;//元器件实例指针
    };
    std::vector<Element> m_elements;//元器件队列

    struct Observation//观测仪器
    {
        std::string my_name;//配置中的仪器唯一名称
        g_output_port_name source;//被观测的输出端口
        std::unique_ptr<Instrument> instrument_ptr;//仪器实例指针
    };
    std::vector<Observation> m_observations;//观测仪器队列

    struct FeedbackEPC//随机搜索控制的EPC，指针只引用m_elements中持有的器件
    {
        std::string element_name;
        EPC *epc;
    };
    bool m_feedback_enabled;//是否启用随机搜索
    VirtualOpticalPowerVisualizer *m_feedback_instrument = nullptr;//引用m_observations中持有的光功率仪
    g_output_port_name m_feedback_source;//反馈测量使用的观测端口
    std::vector<FeedbackEPC> m_feedback_epcs;//按配置顺序保存，保证设置快照的顺序一致
    size_t m_loop = 0;//循环次数
    bool m_running = false;//启动时暂停
    bool m_stopped = false;//是否终止，只在计算线程中修改
    bool m_observations_requested = false;//是否收到刷新当前画面的请求

};
