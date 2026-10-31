#include "Builder.hpp"
#include <functional>
#include <utility>
#include <functional>

//腔内光运行一次
void OpticalCavity::run_once()
{

    //默认一开始均为初始光场
    if (m_loop == 0)
    {
        OpticalField initial_optical_field = init_gauss_optical_field();
        for (const Element &element : m_elements)
        {
            for (const std::pair<const input_port_name,g_output_port_name> &connection : element.connections)
            {
                m_optical_field_map.emplace(connection.second,initial_optical_field);
            }
        }
    }
    
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



Builder::Builder(ReJson &&config)
    : m_config(std::move(config))
{
}

OpticalCavity Builder::build()
{
    return OpticalCavity();
}
