#include "OpticalField.hpp"
#include "OpticalFieldVisualization.hpp"
#include "VirtualOscilloscope.hpp"
#include "OC.hpp"
#include "PD.hpp"
#include "EDF.hpp"
#include "EPC.hpp"
#include "SMF.hpp"
#include <cstddef>
#include <exception>
#include <iostream>
#include <utility>

//消融测试：保留可视化、EDF带宽/噪声及固定拨片，本轮恢复SMF对称分步。
int main()
{
    try
    {
        OC oc(0.9, 0.98);
        PD pd(45.0);
        //保留已对齐的参数、4 THz有限增益带宽和每圈1e-6 pJ噪声，使用固定种子。
        EDF edf(0.4, 100.0, 1.0, 4.0, 1.0e-6, 20261018);
        //恢复SMF对称分步：目标单步非线性相移0.05 rad，最多256段；其余消融配置不变。
        SMF smf(1.3e-2, 5.0, -21.7);
        EPC epc({
            {0, 0.0, 0.0},
            {1, 45.0, 0.0},
            {2, 0.0, 0.0},
            {3, 45.0, 0.0},
        });

        //只加回test的固定虚拟四分之一波片：主轴22.5°，相位延迟90°。
        //放在PD之后、SMF之前，将线偏振变为椭圆偏振；不受四个EPC滑块控制。
        const EPC smf_input_retarder({
            {0, 22.5, 90.0},
        });

        //观测只读取OC输出，不改变腔内光场或推进EDF随机数状态。
        VirtualOscilloscope oscilloscope(0.9, 50.0, 200.0, 30.0);
        OpticalFieldVisualization visualization(4);
        OpticalField optical_field = init_gauss_optical_field();
        std::size_t loop_count = 0;

        //第0圈尚未经过EDF/EPC，只预览初始场按OC分光比例得到的信号。
        OpticalField oc_extracted_field = oc.propagate(optical_field).extracted;
        auto oscilloscope_signal = oscilloscope.detect(oc_extracted_field);

        while (true)
        {
            //暂停、AUTO和调滑块只刷新观测，不额外推进传播或生成噪声。
            const auto command = visualization.update(optical_field, oscilloscope_signal, loop_count);
            if (command.should_exit)
                break;

            //四个滑块只设置下一圈的EPC相位。
            for (std::size_t index = 0; index < command.epc_delta_phase_deg.size(); ++index)
                epc.set_retarder(static_cast<int>(index), command.epc_delta_phase_deg[index]);

            if (command.should_reset)
            {
                optical_field = init_gauss_optical_field();
                edf.reset_noise();
                oc_extracted_field = oc.propagate(optical_field).extracted;
                oscilloscope_signal = oscilloscope.detect(oc_extracted_field);
                loop_count = 0;
                continue;
            }
            if (!command.should_propagate)
                continue;

            //其余器件顺序不变，只在PD与SMF之间加入固定拨片。
            OpticalField edf_output = edf.propagate(optical_field);
            OpticalField epc_output = epc.propagate(edf_output);
            OC::OutputField oc_output = oc.propagate(epc_output);
            OpticalField pd_output = pd.propagate(oc_output.cavity);
            OpticalField smf_input = smf_input_retarder.propagate(pd_output);
            optical_field = smf.propagate(smf_input);

            //示波器缓存本圈实际的OC输出；腔内功率和光谱的观测位置是SMF之后。
            oc_extracted_field = std::move(oc_output.extracted);
            oscilloscope_signal = oscilloscope.detect(oc_extracted_field);
            ++loop_count;
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
