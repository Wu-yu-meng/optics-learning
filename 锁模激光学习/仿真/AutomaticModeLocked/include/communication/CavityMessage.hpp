#pragma once

//光学腔发出的消息基类
class CavityMessage
{
public:
    virtual ~CavityMessage();//虚析构函数，支持通过基类指针释放消息
};
