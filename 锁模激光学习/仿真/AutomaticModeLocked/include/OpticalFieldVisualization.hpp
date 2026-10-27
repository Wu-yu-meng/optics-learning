#pragma once

#include "OpticalField.hpp"
#include "VirtualOscilloscope.hpp"

#include <cstddef>
#include <string>
#include <vector>

// 可视化界面在一次更新后返回给仿真主循环的控制指令。
struct OpticalFieldVisualizationCommand
{
    bool should_propagate = false;//是否计算下一圈
    bool should_reset = false;//是否恢复初始光场
    bool should_exit = false;//是否退出程序
    std::vector<double> epc_delta_phase_deg;//EPC各拨片在下一圈采用的相位差
};

// 实时显示当前谐振腔内部的光场，并提供播放、单步、重置和EPC调节控件。
class OpticalFieldVisualization
{
public:
    explicit OpticalFieldVisualization(std::size_t epc_retarder_count);
    ~OpticalFieldVisualization();

    OpticalFieldVisualization(const OpticalFieldVisualization&) = delete;
    OpticalFieldVisualization& operator=(const OpticalFieldVisualization&) = delete;

    OpticalFieldVisualizationCommand update(
        const OpticalField& optical_field,
        const VirtualOscilloscope::VoltageSignal& oscilloscope_signal,
        std::size_t round_trip);

private:
    static void on_auto_scale(int state, void* user_data);

    bool m_is_running = false;
    bool m_window_created = false;
    bool m_scope_window_created = false;
    bool m_spectrum_window_created = false;
    bool m_boundary_warning_latched = false;
    bool m_auto_scale_requested = true;
    double m_cavity_y_axis_max_w = 1.0;
    double m_scope_y_axis_max_mv = 1.0;
    int m_update_delay_ms = 20;
    std::vector<std::string> m_epc_trackbar_names;
};
