#include "OpticalField.hpp"
#include "optical_element/OC.hpp"
#include "optical_element/PD.hpp"
#include "optical_element/EDF.hpp"
#include "optical_element/EPC.hpp"
#include "optical_element/SMF.hpp"
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "config/config.hpp"
#include "Visualization.hpp"
#include <opencv2/highgui.hpp>
#include "Builder.hpp"



int main()
{

    OC oc(0.9,0.98);
    PD pd(45.0);
    EDF edf(0.4,100,1.0,4.0,1.0e-6,20261018,10.0);//增益带宽与噪声带宽均4 THz，噪声能量1e-6 pJ/圈，种子不变
    SMF smf(1.3e-2, 5.0,-21.7);
    const std::vector<EPC::Retarder> epc_retarders{
        {0, 0.0, 0.0},
        {1, 45.0, 0.0},
        {2, 0.0, 0.0},
        {3, 45.0, 0.0},
    };
    EPC epc(epc_retarders);

    OpticalField optical_field =  init_gauss_optical_field();//初始化高斯光场
    OpticalField oc_extracted_field =  init_gauss_optical_field();//OC的分光
    size_t loop_count = 0;//腔内循环次数

    VirtualOpticalPowerVisualizer power_visualizer_test("test");
    VirtualOpticalPowerVisualizer power_visualizer_edf("edf");

    //Qt滑动条内部使用Latin-1查找窗口，因此使用英文标识，中文仪器名称只作为显示标题。
    const std::string power_window_id = "OpticalPowerTest";
    cv::namedWindow(power_window_id, cv::WINDOW_NORMAL);
    cv::setWindowTitle(power_window_id, power_visualizer_test.name());

    //每个通道控制对应拨片的相位延迟，范围0~360度，主轴方向保持初始化设置。
    for (const EPC::Retarder &retarder : epc_retarders)
    {
        const std::string trackbar_name = "EPC CH" + std::to_string(retarder.ID + 1) + " phase (deg)";
        cv::createTrackbar(trackbar_name, power_window_id, nullptr, 360);
        cv::setTrackbarPos(trackbar_name, power_window_id, static_cast<int>(retarder.delta_phase_deg));
    }

    bool is_running = false;//启动时暂停，便于观察初始光场。

    //环路顺序：EPC->SMF->EDF->OC->PD，观测并保存PD输出作为下一圈输入。
    while (true)
    {
        
        const int key = cv::waitKey(20);
        if (key == 'q' || key == 'Q' || key == 27)
            break;
        if (key == 'a' || key == 'A')
        {
            power_visualizer_test.auto_size();
            continue;
        }
        if (key == ' ')
            is_running = !is_running;
        const bool single_step = key == 'd' || key == 'D';
        if (single_step)
            is_running = false;
        if (!is_running && !single_step)
            continue;
        
        //本圈传播前读取滑动条；暂停时可以调整，继续播放或单步时生效。
        for (const EPC::Retarder &retarder : epc_retarders)
        {
            const std::string trackbar_name = "EPC CH" + std::to_string(retarder.ID + 1) + " phase (deg)";
            const int delta_phase_deg = cv::getTrackbarPos(trackbar_name, power_window_id);
            epc.set_retarder(retarder.ID, static_cast<double>(delta_phase_deg));
        }

        CONFIG.updateJson();
        OpticalOutputs epc_output = epc.propagate({{"in",std::cref(optical_field)}});
        OpticalOutputs smf_output = smf.propagate({{"in",std::cref(epc_output.at("out"))}});
        OpticalOutputs edf_output = edf.propagate({{"in",std::cref(smf_output.at("out"))}});
        OpticalOutputs oc_output = oc.propagate({{"in",std::cref(edf_output.at("out"))}});
        oc_extracted_field = oc_output.at("extracted");
        OpticalOutputs pd_output = pd.propagate({{"in",std::cref(oc_output.at("cavity"))}});
        optical_field = pd_output.at("out");
        cv::imshow(power_window_id, power_visualizer_test.measure(loop_count, optical_field));

        loop_count++;
    }

    cv::destroyWindow(power_window_id);

    return 0;

}
