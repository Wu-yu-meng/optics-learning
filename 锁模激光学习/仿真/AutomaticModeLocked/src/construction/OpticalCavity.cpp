#include "construction/OpticalCavity.hpp"
#include <functional>
#include <utility>

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
