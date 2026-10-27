#include "OpticalFieldVisualization.hpp"
#include "DFT.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

namespace
{

constexpr const char* kWindowName = "Automatic mode-locked laser";
constexpr const char* kScopeWindowName = "Simulated oscilloscope";
constexpr const char* kSpectrumWindowName = "Optical spectrum";
constexpr int kImageWidth = 1280;
constexpr int kImageHeight = 720;
constexpr int kScopeImageHeight = 460;
constexpr int kSpectrumImageWidth = 1920;
constexpr int kSpectrumImageHeight = 600;
constexpr int kPlotLeft = 105;
constexpr int kPlotRight = 1235;
constexpr int kPlotTop = 105;
constexpr int kPlotBottom = 615;
constexpr int kMaxEpcPhaseDeg = 360;
constexpr double kBoundaryEnergyFractionWarning = 1.0e-4;
constexpr double kMinimumEnergyForBoundaryWarningJ = 1.0e-16;//1e-4 pJ，避免纯微弱噪声触发警告。
constexpr double kPulseEnergyFloorJ = 1.0e-16;//1e-4 pJ，仅用于提示低信号，不推断是否存在ASE。
constexpr double kCavityDisplayMinimumFullScaleW = 1.0e-3;//防止微弱残余场被自动放大成“强脉冲”。
constexpr double kSpectrumHalfSpanThz = 4.0;
constexpr double kSpectrumFloorDb = -60.0;
constexpr std::size_t kSpectrumZeroPaddingFactor = 4;

struct FieldMetrics
{
    double max_power_w = 0.0;
    double energy_j = 0.0;
    double edge_energy_fraction = 0.0;
};

std::string format_number(double value, int precision = 3)
{
    std::ostringstream stream;
    const double absolute_value = std::abs(value);
    if ((absolute_value != 0.0 && absolute_value < 1.0e-2) || absolute_value >= 1.0e4)
        stream << std::scientific;
    else
        stream << std::fixed;

    stream << std::setprecision(precision) << value;
    return stream.str();
}

FieldMetrics calculate_field_metrics(const OpticalField& field)
{
    FieldMetrics metrics;
    const auto& polarization = field.polarization();
    const std::size_t sample_size = static_cast<std::size_t>(polarization.cols());
    const std::size_t edge_sample_count = std::max<std::size_t>(1, sample_size / 20);//左右各5%
    double edge_energy_j = 0.0;
    const double sample_stride_s = field.basic_info().sample_stride * 1.0e-15;

    for (Eigen::Index column = 0; column < polarization.cols(); ++column)
    {
        const double power_w =
            std::norm(polarization(0, column)) + std::norm(polarization(1, column));
        if (!std::isfinite(power_w))
            throw std::runtime_error("[OpticalFieldVisualization]光功率超出数值范围，请检查传播模型");

        metrics.max_power_w = std::max(metrics.max_power_w, power_w);
        const double sample_energy_j = power_w * sample_stride_s;
        metrics.energy_j += sample_energy_j;

        const std::size_t index = static_cast<std::size_t>(column);
        if (index < edge_sample_count || index >= sample_size - edge_sample_count)
            edge_energy_j += sample_energy_j;
    }

    if (metrics.energy_j > 0.0)
        metrics.edge_energy_fraction = edge_energy_j / metrics.energy_j;
    return metrics;
}

cv::Mat render_optical_field(
    const OpticalField& field,
    std::size_t round_trip,
    double max_power_w,
    double y_axis_max_w,
    bool is_running,
    int update_delay_ms,
    double edge_energy_fraction,
    bool boundary_risk,
    bool low_signal,
    const std::vector<double>& epc_delta_phase_deg)
{
    const cv::Scalar background(28, 28, 28);
    const cv::Scalar foreground(225, 225, 225);
    const cv::Scalar secondary(155, 155, 155);
    const cv::Scalar grid(58, 58, 58);
    const cv::Scalar total_color(80, 220, 255);
    const cv::Scalar x_color(255, 155, 70);
    const cv::Scalar y_color(95, 205, 110);
    const cv::Scalar warning_color(70, 165, 255);

    cv::Mat image(kImageHeight, kImageWidth, CV_8UC3, background);
    const auto& info = field.basic_info();
    const auto& polarization = field.polarization();
    const double end_time_fs = info.start_time +
        static_cast<double>(info.sample_size - 1) * info.sample_stride;
    y_axis_max_w = std::max(y_axis_max_w, kCavityDisplayMinimumFullScaleW);
    const bool trace_clipped = max_power_w > y_axis_max_w;

    cv::putText(image, "Live cavity optical field power", {kPlotLeft, 40},
                cv::FONT_HERSHEY_SIMPLEX, 0.85, foreground, 2, cv::LINE_AA);
    cv::putText(
        image,
        "Round trip: " + std::to_string(round_trip) +
            (is_running ? "    RUNNING" : "    PAUSED") +
            "    frame delay=" + std::to_string(update_delay_ms) + " ms",
        {kPlotLeft, 74}, cv::FONT_HERSHEY_SIMPLEX, 0.58, secondary, 1, cv::LINE_AA);
    cv::putText(image,
                "Space: run/pause  D/right: one round  [ slower  ] faster  A: auto  R: reset  Q/Esc: quit",
                {590, 74}, cv::FONT_HERSHEY_SIMPLEX, 0.35, secondary, 1, cv::LINE_AA);

    if (boundary_risk)
    {
        cv::putText(
            image,
            "WARNING: FFT boundary wrap risk, outer 10% energy=" +
                format_number(100.0 * edge_energy_fraction, 4) + "% (auto-paused once)",
            {kPlotLeft, 98}, cv::FONT_HERSHEY_SIMPLEX, 0.43,
            warning_color, 1, cv::LINE_AA);
    }
    else if (low_signal)
    {
        cv::putText(
            image,
            "LOW SIGNAL: total field energy is below the display threshold",
            {kPlotLeft, 98}, cv::FONT_HERSHEY_SIMPLEX, 0.43,
            warning_color, 1, cv::LINE_AA);
    }
    else if (trace_clipped)
    {
        cv::putText(
            image,
            "CLIPPED: click AUTO or press A",
            {kPlotLeft, 98}, cv::FONT_HERSHEY_SIMPLEX, 0.43,
            warning_color, 1, cv::LINE_AA);
    }

    constexpr int grid_divisions = 5;
    for (int index = 0; index <= grid_divisions; ++index)
    {
        const double ratio = static_cast<double>(index) / grid_divisions;
        const int x = kPlotLeft + static_cast<int>(ratio * (kPlotRight - kPlotLeft));
        const int y = kPlotBottom - static_cast<int>(ratio * (kPlotBottom - kPlotTop));
        cv::line(image, {x, kPlotTop}, {x, kPlotBottom}, grid, 1, cv::LINE_AA);
        cv::line(image, {kPlotLeft, y}, {kPlotRight, y}, grid, 1, cv::LINE_AA);

        const double time_fs = info.start_time + ratio * (end_time_fs - info.start_time);
        cv::putText(image, format_number(time_fs, 0), {x - 24, kPlotBottom + 27},
                    cv::FONT_HERSHEY_SIMPLEX, 0.42, secondary, 1, cv::LINE_AA);
        cv::putText(image, format_number(ratio * y_axis_max_w, 2), {10, y + 5},
                    cv::FONT_HERSHEY_SIMPLEX, 0.42, secondary, 1, cv::LINE_AA);
    }

    cv::rectangle(image, {kPlotLeft, kPlotTop}, {kPlotRight, kPlotBottom},
                  secondary, 1, cv::LINE_AA);
    cv::putText(image, "Power (W)", {10, kPlotTop - 17},
                cv::FONT_HERSHEY_SIMPLEX, 0.48, foreground, 1, cv::LINE_AA);
    cv::putText(image, "Time (fs)", {(kPlotLeft + kPlotRight) / 2 - 35, kPlotBottom + 57},
                cv::FONT_HERSHEY_SIMPLEX, 0.48, foreground, 1, cv::LINE_AA);

    std::vector<cv::Point> total_points;
    std::vector<cv::Point> x_points;
    std::vector<cv::Point> y_points;
    total_points.reserve(info.sample_size);
    x_points.reserve(info.sample_size);
    y_points.reserve(info.sample_size);

    double peak_power_w = 0.0;
    double energy_j = 0.0;
    const double index_denominator =
        static_cast<double>(std::max<std::size_t>(info.sample_size - 1, 1));
    for (std::size_t index = 0; index < info.sample_size; ++index)
    {
        const Eigen::Index column = static_cast<Eigen::Index>(index);
        const double x_power_w = std::norm(polarization(0, column));
        const double y_power_w = std::norm(polarization(1, column));
        const double total_power_w = x_power_w + y_power_w;
        if (!std::isfinite(total_power_w))
            continue;

        peak_power_w = std::max(peak_power_w, total_power_w);
        energy_j += total_power_w * info.sample_stride * 1.0e-15;
        const int x = kPlotLeft + static_cast<int>(
            static_cast<double>(index) / index_denominator * (kPlotRight - kPlotLeft));
        const auto power_to_y = [y_axis_max_w](double power_w)
        {
            const double ratio = std::clamp(power_w / y_axis_max_w, 0.0, 1.0);
            return kPlotBottom - static_cast<int>(ratio * (kPlotBottom - kPlotTop));
        };
        total_points.emplace_back(x, power_to_y(total_power_w));
        x_points.emplace_back(x, power_to_y(x_power_w));
        y_points.emplace_back(x, power_to_y(y_power_w));
    }

    if (total_points.size() >= 2)
    {
        cv::polylines(image, total_points, false, total_color, 2, cv::LINE_AA);
        cv::polylines(image, x_points, false, x_color, 1, cv::LINE_AA);
        cv::polylines(image, y_points, false, y_color, 1, cv::LINE_AA);
    }

    const int legend_y = 684;
    cv::line(image, {kPlotLeft, legend_y}, {kPlotLeft + 30, legend_y}, total_color, 2);
    cv::putText(image, "Total", {kPlotLeft + 38, legend_y + 5},
                cv::FONT_HERSHEY_SIMPLEX, 0.46, foreground, 1, cv::LINE_AA);
    cv::line(image, {kPlotLeft + 118, legend_y}, {kPlotLeft + 148, legend_y}, x_color, 2);
    cv::putText(image, "X", {kPlotLeft + 156, legend_y + 5},
                cv::FONT_HERSHEY_SIMPLEX, 0.46, foreground, 1, cv::LINE_AA);
    cv::line(image, {kPlotLeft + 200, legend_y}, {kPlotLeft + 230, legend_y}, y_color, 2);
    cv::putText(image, "Y", {kPlotLeft + 238, legend_y + 5},
                cv::FONT_HERSHEY_SIMPLEX, 0.46, foreground, 1, cv::LINE_AA);

    std::string epc_text = "EPC next round:";
    for (std::size_t index = 0; index < epc_delta_phase_deg.size(); ++index)
        epc_text += "  CH" + std::to_string(index + 1) + "=" +
            format_number(epc_delta_phase_deg[index], 0) + " deg";

    cv::putText(image,
                "Peak: " + format_number(peak_power_w) + " W    Energy: " +
                    format_number(energy_j * 1.0e12) + " pJ",
                {430, legend_y + 5}, cv::FONT_HERSHEY_SIMPLEX, 0.45,
                foreground, 1, cv::LINE_AA);
    cv::putText(image, epc_text, {kPlotLeft, legend_y + 30},
                cv::FONT_HERSHEY_SIMPLEX, 0.38, secondary, 1, cv::LINE_AA);
    return image;
}

cv::Mat render_oscilloscope(
    const VirtualOscilloscope::VoltageSignal& signal,
    std::size_t round_trip,
    double y_axis_max_mv,
    bool boundary_risk)
{
    const cv::Scalar background(18, 24, 18);
    const cv::Scalar foreground(220, 235, 220);
    const cv::Scalar secondary(145, 165, 145);
    const cv::Scalar grid(42, 60, 42);
    const cv::Scalar trace_color(90, 255, 150);
    const cv::Scalar warning_color(70, 165, 255);

    constexpr int plot_left = 105;
    constexpr int plot_right = 1235;
    constexpr int plot_top = 120;
    constexpr int plot_bottom = 370;
    cv::Mat image(kScopeImageHeight, kImageWidth, CV_8UC3, background);

    double peak_voltage_v = 0.0;
    for (double voltage_v : signal.samples_v)
    {
        if (std::isfinite(voltage_v))
            peak_voltage_v = std::max(peak_voltage_v, voltage_v);
    }
    y_axis_max_mv = std::max(y_axis_max_mv, 1.0e-9);
    const bool trace_clipped = peak_voltage_v * 1.0e3 > y_axis_max_mv;
    const double start_time_ps = signal.start_time_fs * 1.0e-3;
    const double end_time_ps = signal.samples_v.empty()
        ? start_time_ps
        : (signal.start_time_fs +
           static_cast<double>(signal.samples_v.size() - 1) * signal.sample_stride_fs) *
              1.0e-3;

    cv::putText(image, "Simulated oscilloscope: OC output voltage", {plot_left, 38},
                cv::FONT_HERSHEY_SIMPLEX, 0.82, foreground, 2, cv::LINE_AA);
    cv::putText(
        image,
        "Round trip: " + std::to_string(round_trip) +
            "    local fast-time window (not a full cavity period)",
        {plot_left, 70}, cv::FONT_HERSHEY_SIMPLEX, 0.52, secondary, 1, cv::LINE_AA);
    cv::putText(
        image,
        format_number(signal.optical_attenuation_db, 0) + " dB optical attenuation  |  " +
            format_number(signal.responsivity_a_per_w, 2) + " A/W  |  " +
            format_number(signal.load_resistance_ohm, 0) + " ohm  |  " +
            format_number(signal.bandwidth_ghz, 0) + " GHz bandwidth",
        {plot_left, 92}, cv::FONT_HERSHEY_SIMPLEX, 0.40, secondary, 1, cv::LINE_AA);

    constexpr int grid_divisions = 5;
    for (int index = 0; index <= grid_divisions; ++index)
    {
        const double ratio = static_cast<double>(index) / grid_divisions;
        const int x = plot_left + static_cast<int>(ratio * (plot_right - plot_left));
        const int y = plot_bottom - static_cast<int>(ratio * (plot_bottom - plot_top));
        cv::line(image, {x, plot_top}, {x, plot_bottom}, grid, 1, cv::LINE_AA);
        cv::line(image, {plot_left, y}, {plot_right, y}, grid, 1, cv::LINE_AA);

        const double time_ps = start_time_ps + ratio * (end_time_ps - start_time_ps);
        cv::putText(image, format_number(time_ps, 2), {x - 28, plot_bottom + 25},
                    cv::FONT_HERSHEY_SIMPLEX, 0.40, secondary, 1, cv::LINE_AA);
        cv::putText(image, format_number(ratio * y_axis_max_mv, 2), {12, y + 5},
                    cv::FONT_HERSHEY_SIMPLEX, 0.40, secondary, 1, cv::LINE_AA);
    }

    cv::rectangle(image, {plot_left, plot_top}, {plot_right, plot_bottom},
                  secondary, 1, cv::LINE_AA);
    cv::putText(image, "Voltage (mV)", {10, plot_top - 17},
                cv::FONT_HERSHEY_SIMPLEX, 0.44, foreground, 1, cv::LINE_AA);
    cv::putText(image, "Local time (ps)", {(plot_left + plot_right) / 2 - 45, plot_bottom + 52},
                cv::FONT_HERSHEY_SIMPLEX, 0.44, foreground, 1, cv::LINE_AA);

    std::vector<cv::Point> trace_points;
    trace_points.reserve(signal.samples_v.size());
    const double index_denominator = static_cast<double>(
        std::max<std::size_t>(signal.samples_v.size() - 1, 1));
    for (std::size_t index = 0; index < signal.samples_v.size(); ++index)
    {
        const double voltage_mv = signal.samples_v[index] * 1.0e3;
        if (!std::isfinite(voltage_mv))
            continue;
        const int x = plot_left + static_cast<int>(
            static_cast<double>(index) / index_denominator * (plot_right - plot_left));
        const double ratio = std::clamp(voltage_mv / y_axis_max_mv, 0.0, 1.0);
        const int y = plot_bottom - static_cast<int>(ratio * (plot_bottom - plot_top));
        trace_points.emplace_back(x, y);
    }
    if (trace_points.size() >= 2)
        cv::polylines(image, trace_points, false, trace_color, 2, cv::LINE_AA);

    cv::putText(
        image,
        "Peak: " + format_number(peak_voltage_v * 1.0e3) + " mV",
        {plot_left, 443}, cv::FONT_HERSHEY_SIMPLEX, 0.46,
        foreground, 1, cv::LINE_AA);
    cv::putText(
        image,
        "Fixed full scale: " + format_number(y_axis_max_mv) + " mV  (click AUTO or press A)",
        {335, 443}, cv::FONT_HERSHEY_SIMPLEX, 0.40,
        secondary, 1, cv::LINE_AA);
    if (boundary_risk)
    {
        cv::putText(
            image,
            "Cavity edge energy detected: inspect time-window boundaries",
            {790, 443}, cv::FONT_HERSHEY_SIMPLEX, 0.38,
            warning_color, 1, cv::LINE_AA);
    }
    else if (trace_clipped)
    {
        cv::putText(
            image,
            "CLIPPED: click AUTO",
            {1010, 443}, cv::FONT_HERSHEY_SIMPLEX, 0.43,
            warning_color, 1, cv::LINE_AA);
    }
    return image;
}

cv::Mat render_spectrum(
    const OpticalField& field,
    std::size_t round_trip,
    bool low_signal,
    bool boundary_risk)
{
    const cv::Scalar background(22, 20, 28);
    const cv::Scalar foreground(235, 225, 240);
    const cv::Scalar secondary(165, 150, 175);
    const cv::Scalar grid(55, 48, 65);
    const cv::Scalar trace_color(235, 145, 255);
    const cv::Scalar warning_color(70, 165, 255);

    constexpr int plot_left = 120;
    constexpr int plot_right = 1870;
    constexpr int plot_top = 110;
    constexpr int plot_bottom = 500;
    constexpr int footer_y = 575;
    cv::Mat image(
        kSpectrumImageHeight,
        kSpectrumImageWidth,
        CV_8UC3,
        background);

    const auto& info = field.basic_info();
    if (info.sample_size > static_cast<std::size_t>(std::numeric_limits<int>::max()) / kSpectrumZeroPaddingFactor)
        throw std::invalid_argument("[OpticalFieldVisualization]显示频谱点数超过DFT支持的范围");
    const std::size_t spectrum_sample_size =
        info.sample_size * kSpectrumZeroPaddingFactor;
    OpticalField::Polarization spectrum = OpticalField::Polarization::Zero(
        2,
        static_cast<Eigen::Index>(spectrum_sample_size));
    spectrum.leftCols(static_cast<Eigen::Index>(info.sample_size)) =
        field.polarization();
    //仅对显示数据补零，不修改仿真光场，也不改变DFT接口的输入输出点数。
    const OpticalField display_field(std::move(spectrum), info);
    spectrum = DFT::forward(display_field);

    struct SpectrumPoint
    {
        double frequency_offset_thz;
        double power;
    };

    std::vector<SpectrumPoint> points;
    points.reserve(spectrum_sample_size);
    double peak_spectral_power = 0.0;
    double peak_frequency_offset_thz = 0.0;
    const long long sample_size = static_cast<long long>(spectrum_sample_size);

    //显示物理频偏Ω=ω-ω0，按从负到正排序。
    //当前载波为exp(i*(beta0*z-omega0*t))，物理频偏与标准DFT坐标q符号相反：Ω=-q。
    for (long long signed_bin = -sample_size / 2;
         signed_bin < (sample_size + 1) / 2;
         ++signed_bin)
    {
        const std::size_t index = static_cast<std::size_t>(
            (sample_size - signed_bin) % sample_size);
        const double frequency_offset_thz =
            static_cast<double>(signed_bin) * 1000.0 /
            (static_cast<double>(spectrum_sample_size) * info.sample_stride);
        const double spectral_power =
            std::norm(spectrum(0, static_cast<Eigen::Index>(index))) +
            std::norm(spectrum(1, static_cast<Eigen::Index>(index)));
        if (!std::isfinite(spectral_power))
            throw std::runtime_error("[OpticalFieldVisualization]频谱功率超出数值范围，请检查传播模型");

        points.push_back({frequency_offset_thz, spectral_power});
        if (spectral_power > peak_spectral_power)
        {
            peak_spectral_power = spectral_power;
            peak_frequency_offset_thz = frequency_offset_thz;
        }
    }

    double half_maximum_start_thz = 0.0;
    double half_maximum_end_thz = 0.0;
    bool found_half_maximum = false;
    if (peak_spectral_power > 0.0)
    {
        for (const SpectrumPoint& point : points)
        {
            if (point.power < 0.5 * peak_spectral_power)
                continue;

            if (!found_half_maximum)
            {
                half_maximum_start_thz = point.frequency_offset_thz;
                found_half_maximum = true;
            }
            half_maximum_end_thz = point.frequency_offset_thz;
        }
    }
    const double half_maximum_width_thz = found_half_maximum
        ? half_maximum_end_thz - half_maximum_start_thz
        : 0.0;

    cv::putText(image, "Cavity optical spectrum", {plot_left, 42},
                cv::FONT_HERSHEY_SIMPLEX, 0.82, foreground, 2, cv::LINE_AA);
    cv::putText(
        image,
        "Round trip: " + std::to_string(round_trip) +
            "    normalized power spectrum  |  4x display interpolation (not extra resolution)",
        {plot_left, 80}, cv::FONT_HERSHEY_SIMPLEX, 0.52, secondary, 1, cv::LINE_AA);
    cv::putText(
        image,
        "Carrier: " + format_number(info.center_frequency, 3) + " THz  |  " +
            format_number(info.wave_length, 1) + " nm",
        {1420, 80}, cv::FONT_HERSHEY_SIMPLEX, 0.44, secondary, 1, cv::LINE_AA);

    constexpr int grid_divisions = 5;
    for (int index = 0; index <= grid_divisions; ++index)
    {
        const double ratio = static_cast<double>(index) / grid_divisions;
        const int x = plot_left + static_cast<int>(ratio * (plot_right - plot_left));
        const int y = plot_bottom - static_cast<int>(ratio * (plot_bottom - plot_top));
        cv::line(image, {x, plot_top}, {x, plot_bottom}, grid, 1, cv::LINE_AA);
        cv::line(image, {plot_left, y}, {plot_right, y}, grid, 1, cv::LINE_AA);

        const double frequency_offset_thz =
            -kSpectrumHalfSpanThz + ratio * 2.0 * kSpectrumHalfSpanThz;
        const double power_db = kSpectrumFloorDb + ratio * -kSpectrumFloorDb;
        cv::putText(image, format_number(frequency_offset_thz, 1),
                    {x - 25, plot_bottom + 25},
                    cv::FONT_HERSHEY_SIMPLEX, 0.40, secondary, 1, cv::LINE_AA);
        cv::putText(image, format_number(power_db, 0), {28, y + 5},
                    cv::FONT_HERSHEY_SIMPLEX, 0.40, secondary, 1, cv::LINE_AA);
    }

    cv::rectangle(image, {plot_left, plot_top}, {plot_right, plot_bottom},
                  secondary, 1, cv::LINE_AA);
    cv::putText(image, "Power (dB)", {22, plot_top - 17},
                cv::FONT_HERSHEY_SIMPLEX, 0.44, foreground, 1, cv::LINE_AA);
    cv::putText(image, "Frequency offset from carrier (THz)",
                {(plot_left + plot_right) / 2 - 125, plot_bottom + 52},
                cv::FONT_HERSHEY_SIMPLEX, 0.44, foreground, 1, cv::LINE_AA);

    std::vector<cv::Point> trace_points;
    trace_points.reserve(points.size());
    if (peak_spectral_power > 0.0)
    {
        for (const SpectrumPoint& point : points)
        {
            if (point.frequency_offset_thz < -kSpectrumHalfSpanThz ||
                point.frequency_offset_thz > kSpectrumHalfSpanThz)
                continue;

            const double normalized_power = point.power / peak_spectral_power;
            const double power_db = std::max(
                10.0 * std::log10(std::max(normalized_power, 1.0e-12)),
                kSpectrumFloorDb);
            const double x_ratio =
                (point.frequency_offset_thz + kSpectrumHalfSpanThz) /
                (2.0 * kSpectrumHalfSpanThz);
            const double y_ratio =
                (power_db - kSpectrumFloorDb) / -kSpectrumFloorDb;
            const int x = plot_left + static_cast<int>(x_ratio * (plot_right - plot_left));
            const int y = plot_bottom - static_cast<int>(y_ratio * (plot_bottom - plot_top));
            trace_points.emplace_back(x, y);
        }
    }
    if (trace_points.size() >= 2)
        cv::polylines(image, trace_points, false, trace_color, 2, cv::LINE_AA);

    cv::putText(
        image,
        "Peak offset: " + format_number(peak_frequency_offset_thz, 3) + " THz",
        {plot_left, footer_y}, cv::FONT_HERSHEY_SIMPLEX, 0.44,
        foreground, 1, cv::LINE_AA);
    cv::putText(
        image,
        "Half-maximum span: " + format_number(half_maximum_width_thz, 3) + " THz",
        {500, footer_y}, cv::FONT_HERSHEY_SIMPLEX, 0.44,
        foreground, 1, cv::LINE_AA);

    if (boundary_risk)
    {
        cv::putText(
            image,
            "Edge energy detected: check truncation / periodic wrap",
            {1100, footer_y}, cv::FONT_HERSHEY_SIMPLEX, 0.37,
            warning_color, 1, cv::LINE_AA);
    }
    else if (low_signal)
    {
        cv::putText(
            image,
            "LOW SIGNAL: this spectrum is normalized, not absolute power",
            {1260, footer_y}, cv::FONT_HERSHEY_SIMPLEX, 0.39,
            warning_color, 1, cv::LINE_AA);
    }
    return image;
}

bool is_single_step_key(int key)
{
    return key == 'd' || key == 'D' || key == 65363 || key == 2555904 || key == 63235;
}

} // namespace

