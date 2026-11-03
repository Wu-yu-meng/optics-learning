#pragma once
#include "config/config.hpp"
#include "construction/OpticalCavity.hpp"

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
