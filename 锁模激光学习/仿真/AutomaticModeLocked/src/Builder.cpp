#include "Builder.hpp"
#include "optical_element/SMF.hpp"
#include "optical_element/EDF.hpp"
#include "optical_element/EPC.hpp"
#include "optical_element/OC.hpp"
#include "optical_element/PD.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <regex>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace
{
    //配置错误在构建时给出字段位置，传播循环只负责数值计算。
    double number(const cv::FileNode& node, const std::string& key, const std::string& path,
                  double minimum = -std::numeric_limits<double>::infinity(),
                  double maximum = std::numeric_limits<double>::infinity(), bool positive = false)
    {
        const cv::FileNode value = node[key];
        if (value.empty() || (!value.isInt() && !value.isReal()))
            throw std::invalid_argument(path + "." + key + "：需要数值");
        const double result = value.real();
        if (!std::isfinite(result) || result < minimum || result > maximum || (positive && result <= 0.0))
            throw std::invalid_argument(path + "." + key + "：数值超出允许范围");
        return result;
    }

    size_t integer(const cv::FileNode& node, const std::string& key, const std::string& path,
                   double minimum, double maximum)
    {
        const double result = number(node,key,path,minimum,maximum);
        if (std::floor(result) != result)
            throw std::invalid_argument(path + "." + key + "：需要整数");
        return static_cast<size_t>(result);
    }

    std::string text(const cv::FileNode& node, const std::string& key, const std::string& path)
    {
        const cv::FileNode value = node[key];
        if (!value.isString() || value.string().empty())
            throw std::invalid_argument(path + "." + key + "：需要非空字符串");
        return value.string();
    }
}

OpticalCavity::OpticalCavity(OpticalField initial_optical_field)
    : m_initial_optical_field(std::move(initial_optical_field))
{
}

void OpticalCavity::reset()
{
    m_loop = 0;
    m_optical_field_map.clear();
    //默认一开始均为初始光场
    for (const Element &element : m_elements)
    {
        for (const std::pair<const input_port_name,g_output_port_name> &connection : element.connections)
            m_optical_field_map.emplace(connection.second,m_initial_optical_field);
        EDF *edf = dynamic_cast<EDF*>(element.element_ptr.get());
        if (edf != nullptr)
            edf->reset_noise();
    }
    //未连接到下一器件的输出端口也可在启动画面中观测。
    for (const Observation &observation : m_observations)
        m_optical_field_map.emplace(observation.source,m_initial_optical_field);
}

//腔内光运行一次
void OpticalCavity::run_once()
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

size_t OpticalCavity::loop_count() const
{
    return m_loop;
}

const OpticalField& OpticalCavity::field(const std::string& output_port_name) const
{
    return m_optical_field_map.at(output_port_name);
}

OpticalElement& OpticalCavity::element(const std::string& element_name)
{
    for (const Element &element : m_elements)
        if (element.my_name == element_name)
            return *element.element_ptr;
    throw std::out_of_range("不存在器件：" + element_name);
}

const std::vector<OpticalCavity::Observation>& OpticalCavity::observations() const
{
    return m_observations;
}

const std::vector<std::string>& OpticalCavity::epc_names() const
{
    return m_epc_names;
}

const OpticalCavity::Runtime& OpticalCavity::runtime() const
{
    return m_runtime;
}

void OpticalCavity::describe() const
{
    std::cout << "Model: ordered_round_trip (no physical delay synchronization)\n";
    for (const Element &element : m_elements)
        for (const std::pair<const input_port_name,g_output_port_name> &connection : element.connections)
            std::cout << "  " << element.my_name << "." << connection.first << " <- " << connection.second << '\n';
    for (const Observation &observation : m_observations)
        std::cout << "  Observe: " << observation.name << " <- " << observation.source << '\n';
    std::cout << "Grid: " << m_initial_optical_field.basic_info().sample_size << " samples, "
              << m_initial_optical_field.basic_info().sample_stride << " fs/sample\n";
}
Builder::Builder(ReJson &&config)
    : m_config(std::move(config))
{
}

