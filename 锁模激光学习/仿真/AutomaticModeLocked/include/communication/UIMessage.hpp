#pragma once

class OpticalCavity;//前向声明

//UI发出的消息基类
class UIMessage
{
public:
    virtual ~UIMessage();//虚析构函数，支持通过基类指针释放消息
    virtual void apply(OpticalCavity &cavity) const = 0;//由计算线程执行指令
};