OpticalFieldVisualization::OpticalFieldVisualization(std::size_t epc_retarder_count)
{
    if (epc_retarder_count == 0)
        throw std::invalid_argument("[OpticalFieldVisualization]EPC拨片数不能为0");

    cv::namedWindow(kWindowName, cv::WINDOW_NORMAL);
    cv::resizeWindow(kWindowName, kImageWidth, kImageHeight);
    m_window_created = true;

    cv::namedWindow(kScopeWindowName, cv::WINDOW_NORMAL);
    cv::resizeWindow(kScopeWindowName, kImageWidth, kScopeImageHeight);
    m_scope_window_created = true;

    cv::namedWindow(kSpectrumWindowName, cv::WINDOW_NORMAL);
    cv::resizeWindow(
        kSpectrumWindowName,
        kSpectrumImageWidth,
        kSpectrumImageHeight);
    m_spectrum_window_created = true;

    m_epc_trackbar_names.reserve(epc_retarder_count);
    for (std::size_t index = 0; index < epc_retarder_count; ++index)
    {
        m_epc_trackbar_names.push_back("EPC CH" + std::to_string(index + 1) + " phase (deg)");
        cv::createTrackbar(m_epc_trackbar_names.back(), kWindowName, nullptr, kMaxEpcPhaseDeg);
        cv::setTrackbarPos(m_epc_trackbar_names.back(), kWindowName, 0);
    }

    //Qt后端提供AUTO按钮；其他后端仍可通过A键重新设置纵轴量程。
    try
    {
        cv::createButton("AUTO vertical scales", &OpticalFieldVisualization::on_auto_scale,
                         this, cv::QT_PUSH_BUTTON, false);
    }
    catch (const cv::Exception&)
    {
        std::cerr << "AUTO button is unavailable on this GUI backend; press A to rescale.\n";
    }
}

