#pragma once
#include "optical_element/EPC.hpp"
#include "instrument/VirtualOpticalPowerVisualizer.hpp"
#include "json/ReJson.hpp"
#include <vector>
#include <functional>
#include <optional>

namespace random_search
{

//epc的设置（单个通道值）
struct EPCSetting
{
    std::string element_name;
    int channel;
    double phase_deg;
};

//反馈函数的输入
struct FeedbackInput
{
    const Eigen::Matrix<double,3,Eigen::Dynamic>& measurement;//仪器的测量结果
    std::vector<EPCSetting> epc_settings;//产生本次测量时使用的EPC设置
};

//反馈函数的输出
struct FeedbackOutput
{
    bool is_locked = false;
    std::vector<EPCSetting> epc_settings;
};

//随机搜索算法
class RandomSearch
{
public:
    //构造函数
    RandomSearch(const ReJson& config);
    
    //搜集数据（虚拟光功率计）或反馈一次（是否锁模，调整EPC）
    std::optional<FeedbackOutput> feedback(const FeedbackInput &feeback_input);

private:
    const ReJson& m_config;//配置文件
    
    struct AnalyzedData//对FeedbackInput中的数据进行分析从而得到的数据
    {

    };


};


}//namespace random_search