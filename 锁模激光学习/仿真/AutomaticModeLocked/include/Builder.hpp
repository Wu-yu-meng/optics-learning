#pragma once
#include "config/config.hpp"
#include "optical_element/OpticalElement.hpp"
#include <memory>
#include <vector>

class Builder;//前向声明

//OpticalInputs和OpticalOutputs的时间同步应该是在OpticalCavity中进行的
//同时OpticalCavity还要负责将下一个真正传入器件的OpticalInputs中的映射名称由器件+port的表达改为标准的名称，例如in1,in2
//当前没有考虑时序关系！！！
class OpticalCavity
{
friend Builder;

public:
    struct Observation
    {
        std::string name;//仪器名称
        std::string source;//观测的全局输出端口名
    };
    struct Runtime
    {
        bool start_paused = false;//是否暂停启动
        int frame_delay_ms = 20;//画面更新等待时间
        bool epc_controls = true;//是否显示EPC滑动条
        size_t render_every_n_rounds = 1;//每隔多少圈绘制一次
        size_t max_rounds = 0;//0表示持续运行
    };

    explicit OpticalCavity(OpticalField initial_optical_field);
    void run_once();
    void reset();//重置光场、循环次数和EDF噪声种子，保留当前器件参数
    size_t loop_count() const;
    const OpticalField& field(const std::string& output_port_name) const;//获取指定观测光场
    OpticalElement& element(const std::string& element_name);//获取需要交互调节的器件
    const std::vector<Observation>& observations() const;
    const std::vector<std::string>& epc_names() const;
    const Runtime& runtime() const;
    void describe() const;//打印实际执行顺序与连接
private:

    using g_output_port_name = std::string;//g_output_port_name = 唯一器件名称 + 器件端输出口名 
    using input_port_name = std::string;//input_port_name = 器件输入端口名


    //用于暂存光场：output_port_name - 光场
    std::unordered_map<g_output_port_name,OpticalField> m_optical_field_map;

    struct Element
    {
        std::string my_name;//唯一器件名称
        std::unordered_map<input_port_name,g_output_port_name> connections;//用于表示连接关系：output_port_name - input_port_name
        std::unique_ptr<OpticalElement> element_ptr;//元器件实例指针
    };
    std::vector<Element> m_elements;//元器件队列
    size_t m_loop = 0;//循环次数
    OpticalField m_initial_optical_field;//来自本次构建配置的初始光场
    std::vector<Observation> m_observations;//观测点
    std::vector<std::string> m_epc_names;//需要创建滑动条的EPC
    Runtime m_runtime;

};


//读取配置文件构建激光腔
class Builder
{
public:
    Builder() = delete;
    Builder(ReJson &&config);

    OpticalCavity build();//创建激光腔
private:
    ReJson m_config;
};
