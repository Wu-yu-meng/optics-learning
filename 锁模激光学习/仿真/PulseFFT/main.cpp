#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <thread>
#include <vector>

#include "OpticalWave.hpp"
#include "OpticsConstants.hpp"
#include "foxglove_viz/foxglove_viz.hpp"
#include "img_viz.hpp"

namespace
{

constexpr double kDxUm = 0.05;
constexpr double kAxisLengthUm = 600.0;
constexpr int kSampleCount = static_cast<int>(kAxisLengthUm / kDxUm);
constexpr int kDisplayWidth = 1200;
constexpr int kImageHeight = 480;
constexpr int kWaveCount = 20;
constexpr double kFirstFrequencyThz = 574.0;
constexpr double kFrequencySpacingThz = 3.0;
constexpr double kLinearPhaseStepRad = pulsefft::constants::pi / 2.0;
// 二次频谱相位 eta(omega) = a * (omega - omega_0)^2 中的系数 a，单位为 fs^2。
constexpr double kQuadraticSpectralPhaseCoefficientFs2 = 80.0;
constexpr double kCenterFrequencyThz = kFirstFrequencyThz
    + (kWaveCount - 1) * kFrequencySpacingThz / 2.0;
constexpr unsigned int kFixedRandomPhaseSeed = 20260912;
constexpr unsigned int kChangingRandomPhaseSeed = 20260913;

using PhaseArray = std::array<double, kWaveCount>;
using SpatialFrame = std::array<double, kSampleCount>;

std::vector<OpticalWave> createWaves(
    timetool::Timestamp reference_time,
    const PhaseArray& initial_phases)
{
    std::vector<OpticalWave> waves;
    waves.reserve(kWaveCount);

    for (int wave_index = 0; wave_index < kWaveCount; ++wave_index)
    {
        const double frequency_thz = kFirstFrequencyThz
                                     + wave_index * kFrequencySpacingThz;
        waves.emplace_back(
            frequency_thz,
            initial_phases[wave_index],
            1.0,
            reference_time);
    }

    return waves;
}

void calculateSpatialFrame(
    const std::vector<OpticalWave>& waves,
    timetool::Timestamp now,
    SpatialFrame& electric_field,
    SpatialFrame& intensity)
{
    for (int i = 0; i < kSampleCount; ++i)
    {
        const double position_um = i * kDxUm;
        double total_field = 0.0;

        for (const auto& wave : waves)
        {
            total_field += wave.amplitude(now, position_um);
        }

        electric_field[i] = total_field;
        intensity[i] = total_field * total_field;
    }
}

// 将一个空间截面的电场数据绘制为图像。
cv::Mat renderElectricField(const SpatialFrame& electric_field)
{
    cv::Mat image(kImageHeight, kDisplayWidth, CV_8UC3, cv::Scalar(20, 20, 20));
    const int center_y = kImageHeight / 2;
    const auto [minimum_field, maximum_field] = std::minmax_element(
        electric_field.begin(), electric_field.end());
    const double electric_field_limit = std::max({
        std::abs(*minimum_field),
        std::abs(*maximum_field),
        1e-12
    });

    // 绘制 E = 0 的水平参考线。
    for (int x = 0; x < kDisplayWidth; ++x)
    {
        image.at<cv::Vec3b>(center_y, x) = cv::Vec3b(80, 80, 80);
    }

    // 纵坐标采用本帧的对称自适应范围 [-peak, +peak]。
    // 计算仍保留全部 6000 个空间点；显示时每个像素列聚合一小段数据。
    // 这样可在总览中保留快速载波的上下边界，而不会生成过宽的窗口。
    for (int display_x = 0; display_x < kDisplayWidth; ++display_x)
    {
        const int begin_index = display_x * kSampleCount / kDisplayWidth;
        const int end_index = (display_x + 1) * kSampleCount / kDisplayWidth;
        const auto [minimum, maximum] = std::minmax_element(
            electric_field.begin() + begin_index,
            electric_field.begin() + end_index);

        const auto to_y = [center_y, electric_field_limit](double field) {
            const double normalized_field = std::clamp(
                field / electric_field_limit,
                -1.0,
                1.0);
            return static_cast<int>(std::lround(
                center_y - normalized_field * (kImageHeight * 0.45)));
        };

        const int y_min = to_y(*maximum);
        const int y_max = to_y(*minimum);

        for (int y = y_min; y <= y_max; ++y)
        {
            image.at<cv::Vec3b>(y, display_x) = cv::Vec3b(0, 220, 255);
        }
    }

    return image;
}

// 将瞬时电场平方 E^2 绘制为相对强度图像。
cv::Mat renderIntensity(const SpatialFrame& intensity)
{
    cv::Mat image(kImageHeight, kDisplayWidth, CV_8UC3, cv::Scalar(20, 20, 20));
    const int baseline_y = kImageHeight - 1;
    const double intensity_limit = std::max(*std::max_element(
        intensity.begin(), intensity.end()), 1e-12);

    // 绘制 I = 0 的水平参考线。
    for (int x = 0; x < kDisplayWidth; ++x)
    {
        image.at<cv::Vec3b>(baseline_y, x) = cv::Vec3b(80, 80, 80);
    }

    // 纵坐标采用本帧自适应范围 [0, peak]。
    for (int display_x = 0; display_x < kDisplayWidth; ++display_x)
    {
        const int begin_index = display_x * kSampleCount / kDisplayWidth;
        const int end_index = (display_x + 1) * kSampleCount / kDisplayWidth;
        const double maximum_intensity = *std::max_element(
            intensity.begin() + begin_index,
            intensity.begin() + end_index);
        const double normalized_intensity = std::clamp(
            maximum_intensity / intensity_limit,
            0.0,
            1.0);
        const int y = static_cast<int>(std::lround(
            baseline_y - normalized_intensity * (kImageHeight * 0.90)));

        for (int row = y; row <= baseline_y; ++row)
        {
            image.at<cv::Vec3b>(row, display_x) = cv::Vec3b(80, 220, 80);
        }
    }

    return image;
}

} // namespace

