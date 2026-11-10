#include "algorithm/RandomSearch.hpp"
#include <algorithm>
#include <cstdint>
#include <utility>

namespace random_search
{

RandomSearch::RandomSearch(const ReJson& config)
    : m_downsample_factor(static_cast<int>(config.config_["random_search"]["downsample_factor"])),
      m_data_stride_s(static_cast<double>(config.config_["basic_config"]["init_gauss_optical_field"]["sample_stride_fs"]) * 1.0e-15),
      m_foreground_power_ratio(static_cast<double>(config.config_["random_search"]["foreground_power_ratio"])),
      m_min_pulse_peak_power_w(static_cast<double>(config.config_["random_search"]["min_pulse_peak_power_w"])),
      m_lock_sample_count(static_cast<int>(config.config_["random_search"]["lock_sample_count"])),
      m_min_total_energy_j(static_cast<double>(config.config_["random_search"]["min_total_energy_pj"]) * 1.0e-12),
      m_feedback_sample_count(static_cast<int>(config.config_["random_search"]["feedback_sample_count"])),
      m_feedback_check_interval(static_cast<int>(config.config_["random_search"]["feedback_check_interval"])),
      m_max_data_size(static_cast<int>(config.config_["random_search"]["max_data_size"])),
      m_random_engine(static_cast<std::uint64_t>(static_cast<double>(config.config_["random_search"]["random_seed"])))
{
}

std::optional<FeedbackOutput> RandomSearch::feedback(const FeedbackInput &feeback_input)
{
    //首先比对上次和这次的EPC值是否是相同的
    if (m_last_epc_settings != feeback_input.epc_settings)
    {
        //如果不相同(说明被手动调了或者上次进行了反馈)
        m_analyzed_datas.clear();//清空m_analyzed_datas
        m_samples_since_feedback = 0;
    }

    //分析并更新数据
    analyze_and_cache(feeback_input);
    m_last_epc_settings = feeback_input.epc_settings;
    m_samples_since_feedback++;

    //按间隔尝试提前反馈，没有结果时继续收集数据。
    if (m_samples_since_feedback % m_feedback_check_interval == 0)
    {
        std::optional<FeedbackOutput> feedback_output = try_feedback();
        if (feedback_output.has_value())
        {
            m_analyzed_datas.clear();
            m_samples_since_feedback = 0;
            return feedback_output;
        }
    }

    //判断是否达到了必须要进行反馈的反馈的条件
    if (m_samples_since_feedback >= m_feedback_sample_count)
    {
        // 至少进行一次反馈
        FeedbackOutput feedback_output = feedback();
        m_analyzed_datas.clear();
        m_samples_since_feedback = 0;
        return feedback_output;
    }


    //运行到此处说明无法进行反馈
    return {};

}

void RandomSearch::analyze_and_cache(const FeedbackInput &feeback_input)
{
    //遍历measurement的合功率
    const Eigen::Matrix<double,3,Eigen::Dynamic> &measurement = feeback_input.measurement;
    const Eigen::Index sample_size = measurement.cols();
    const Eigen::Index downsample_factor = m_downsample_factor;
    AnalyzedData analyzed_data{};
    analyzed_data.sample_datas.reserve(static_cast<size_t>((sample_size + downsample_factor - 1) / downsample_factor));

    //同时按照m_downsample_factor采样得到std::vector<SampleData>,同时计算total_energy
    for (Eigen::Index begin = 0; begin < sample_size; begin += downsample_factor)
    {
        const Eigen::Index end = std::min(begin + downsample_factor,sample_size);
        SampleData sample_data{0.0,0.0};
        double power_sum = 0.0;
        for (Eigen::Index column = begin; column < end; ++column)
        {
            const double power = measurement(2,column);
            sample_data.max_power = std::max(sample_data.max_power,power);
            power_sum += power;
        }
        sample_data.avg_power = power_sum / static_cast<double>(end - begin);
        analyzed_data.total_energy += power_sum * m_data_stride_s;//W*s得到总能量J。
        analyzed_data.sample_datas.push_back(sample_data);
    }

    //用下采样的数据计算energy_concentration
    double power_sum = 0.0;
    double squared_power_sum = 0.0;
    for (size_t index = 0; index < analyzed_data.sample_datas.size(); ++index)
    {
        const SampleData &sample_data = analyzed_data.sample_datas[index];
        //按每段实际包含的点数加权，最后不足一段时仍保持正确的时间占比。
        const Eigen::Index sample_count = std::min(downsample_factor,sample_size - static_cast<Eigen::Index>(index) * downsample_factor);
        power_sum += sample_data.avg_power * static_cast<double>(sample_count);
        squared_power_sum += sample_data.avg_power * sample_data.avg_power * static_cast<double>(sample_count);
    }
    //C=N*sum(P^2)/sum(P)^2，P取分段平均功率；均匀分布为1，无能量时保留0。
    if (power_sum > 0.0)
    {
        analyzed_data.energy_concentration = static_cast<double>(sample_size) * squared_power_sum / (power_sum * power_sum);
    }

    //用平均功率的中位数估计背景，在副本上计算，保留原数据的时间顺序。
    if (!analyzed_data.sample_datas.empty())
    {
        std::vector<double> average_powers;
        average_powers.reserve(analyzed_data.sample_datas.size());
        for (const SampleData &sample_data : analyzed_data.sample_datas)
        {
            average_powers.push_back(sample_data.avg_power);
        }
        const size_t middle_index = average_powers.size() / 2;
        const std::vector<double>::iterator middle = average_powers.begin() + static_cast<std::vector<double>::difference_type>(middle_index);
        std::nth_element(average_powers.begin(),middle,average_powers.end());
        double background_power = *middle;
        if (average_powers.size() % 2 == 0)
        {
            background_power = (*std::max_element(average_powers.begin(),middle) + *middle) / 2.0;
        }
        const double foreground_threshold = background_power * m_foreground_power_ratio;

        //连续的前景分组构成一个区域，每个区域只保留最大的峰值功率。
        std::vector<double> region_peak_powers;
        double region_peak_power = 0.0;
        bool in_foreground = false;
        for (const SampleData &sample_data : analyzed_data.sample_datas)
        {
            if (sample_data.avg_power > foreground_threshold)
            {
                region_peak_power = std::max(region_peak_power,sample_data.max_power);
                in_foreground = true;
            }
            else if (in_foreground)
            {
                region_peak_powers.push_back(region_peak_power);
                region_peak_power = 0.0;
                in_foreground = false;
            }
        }
        if (in_foreground)
        {
            region_peak_powers.push_back(region_peak_power);
        }

        //FFT时间窗口首尾相邻，合并跨窗口边界的同一前景区域。
        if (region_peak_powers.size() > 1
            && analyzed_data.sample_datas.front().avg_power > foreground_threshold
            && analyzed_data.sample_datas.back().avg_power > foreground_threshold)
        {
            region_peak_powers.front() = std::max(region_peak_powers.front(),region_peak_powers.back());
            region_peak_powers.pop_back();
        }

        //区域峰值超过最低功率时，才保存为有效的候选脉冲。
        for (double peak_power : region_peak_powers)
        {
            if (peak_power > m_min_pulse_peak_power_w)
            {
                analyzed_data.candidate_pulses.push_back(peak_power);
            }
        }
    }

    //缓存已满时移除最旧的一帧，再保存本次分析结果。
    if (m_analyzed_datas.size() >= static_cast<size_t>(m_max_data_size))
    {
        m_analyzed_datas.pop_front();
    }
    m_analyzed_datas.push_back(std::move(analyzed_data));
}

std::optional<FeedbackOutput> RandomSearch::try_feedback()
{
    //历史不足时继续收集；最近指定帧的总能量都偏低时，提前返回随机设置。
    if (m_analyzed_datas.size() < static_cast<size_t>(m_lock_sample_count))
    {
        return {};
    }
    if (std::all_of(m_analyzed_datas.cend() - m_lock_sample_count,m_analyzed_datas.cend(),
                    [this](const AnalyzedData &analyzed_data)
                    {
                        return analyzed_data.total_energy < m_min_total_energy_j;
                    }))
    {
        return FeedbackOutput{false,generate_random_epc_settings()};
    }

    if (is_locked())
    {
        return FeedbackOutput{true,generate_random_epc_settings()};
    }
    return {};
}

FeedbackOutput RandomSearch::feedback()
{
    return FeedbackOutput{is_locked(),generate_random_epc_settings()};
}

bool RandomSearch::is_locked() const
{
    if (m_analyzed_datas.size() < static_cast<size_t>(m_lock_sample_count))
    {
        return false;
    }
    return std::all_of(m_analyzed_datas.cend() - m_lock_sample_count,m_analyzed_datas.cend(),
                       [](const AnalyzedData &analyzed_data)
                       {
                           return !analyzed_data.candidate_pulses.empty();
                       });
}

std::vector<EPCSetting> RandomSearch::generate_random_epc_settings()
{
    //保留器件名称和通道号，为每个通道独立生成[0,360)范围内的随机相位。
    std::vector<EPCSetting> epc_settings = m_last_epc_settings;
    std::uniform_real_distribution<double> phase_distribution(0.0,360.0);
    for (EPCSetting &setting : epc_settings)
    {
        setting.phase_deg = phase_distribution(m_random_engine);
    }
    return epc_settings;
}

}//namespace random_search