OpticalCavity Builder::build()
{
    const cv::FileStorage &config = m_config.config_;
    if (!config.isOpened())
        throw std::invalid_argument("无法打开配置文件");
    if (static_cast<int>(config["version"]) != 1)
        throw std::invalid_argument("version：需要版本1的腔配置，请使用配置编辑器导出");

    const cv::FileNode constants = config["scientific_constants"];
    number(constants,"light_speed","scientific_constants",0.0,std::numeric_limits<double>::infinity(),true);
    const cv::FileNode gaussian = config["init_gauss_optical_field"];
    number(gaussian,"wave_length_nm","init_gauss_optical_field",0.0,std::numeric_limits<double>::infinity(),true);
    number(gaussian,"sample_stride_fs","init_gauss_optical_field",0.0,std::numeric_limits<double>::infinity(),true);
    integer(gaussian,"sample_size","init_gauss_optical_field",2,1048576);
    number(gaussian,"fwhm_time_fs","init_gauss_optical_field",0.0,std::numeric_limits<double>::infinity(),true);
    number(gaussian,"peak_power_w","init_gauss_optical_field",0.0);
    number(gaussian,"polarization_angle_deg","init_gauss_optical_field");
    number(gaussian,"relative_phase_deg","init_gauss_optical_field");
    OpticalCavity cavity(init_gauss_optical_field(config));

    const cv::FileNode elements = config["elements"];
    if (!elements.isSeq() || elements.empty())
        throw std::invalid_argument("elements：需要非空的器件数组，数组顺序就是执行顺序");
    std::unordered_set<std::string> names;
    std::unordered_set<std::string> output_ports;
    size_t index = 0;
    for (const cv::FileNode &node : elements)
    {
        const std::string path = "elements[" + std::to_string(index++) + "]";
        const std::string name = text(node,"name",path);
        const std::string type = text(node,"type",path);
        if (!std::regex_match(name,std::regex("[A-Za-z][A-Za-z0-9_]*")) || !names.emplace(name).second)
            throw std::invalid_argument(path + ".name：需要唯一的英文字母开头的字母、数字或下划线名称");
        const cv::FileNode parameters = node["parameters"];
        const std::string parameter_path = path + ".parameters";
        std::unique_ptr<OpticalElement> element;

        if (type == "SMF")
        {
            const double gamma = number(parameters,"gamma_per_w_m",parameter_path,0.0);
            const double length = number(parameters,"length_m",parameter_path,0.0);
            const double beta2 = number(parameters,"beta2_ps2_per_km",parameter_path);
            const double step = number(parameters,"step_length_m",parameter_path,0.0,std::numeric_limits<double>::infinity(),true);
            element = std::make_unique<SMF>(gamma,length,beta2,step);
        }
        else if (type == "EDF")
        {
            const double gain = number(parameters,"g_0_per_m",parameter_path,0.0);
            const double energy = number(parameters,"saturation_energy_pj",parameter_path,0.0,std::numeric_limits<double>::infinity(),true);
            const double length = number(parameters,"length_m",parameter_path,0.0);
            const double gain_bandwidth = number(parameters,"gain_bandwidth_thz",parameter_path,0.0,std::numeric_limits<double>::infinity(),true);
            const double noise_energy = number(parameters,"noise_energy_pj_per_pass",parameter_path,0.0);
            const std::uint64_t seed = integer(parameters,"random_seed",parameter_path,0,9007199254740991.0);
            const double noise_bandwidth = number(parameters,"noise_bandwidth_thz",parameter_path,0.0,std::numeric_limits<double>::infinity(),true);
            element = std::make_unique<EDF>(gain,energy,length,gain_bandwidth,noise_energy,seed,noise_bandwidth);
        }
        else if (type == "EPC")
        {
            const cv::FileNode retarders = parameters["retarders"];
            if (!retarders.isSeq() || retarders.empty())
                throw std::invalid_argument(parameter_path + ".retarders：需要非空的延迟器数组");
            std::vector<EPC::Retarder> values;
            for (const cv::FileNode &retarder : retarders)
            {
                const std::string retarder_path = parameter_path + ".retarders[" + std::to_string(values.size()) + "]";
                const int id = static_cast<int>(integer(retarder,"ID",retarder_path,0,1023));
                const double orientation = number(retarder,"orientation_deg",retarder_path);
                const double phase = number(retarder,"delta_phase_deg",retarder_path,0.0,360.0);
                values.push_back(EPC::Retarder{id,orientation,phase});
            }
            element = std::make_unique<EPC>(values);
            cavity.m_epc_names.push_back(name);
        }
        else if (type == "OC")
        {
            const double ratio = number(parameters,"cavity_ratio",parameter_path,0.0,1.0);
            const double transmission = number(parameters,"power_transmittance",parameter_path,0.0,1.0);
            element = std::make_unique<OC>(ratio,transmission);
        }
        else if (type == "PD")
            element = std::make_unique<PD>(number(parameters,"transmission_axis_angle_deg",parameter_path));
        else
            throw std::invalid_argument(path + ".type：不支持的器件类型 " + type);

        OpticalCavity::Element entry;
        entry.my_name = name;
        entry.element_ptr = std::move(element);
        const cv::FileNode connections = node["connections"];
        if (!connections.isMap() || connections.size() != 1 || !connections["in"].isString())
            throw std::invalid_argument(path + ".connections：当前五类器件均需要一个名为in的输入");
        entry.connections.emplace("in",text(connections,"in",path + ".connections"));
        cavity.m_elements.push_back(std::move(entry));
        if (type == "OC")
        {
            output_ports.emplace(name + "-cavity");
            output_ports.emplace(name + "-extracted");
        }
        else
            output_ports.emplace(name + "-out");
    }
    for (const OpticalCavity::Element &element : cavity.m_elements)
        for (const std::pair<const std::string,std::string> &connection : element.connections)
            if (!output_ports.contains(connection.second))
                throw std::invalid_argument(element.my_name + ".connections." + connection.first + "：不存在输出端口 " + connection.second);

    const cv::FileNode observations = config["observations"];
    if (!observations.isSeq())
        throw std::invalid_argument("observations：需要数组，允许为空");
    std::unordered_set<std::string> observation_names;
    for (const cv::FileNode &node : observations)
    {
        const std::string name = text(node,"name","observations");
        const std::string source = text(node,"source","observations");
        if (text(node,"type","observations") != "VirtualOpticalPowerVisualizer")
            throw std::invalid_argument("observations.type：当前支持VirtualOpticalPowerVisualizer");
        if (!output_ports.contains(source) || !observation_names.emplace(name).second)
            throw std::invalid_argument("observations：观测端口不存在或仪器名称重复：" + name);
        cavity.m_observations.push_back(OpticalCavity::Observation{name,source});
    }

    const cv::FileNode runtime = config["runtime"];
    if (!runtime.empty())
    {
        cavity.m_runtime.start_paused = integer(runtime,"start_paused","runtime",0,1) != 0;
        cavity.m_runtime.frame_delay_ms = static_cast<int>(integer(runtime,"frame_delay_ms","runtime",1,60000));
        cavity.m_runtime.epc_controls = integer(runtime,"epc_controls","runtime",0,1) != 0;
        cavity.m_runtime.render_every_n_rounds = integer(runtime,"render_every_n_rounds","runtime",1,1000000);
        cavity.m_runtime.max_rounds = integer(runtime,"max_rounds","runtime",0,9007199254740991.0);
    }
    cavity.reset();
    return cavity;
}