int main()
{
    // 五种情形都使用当前的 20 个严格等间隔频率成分。
    const timetool::Timestamp reference_time = timetool::now();
    const PhaseArray all_zero_phases{};

    PhaseArray linear_phases{};
    for (int wave_index = 0; wave_index < kWaveCount; ++wave_index)
    {
        linear_phases[wave_index] = wave_index * kLinearPhaseStepRad;
    }

    PhaseArray quadratic_phases{};
    for (int wave_index = 0; wave_index < kWaveCount; ++wave_index)
    {
        const double frequency_thz = kFirstFrequencyThz
                                     + wave_index * kFrequencySpacingThz;
        const double angular_frequency_offset_rad_per_fs =
            pulsefft::constants::two_pi * 1e-3
            * (frequency_thz - kCenterFrequencyThz);
        quadratic_phases[wave_index] =
            kQuadraticSpectralPhaseCoefficientFs2
            * angular_frequency_offset_rad_per_fs
            * angular_frequency_offset_rad_per_fs;
    }

    std::uniform_real_distribution<double> phase_distribution(
        0.0,
        pulsefft::constants::two_pi);
    PhaseArray fixed_random_phases{};
    std::mt19937 fixed_random_engine(kFixedRandomPhaseSeed);
    for (double& phase : fixed_random_phases)
    {
        phase = phase_distribution(fixed_random_engine);
    }

    const std::vector<OpticalWave> all_zero_waves =
        createWaves(reference_time, all_zero_phases);
    const std::vector<OpticalWave> linear_phase_waves =
        createWaves(reference_time, linear_phases);
    const std::vector<OpticalWave> quadratic_phase_waves =
        createWaves(reference_time, quadratic_phases);
    const std::vector<OpticalWave> fixed_random_phase_waves =
        createWaves(reference_time, fixed_random_phases);
    std::mt19937 changing_random_engine(kChangingRandomPhaseSeed);

    // 在同一个 Foxglove topic 中发布五种相位情形下 x = 0 um 处的相对光强。
    auto intensity_at_x0_publisher = foxglove_viz::global_foxglove_server()
        .create_publisher<double, double, double, double, double>(
            "/pulse/intensity_at_x0",
            {
                "unlocked_changing_phase_au",
                "locked_equal_phase_au",
                "locked_linear_phase_au",
                "fixed_random_phase_au",
                "locked_quadratic_phase_au"
            });

    // 坐标轴：0 ~ 599.95 um，步长 0.05 um；共 12000 个空间采样点。
    SpatialFrame E_frame;//电场
    SpatialFrame I_frame;//瞬时电场平方

    ImgViz::init(true);

    while (true)
    {
        //获取当前时间
        timetool::Timestamp now = timetool::now();

        // 情形 1：每帧重置随机相位，代表未锁定时相对相位不断漂移。
        PhaseArray changing_random_phases{};
        for (double& phase : changing_random_phases)
        {
            phase = phase_distribution(changing_random_engine);
        }
        const std::vector<OpticalWave> changing_random_phase_waves =
            createWaves(reference_time, changing_random_phases);
        calculateSpatialFrame(changing_random_phase_waves, now, E_frame, I_frame);
        const double unlocked_intensity_at_x0 = I_frame[0];
        ImgViz::enqueue_image_copy(
            "1 Unlocked: changing random phases, I(x)",
            renderIntensity(I_frame));

        // 情形 2：所有模式同相，产生最理想的尖锐脉冲。
        calculateSpatialFrame(all_zero_waves, now, E_frame, I_frame);
        const double equal_phase_intensity_at_x0 = I_frame[0];
        ImgViz::enqueue_image_copy(
            "2 Locked: all phases equal, E(x)",
            renderElectricField(E_frame));
        ImgViz::enqueue_image_copy(
            "2 Locked: all phases equal, I(x)",
            renderIntensity(I_frame));

        // 情形 3：线性相位斜坡使脉冲平移，但不改变理想脉冲形状。
        calculateSpatialFrame(linear_phase_waves, now, E_frame, I_frame);
        const double linear_phase_intensity_at_x0 = I_frame[0];
        ImgViz::enqueue_image_copy(
            "3 Locked: linear phase ramp, I(x)",
            renderIntensity(I_frame));

        // 情形 4：随机相位固定不变，波形重复但一般不再是尖锐脉冲。
        calculateSpatialFrame(fixed_random_phase_waves, now, E_frame, I_frame);
        const double fixed_random_phase_intensity_at_x0 = I_frame[0];
        ImgViz::enqueue_image_copy(
            "4 Fixed random phases, I(x)",
            renderIntensity(I_frame));

        // 情形 5：二次频谱相位保持稳定，但使不同频率具有不同延迟，脉冲发生展宽。
        calculateSpatialFrame(quadratic_phase_waves, now, E_frame, I_frame);
        const double quadratic_phase_intensity_at_x0 = I_frame[0];
        ImgViz::enqueue_image_copy(
            "5 Locked but chirped: quadratic spectral phase, I(x)",
            renderIntensity(I_frame));

        intensity_at_x0_publisher->publish_with_time(
            now,
            unlocked_intensity_at_x0,
            equal_phase_intensity_at_x0,
            linear_phase_intensity_at_x0,
            fixed_random_phase_intensity_at_x0,
            quadratic_phase_intensity_at_x0);

        // 防止计算循环占满一个 CPU 核；可视化线程会显示最新提交的帧。
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return 0;
}