OpticalFieldVisualization::~OpticalFieldVisualization()
{
    //用户可能已经关闭窗口，析构时不能再向外抛出HighGUI异常。
    const auto close_window = [](const char* name, bool created)
    {
        if (!created)
            return;
        try { cv::destroyWindow(name); }
        catch (const cv::Exception&) {}
    };
    close_window(kWindowName, m_window_created);
    close_window(kScopeWindowName, m_scope_window_created);
    close_window(kSpectrumWindowName, m_spectrum_window_created);
}

OpticalFieldVisualizationCommand OpticalFieldVisualization::update(
    const OpticalField& optical_field,
    const VirtualOscilloscope::VoltageSignal& oscilloscope_signal,
    std::size_t round_trip)
{
    OpticalFieldVisualizationCommand command;
    command.epc_delta_phase_deg.reserve(m_epc_trackbar_names.size());

    const auto& info = optical_field.basic_info();
    if (optical_field.polarization().cols() == 0 ||
        !std::isfinite(info.sample_stride) || info.sample_stride <= 0.0 ||
        !std::isfinite(info.start_time))
        throw std::invalid_argument("[OpticalFieldVisualization]光场为空或时间采样信息无效");
    if (!optical_field.polarization().allFinite())
        throw std::invalid_argument("[OpticalFieldVisualization]光场含有NaN或无穷大，请检查传播模型");

    const auto window_visible = [](const char* name)
    {
        try { return cv::getWindowProperty(name, cv::WND_PROP_VISIBLE) >= 1.0; }
        catch (const cv::Exception&) { return false; }
    };
    if (!window_visible(kWindowName) || !window_visible(kScopeWindowName) ||
        !window_visible(kSpectrumWindowName))
    {
        command.should_exit = true;
        return command;
    }

    for (const std::string& trackbar_name : m_epc_trackbar_names)
    {
        command.epc_delta_phase_deg.push_back(static_cast<double>(
            cv::getTrackbarPos(trackbar_name, kWindowName)));
    }

    const FieldMetrics metrics = calculate_field_metrics(optical_field);
    const bool boundary_risk =
        metrics.energy_j >= kMinimumEnergyForBoundaryWarningJ &&
        metrics.edge_energy_fraction >= kBoundaryEnergyFractionWarning;
    const bool low_signal = round_trip > 0 && metrics.energy_j < kPulseEnergyFloorJ;

    double scope_peak_voltage_mv = 0.0;
    for (double voltage_v : oscilloscope_signal.samples_v)
    {
        if (std::isfinite(voltage_v))
            scope_peak_voltage_mv = std::max(scope_peak_voltage_mv, voltage_v * 1.0e3);
    }
    if (m_auto_scale_requested)
    {
        m_cavity_y_axis_max_w = std::max(
            metrics.max_power_w * 1.05,
            kCavityDisplayMinimumFullScaleW);
        m_scope_y_axis_max_mv = std::max(scope_peak_voltage_mv * 1.05, 1.0e-9);
        m_auto_scale_requested = false;
    }

    //第一次进入FFT边界风险状态时自动暂停；用户仍可以再次按空格继续观察。
    if (boundary_risk && !m_boundary_warning_latched)
    {
        m_is_running = false;
        m_boundary_warning_latched = true;
    }
    else if (!boundary_risk)
    {
        m_boundary_warning_latched = false;
    }

    //两个窗口都保持当前纵轴量程，只有AUTO按钮或A键会重新定标。
    cv::imshow(kWindowName, render_optical_field(
        optical_field,
        round_trip,
        metrics.max_power_w,
        m_cavity_y_axis_max_w,
        m_is_running,
        m_update_delay_ms,
        metrics.edge_energy_fraction,
        boundary_risk,
        low_signal,
        command.epc_delta_phase_deg));
    cv::imshow(
        kScopeWindowName,
        render_oscilloscope(
            oscilloscope_signal,
            round_trip,
            m_scope_y_axis_max_mv,
            boundary_risk));
    cv::imshow(
        kSpectrumWindowName,
        render_spectrum(
            optical_field,
            round_trip,
            low_signal,
            boundary_risk));

    const int key = cv::waitKeyEx(m_update_delay_ms);
    if (key == 27 || key == 'q' || key == 'Q')
    {
        command.should_exit = true;
        return command;
    }

    if (key == 'r' || key == 'R')
    {
        m_is_running = false;
        m_boundary_warning_latched = false;
        command.should_reset = true;
    }
    else if (is_single_step_key(key))
    {
        m_is_running = false;
        command.should_propagate = true;
    }
    else if (key == ' ')
    {
        m_is_running = !m_is_running;
        command.should_propagate = m_is_running;
    }
    else if (key == '[' || key == '{')
    {
        m_update_delay_ms = std::min(m_update_delay_ms + 50, 1000);
        command.should_propagate = m_is_running;
    }
    else if (key == ']' || key == '}')
    {
        m_update_delay_ms = std::max(m_update_delay_ms - 50, 10);
        command.should_propagate = m_is_running;
    }
    else if (key == 'a' || key == 'A')
    {
        m_auto_scale_requested = true;
        command.should_propagate = m_is_running;
    }
    else
    {
        command.should_propagate = m_is_running;
    }

    return command;
}

void OpticalFieldVisualization::on_auto_scale(int, void* user_data)
{
    auto* visualization = static_cast<OpticalFieldVisualization*>(user_data);
    if (visualization != nullptr)
        visualization->m_auto_scale_requested = true;
}
