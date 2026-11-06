#include "construction/Builder.hpp"
#include "UI/UIManager.hpp"
#include <filesystem>
#include <string>
#include <thread>

int main()
{

    Channel<UIMessage> ui_channel;//UI->计算的通道
    Channel<CavityMessage> cavity_channel;//计算->UI的通道
    
    const std::string config_path = (std::filesystem::path(__FILE__).parent_path() / "config/config.json").string();//配置文件
    Builder builder(config_path);//腔构造器
    
    OpticalCavity cavity = builder.build(cavity_channel,ui_channel);//腔
    UIManager ui(config_path,ui_channel,cavity_channel);//UI

    //UI先完成订阅和窗口创建，再启动计算线程，避免丢失初始画面。
    std::thread calculation_thread(&OpticalCavity::run,&cavity);
    ui.run();
    calculation_thread.join();

    return 0;
}
