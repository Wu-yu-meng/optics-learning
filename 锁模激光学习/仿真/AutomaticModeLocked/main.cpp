#include "OpticalField.hpp"
#include "OC.hpp"
#include "PD.hpp"

//目前想法是分为如下的部分进行仿真：
//WDM：不仿真
//EDF：模拟饱和增益
//OC：不太确定，是否直接认为减少10%的光强即可
//PC：当作一个4输入影响偏振的系统，即设计一个含有4个参数的2x2矩阵作用于琼斯矢量
//PD-ISO：按照偏振来抑制光强
//SMF：固定损耗，双折射，Kerr非线性，不考虑二阶色散，考虑这些来生成矩阵


//全流程应该是：
//1. 选取一个初始化方式（高斯，随机，或者其他），和一个窗口时间，在窗口内按照步长进行采样得到初始的光场序列
//2. 按照顺序依次通过EDF，OC，PC，PD-ISO，SMF。依次循环

int main()
{

    OpticalField init_optical_field =  init_gauss_optical_field();
    OC oc(0.9,0.98);
    PD pd(45.0);
    OC::OutputField oc_output = oc.propagate(init_optical_field);
    OpticalField pd_output = pd.propagate(oc_output.cavity);
    
    return 0;

}