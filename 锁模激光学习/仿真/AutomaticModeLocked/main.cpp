#include "OpticalField.hpp"
#include "OpticalFieldVisualization.hpp"
#include "OC.hpp"
#include "PD.hpp"
#include "EDF.hpp"
#include "EPC.hpp"
#include "SMF.hpp"
#include "Photodetector.hpp"
#include <cstddef>
#include <exception>
#include <iostream>


//目前想法是分为如下的部分进行仿真：
//WDM：不仿真
//EDF：模拟饱和增益与有限带宽ASE噪声
//OC：不太确定，是否直接认为减少10%的光强即可
//EPC：当作一个4输入影响偏振的系统，即设计一个含有4个参数的2x2矩阵作用于琼斯矢量
//PD-ISO：按照偏振来抑制光强
//SMF：当前考虑Kerr非线性和二阶色散，固定损耗与线性双折射待加入


//全流程应该是：
//1. 选取一个初始化方式（高斯，随机，或者其他），和一个窗口时间，在窗口内按照步长进行采样得到初始的光场序列
//2. 按照顺序依次通过EDF，OC，PC，PD-ISO，SMF。依次循环

int main()
{
    try
    {
        OC oc(0.9, 0.98);
        PD pd(45.0);
        //EDF：g0*L=0.4，对应未饱和功率增益exp(0.4)=1.49（约1.74 dB）。
        //每圈加入1e-6 pJ带限ASE噪声，约为1550 nm处数个光子的能量，不会直接产生强脉冲。
        EDF edf(0.01, 100.0, 40.0, 1.0e-6, 4.0, 20261018);

        //标准1550 nm单模光纤的beta2约为-21.7 ps^2/km。
        //对称分步傅里叶法按峰值功率自适应分段：目标单步Kerr相移不超过0.05 rad，最多256步。
        SMF smf(1.3e-2, -21.7, 5.0, 0.05, 256);
        //OC输出先经30 dB光衰减，再由0.9 A/W、50 ohm、50 GHz的探测链路转换为示波器电压。
        Photodetector photodetector(0.9, 50.0, 200.0, 30.0);
        EPC epc({
            {0, 0.0, 0.0},
            {1, 45.0, 0.0},
            {2, 0.0, 0.0},
            {3, 45.0, 0.0},
        });

        //PD输出的是45°线偏振。SMF前加入一个固定的虚拟四分之一波片作为集总等效：
        //波片主轴为22.5°、相位延迟为90°，使入射SMF的光成为椭圆偏振。
        //这样U/V两个圆偏振分量均非零且功率不相等，Kerr效应才能产生
        //随瞬时功率变化的相对相位，之后由下一圈的EPC和PD转化为强度相关透过率。
        EPC smf_input_retarder({
            {0, 22.5, 90.0},
        });

        // 第0圈为初始光场，之后每收到一次运行或单步指令就完成一圈传播。
        OpticalField cavity_field = init_gauss_optical_field();
        std::size_t round_trip = 0;
        OpticalFieldVisualization visualization(4);

        while (true)
        {
            //OC是本圈的第一个器件：extracted用于示波器，cavity继续在腔内传播。
            OC::OutputField oc_output = oc.propagate(cavity_field);
            const Photodetector::VoltageSignal oscilloscope_signal =
                photodetector.detect(oc_output.extracted);
            const OpticalFieldVisualizationCommand command =
                visualization.update(cavity_field, oscilloscope_signal, round_trip);

            if (command.should_exit)
                break;

            // 滑块数值会在下一圈光场经过EPC时生效。
            for (std::size_t index = 0; index < command.epc_delta_phase_deg.size(); ++index)
            {
                epc.set_retarder(
                    static_cast<int>(index),
                    command.epc_delta_phase_deg[index]);
            }

            if (command.should_reset)
            {
                cavity_field = init_gauss_optical_field();
                edf.reset_noise();
                round_trip = 0;
                continue;
            }

            if (!command.should_propagate)
                continue;

            OpticalField epc_output = epc.propagate(oc_output.cavity);
            OpticalField pd_output = pd.propagate(epc_output);
            OpticalField edf_output = edf.propagate(pd_output);
            OpticalField smf_input = smf_input_retarder.propagate(edf_output);
            cavity_field = smf.propagate(smf_input);
            ++round_trip;
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << std::endl;
        return 1;
    }

    return 0;
}
