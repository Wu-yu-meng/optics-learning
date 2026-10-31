#pragma once
#include "config/config.hpp"
#include "optical_element/OpticalElement.hpp"
#include <queue>

class Builder;//前向声明

//OpticalInputs和OpticalOutputs的时间同步应该是在OpticalCavity中进行的
//同时OpticalCavity还要负责将下一个真正传入器件的OpticalInputs中的映射名称由器件+port的表达改为标准的名称，例如in1,in2
//当前没有考虑时序关系！！！
class OpticalCavity
{
friend Builder;

public:
    void run_once();
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