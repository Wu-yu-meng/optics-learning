#include "construction/Builder.hpp"
#include "construction/InitOpticalField.hpp"
#include "optical_element/SMF.hpp"
#include "optical_element/EDF.hpp"
#include "optical_element/EPC.hpp"
#include "optical_element/OC.hpp"
#include "optical_element/PD.hpp"
#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

Builder::Builder(ReJson &&config)
    : m_config(std::move(config))
{
}

OpticalCavity Builder::build(Channel<CavityMessage> &cavity_channel, Channel<UIMessage> &ui_channel)
{
    //读取配置文件中的参数。构建element
    const cv::FileNode elements_config = m_config.config_["elements"];
    const cv::FileNode basic_config = m_config.config_["basic_config"];
    const std::string start_element = static_cast<std::string>(m_config.config_["start_element"]);
    std::vector<OpticalCavity::Element> elements;
    std::unordered_map<std::string,size_t> element_indices;//器件名称对应的编号
    std::unordered_map<std::string,size_t> output_indices;//完整输出端口名对应的器件编号
    
    //构造器件
    for (const cv::FileNode &element_config : elements_config)
    {
        OpticalCavity::Element element;
        element.my_name = static_cast<std::string>(element_config["name"]);
        const std::string type = static_cast<std::string>(element_config["type"]);
        const cv::FileNode parameters = element_config["parameters"];
        const size_t element_index = elements.size();
        if (!element_indices.emplace(element.my_name,element_index).second)
        {
            throw std::invalid_argument("[Builder]器件名称重复：" + element.my_name);
        }

        //根据器件类型读取各自的构造参数
        if (type == "SMF")
        {
            element.element_ptr = std::make_unique<SMF>(
                static_cast<double>(parameters["gamma_per_w_m"]),
                static_cast<double>(parameters["length_m"]),
                static_cast<double>(parameters["beta2_ps2_per_km"]),
                static_cast<double>(basic_config["SMF"]["step_length_m"]));
        }
        else if (type == "EDF")
        {
            element.element_ptr = std::make_unique<EDF>(
                static_cast<double>(parameters["g_0_per_m"]),
                static_cast<double>(parameters["saturation_energy_pj"]),
                static_cast<double>(parameters["length_m"]),
                static_cast<double>(parameters["gain_bandwidth_thz"]),
                static_cast<double>(parameters["noise_energy_pj_per_pass"]),
                static_cast<std::uint64_t>(static_cast<double>(parameters["random_seed"])),
                static_cast<double>(parameters["noise_bandwidth_thz"]));
        }
        else if (type == "EPC")
        {
            std::vector<EPC::Retarder> retarders;
            for (const cv::FileNode &retarder_config : parameters["retarders"])
            {
                retarders.push_back({
                    static_cast<int>(retarder_config["ID"]),
                    static_cast<double>(retarder_config["orientation_deg"]),
                    static_cast<double>(retarder_config["delta_phase_deg"])
                });
            }
            element.element_ptr = std::make_unique<EPC>(retarders);
        }
        else if (type == "OC")
        {
            element.element_ptr = std::make_unique<OC>(
                static_cast<double>(parameters["cavity_ratio"]),
                static_cast<double>(parameters["power_transmittance"]));
        }
        else if (type == "PD")
        {
            element.element_ptr = std::make_unique<PD>(static_cast<double>(parameters["transmission_axis_angle_deg"]));
        }
        else
        {
            throw std::invalid_argument("[Builder]未知的器件类型：" + type);
        }

        //保存全部连接，反馈连接也保留在其中
        for (const cv::FileNode &connection : element_config["connections"])
        {
            element.connections.emplace(connection.name(),static_cast<std::string>(connection));
        }

        //记录器件实际提供的输出端口，OC有腔内和提取两个输出
        if (type == "OC")
        {
            output_indices.emplace(element.my_name + "-cavity",element_index);
            output_indices.emplace(element.my_name + "-extracted",element_index);
        }
        else
        {
            output_indices.emplace(element.my_name + "-out",element_index);
        }
        elements.push_back(std::move(element));
    }

    //判断连接顺序是否正确
    //先读取全部器件，再检查连接；允许前面的器件引用后面的器件输出
    std::vector<std::vector<size_t>> next_elements(elements.size());
    for (size_t index = 0; index < elements.size(); ++index)
    {
        const OpticalCavity::Element &element = elements[index];
        if (element.connections.size() != 1 || !element.connections.contains("in"))
        {
            throw std::invalid_argument("[Builder]当前器件需要一个in输入端口：" + element.my_name);
        }
        for (const std::pair<const std::string,std::string> &connection : element.connections)
        {
            const std::unordered_map<std::string,size_t>::const_iterator source = output_indices.find(connection.second);
            if (source == output_indices.end())
            {
                throw std::invalid_argument("[Builder]连接引用了不存在的输出端口：" + connection.second);
            }
            next_elements[source->second].push_back(index);
        }
    }

    //判断顺序，放入OpticalCavity
    //按名称固定分支遍历顺序，避免配置数组重新排列后改变反馈边的选择
    std::vector<size_t> traversal_order;
    for (size_t index = 0; index < elements.size(); ++index)
    {
        traversal_order.push_back(index);
        std::sort(next_elements[index].begin(),next_elements[index].end(),
                  [&elements](size_t left, size_t right)
                  {
                      return elements[left].my_name < elements[right].my_name;
                  });
    }
    std::sort(traversal_order.begin(),traversal_order.end(),
              [&elements](size_t left, size_t right)
              {
                  return elements[left].my_name < elements[right].my_name;
              });

    //0表示未访问，1表示正在当前路径中访问，2表示访问完成
    std::vector<int> visit_states(elements.size(),0);
    std::vector<size_t> post_order;
    const std::function<void(size_t)> visit = [&](size_t index)
    {
        visit_states[index] = 1;
        for (size_t next_index : next_elements[index])
        {
            //回到当前路径的边是反馈边：排序时跳过，运行时读取上一圈缓存
            if (visit_states[next_index] == 1)
            {
                continue;
            }
            if (visit_states[next_index] == 0)
            {
                visit(next_index);
            }
        }
        visit_states[index] = 2;
        post_order.push_back(index);
    };
    visit(element_indices.at(start_element));
    for (size_t index : traversal_order)
    {
        if (visit_states[index] == 0)
        {
            visit(index);
        }
    }

    //逆后序是忽略反馈边后的拓扑顺序，单环从start_element开始执行
    OpticalCavity cavity(cavity_channel,ui_channel);
    for (std::vector<size_t>::const_reverse_iterator index = post_order.rbegin(); index != post_order.rend(); ++index)
    {
        cavity.m_elements.push_back(std::move(elements[*index]));
    }

    //默认一开始均为初始光场，使用本次配置中的basic_config
    OpticalField initial_optical_field = init_gauss_optical_field(basic_config);
    for (const OpticalCavity::Element &element : cavity.m_elements)
    {
        for (const std::pair<const std::string,std::string> &connection : element.connections)
        {
            cavity.m_optical_field_map.emplace(connection.second,initial_optical_field);
        }
    }

    return cavity;
}
