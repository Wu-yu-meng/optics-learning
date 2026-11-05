#pragma once
#include "communication/OpticalField.hpp"

//可视化管理器，负责调用可视化
//分为两个部分，一个是仿真仪器的可视化，一个是虚拟仪器的可视化
class UIManager
{
public:
    //进行所有的可视化更新。需要如下的数据
    //光场数据
    void update(size_t loop_count,const OpticalField &optical_field);
private:

};
