#pragma once
#include "json/ReJson.hpp"
#include "construction/OpticalCavity.hpp"

//读取配置文件构建激光腔
class Builder
{
public:
    Builder() = delete;
    explicit Builder(const std::string &config_path);

    OpticalCavity build(Channel<CavityMessage> &cavity_channel, Channel<UIMessage> &ui_channel);//创建激光腔
private:
    ReJson m_config;
};
