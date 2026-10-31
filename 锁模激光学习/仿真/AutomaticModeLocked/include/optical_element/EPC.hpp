#pragma once
#include <vector>
#include "OpticalElement.hpp"


//等效为四个独立可调拨片
//问题在于，不清楚每个拨片的等效可调节的相位差距，是否可以等效为都是2pi，即单片拨片即可等效4个拨片
class EPC : public OpticalElement
{
public:
    //相位延迟器的
    struct Retarder
    {
        int ID;//编号，从0开始，越小越先作用
        double orientation_deg;//主轴相对于标准坐标系的x轴的角度（角度制）
        double delta_phase_deg;//fi(y) - fi(x),相位差
    };

    EPC(const std::vector<Retarder> &retarders);
    OpticalOutputs propagate(const OpticalField& input_optical_field) override;//EPC的传播函数

    //修改EPC某个通道的偏振量
    void set_retarder(int ID,double delta_phase_deg);
private:
    std::vector<Retarder> m_retarders;//线性延迟器队列(从ID0到大排序)
    Eigen::Matrix<std::complex<double>,2,2> total_jones_matrix() const;//总的传播矩阵
};
