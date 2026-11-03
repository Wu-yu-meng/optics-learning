#include "Builder.hpp"
#include "Visualization.hpp"
#include "optical_element/EPC.hpp"
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

struct ObservationWindow
{
    std::string window_id;//Qt内部使用英文标识
    std::string source;
    std::unique_ptr<VirtualOpticalPowerVisualizer> instrument;
};

struct EPCSlider
{
    std::string window_id;
    std::string trackbar_name;
    std::string element_name;
    int retarder_id;
    int last_position;//未移动滑条时保留配置中的小数相位
};

int main(int argc, char **argv)
{
    try
    {
        std::string config_path = (std::filesystem::path(__FILE__).parent_path() / "config/config.json").string();
        std::string metrics_path;
        bool headless = false;
        bool validate_only = false;
        size_t rounds = 0;
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--help" || argument == "-h")
            {
                std::cout << "AutomaticModeLocked [--config FILE] [--validate-only]\n"
                          << "                    [--headless] [--rounds N] [--metrics FILE.csv]\n"
                          << "GUI: Space run/pause | D step | A AUTO | R reset | Q/Esc quit\n";
                return 0;
            }
            if (argument == "--headless")
                headless = true;
            else if (argument == "--validate-only")
                validate_only = true;
            else if (argument == "--config" || argument == "--metrics" || argument == "--rounds")
            {
                if (index + 1 == argc)
                    throw std::invalid_argument(argument + "：缺少参数");
                const std::string value = argv[++index];
                if (argument == "--config")
                    config_path = value;
                else if (argument == "--metrics")
                    metrics_path = value;
                else
                {
                    const std::from_chars_result result = std::from_chars(value.data(),value.data() + value.size(),rounds);
                    if (result.ec != std::errc() || result.ptr != value.data() + value.size() || rounds == 0)
                        throw std::invalid_argument("--rounds：需要正整数");
                }
            }
            else
                throw std::invalid_argument("未知参数：" + argument);
        }

        Builder builder{ReJson{config_path}};
        OpticalCavity cavity = builder.build();
        std::cout << "Config: " << std::filesystem::absolute(config_path) << '\n';
        cavity.describe();
        if (validate_only)
        {
            std::cout << "Configuration valid.\n";
            return 0;
        }

        const OpticalCavity::Runtime &runtime = cavity.runtime();
        if (cavity.observations().empty() && (!runtime.epc_controls || cavity.epc_names().empty()))
        {
            headless = true;
            std::cout << "No windows configured; using headless mode.\n";
        }
        const size_t max_rounds = rounds != 0 ? rounds : runtime.max_rounds != 0 ? runtime.max_rounds : headless ? 100 : 0;
        std::ofstream metrics;
        if (!metrics_path.empty())
        {
            metrics.open(metrics_path);
            if (!metrics)
                throw std::runtime_error("无法写入指标文件：" + metrics_path);
            metrics << "round,source,peak_w,energy_pj\n" << std::setprecision(17);
        }
        const auto record_metrics = [&]()
        {
            for (const OpticalCavity::Observation &observation : cavity.observations())
            {
                const OpticalField &field = cavity.field(observation.source);
                const Eigen::RowVectorXd power = field.polarization().cwiseAbs2().colwise().sum();
                metrics << cavity.loop_count() << ',' << observation.source << ',' << power.maxCoeff() << ','
                        << power.sum() * field.basic_info().sample_stride * 1.0e-3 << '\n';
            }
        };

        std::vector<ObservationWindow> windows;
        std::vector<EPCSlider> sliders;
        if (!headless)
        {
            for (const OpticalCavity::Observation &observation : cavity.observations())
            {
                const std::string window_id = "OpticalObservation" + std::to_string(windows.size());
                std::unique_ptr<VirtualOpticalPowerVisualizer> instrument = std::make_unique<VirtualOpticalPowerVisualizer>(observation.name);
                cv::namedWindow(window_id,cv::WINDOW_NORMAL);
                cv::setWindowTitle(window_id,instrument->name() + " | Space: run/pause  D: step  A: AUTO  R: reset  Q: quit");
                windows.push_back(ObservationWindow{window_id,observation.source,std::move(instrument)});
            }
            if (runtime.epc_controls)
            {
                for (const std::string &name : cavity.epc_names())
                {
                    const std::string window_id = "EPCControls" + std::to_string(sliders.size());
                    cv::namedWindow(window_id,cv::WINDOW_NORMAL);
                    cv::setWindowTitle(window_id,"EPC - " + name);
                    cv::Mat controls(100,720,CV_8UC3,cv::Scalar(28,28,28));
                    cv::putText(controls,name + " | phase (deg)",{22,40},cv::FONT_HERSHEY_SIMPLEX,0.7,cv::Scalar(225,225,225),1,cv::LINE_AA);
                    cv::putText(controls,"Space: run/pause  D: step  A: AUTO  R: reset  Q: quit",{22,76},cv::FONT_HERSHEY_SIMPLEX,0.45,cv::Scalar(160,160,160),1,cv::LINE_AA);
                    cv::imshow(window_id,controls);
                    const EPC &epc = dynamic_cast<EPC&>(cavity.element(name));
                    for (const EPC::Retarder &retarder : epc.retarders())
                    {
                        const std::string trackbar_name = "CH" + std::to_string(retarder.ID + 1) + " phase (deg)";
                        const int position = static_cast<int>(retarder.delta_phase_deg);
                        cv::createTrackbar(trackbar_name,window_id,nullptr,360);
                        cv::setTrackbarPos(trackbar_name,window_id,position);
                        sliders.push_back(EPCSlider{window_id,trackbar_name,name,retarder.ID,position});
                    }
                }
            }
        }
        const auto render = [&]()
        {
            for (const ObservationWindow &window : windows)
                cv::imshow(window.window_id,window.instrument->measure(cavity.loop_count(),cavity.field(window.source)));
        };
        const auto auto_size = [&]()
        {
            for (const ObservationWindow &window : windows)
                window.instrument->auto_size();
        };
        if (!headless)
            render();
        if (metrics.is_open())
            record_metrics();

        bool is_running = !runtime.start_paused;
        const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        while (max_rounds == 0 || cavity.loop_count() < max_rounds)
        {
            bool single_step = false;
            if (!headless)
            {
                const int key = cv::waitKey(runtime.frame_delay_ms);
                if (key == 'q' || key == 'Q' || key == 27)
                    break;
                if (key == ' ')
                    is_running = !is_running;
                if (key == 'a' || key == 'A')
                {
                    auto_size();
                    render();//暂停时也能重新调整画幅，不推进光场和随机数状态。
                }
                if (key == 'r' || key == 'R')
                {
                    cavity.reset();
                    auto_size();
                    render();
                    continue;
                }
                single_step = key == 'd' || key == 'D';
                if (single_step)
                    is_running = false;
                if (!is_running && !single_step)
                    continue;
                for (EPCSlider &slider : sliders)
                {
                    const int position = cv::getTrackbarPos(slider.trackbar_name,slider.window_id);
                    if (position != slider.last_position)
                    {
                        dynamic_cast<EPC&>(cavity.element(slider.element_name)).set_retarder(slider.retarder_id,position);
                        slider.last_position = position;
                    }
                }
            }

            cavity.run_once();
            if (metrics.is_open())
                record_metrics();
            if (!headless && (single_step || cavity.loop_count() % runtime.render_every_n_rounds == 0))
                render();
        }
        const double elapsed_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "Finished: " << cavity.loop_count() << " rounds in " << elapsed_s << " s\n";
        if (!headless)
            cv::destroyAllWindows();
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "配置或运行错误：" << error.what() << '\n';
        return 1;
    }
}
