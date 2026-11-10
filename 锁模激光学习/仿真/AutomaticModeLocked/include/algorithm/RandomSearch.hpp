#pragma once
#include "optical_element/EPC.hpp"
#include "instrument/VirtualOpticalPowerVisualizer.hpp"
#include "json/ReJson.hpp"
#include <deque>
#include <vector>
#include <functional>
#include <optional>
#include <random>

namespace random_search
{

//epc的设置（单个通道值）
struct EPCSetting
{
    std::string element_name;
    int channel;
    double phase_deg;

    bool operator==(const EPCSetting&) const = default;
};

//反馈函数的输入
struct FeedbackInput
{
    const Eigen::Matrix<double,3,Eigen::Dynamic>& measurement;//仪器的测量结果p_x,p_y,p_total
    std::vector<EPCSetting> epc_settings;//产生本次测量时使用的EPC设置
};

//反馈函数的输出
struct FeedbackOutput
{
    bool is_locked = false;//本次测量历史是否满足锁模判据
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

    void analyze_and_cache(const FeedbackInput &feeback_input);//对原始数据进行下采样然后缓存
    std::optional<FeedbackOutput> try_feedback();//判断是否可以提前反馈，可以时返回反馈结果
    FeedbackOutput feedback();//返回反馈
    bool is_locked() const;//最近连续指定帧数都存在候选脉冲时，判定为锁模
    std::vector<EPCSetting> generate_random_epc_settings();//生成随机EPC设置，保留器件名称和通道号

    //单个下采样的数据
    struct SampleData
    {
        double max_power;
        double avg_power;
    };

    //对FeedbackInput中的数据进行分析从而得到的数据
    struct AnalyzedData
    {
        std::vector<SampleData> sample_datas;//下采样的数据
        std::vector<double> candidate_pulses;//候选脉冲的峰值
        double total_energy = 0;//总能量J
        double energy_concentration;//能量集中度（类比IPR）
    };

    int m_downsample_factor;//算法的下采样分组点数，需要大于0
    double m_data_stride_s;//数据的采样间隔
    double m_foreground_power_ratio;//前景平均功率需要超过背景中位数的倍数
    double m_min_pulse_peak_power_w;//候选脉冲需要超过的最低峰值功率W
    int m_lock_sample_count;//判定锁模需要连续存在候选脉冲的帧数
    double m_min_total_energy_j;//配置中的pJ转换为J；最近判定窗口全部低于此总能量时，提前随机反馈
    int m_feedback_sample_count;//必须进行一次反馈的最大分析帧数
    int m_feedback_check_interval;//每收集多少帧尝试一次提前反馈
    int m_max_data_size;//最大m_analyzed_datas的长度
    int m_samples_since_feedback = 0;//当前EPC设置下，距上次反馈收集的帧数
    std::mt19937_64 m_random_engine;//随机引擎，连续反馈时保留随机序列状态
    std::vector<EPCSetting> m_last_epc_settings;//上次EPC的值
    std::deque<AnalyzedData> m_analyzed_datas;//按时间顺序缓存分析结果，队首为最旧数据


};


}//namespace random_search
