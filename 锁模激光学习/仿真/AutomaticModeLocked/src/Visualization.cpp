#include "Visualization.hpp"
#include <Eigen/Core>
#include <vector>
#include <opencv2/imgproc.hpp>

VirtualOpticalPowerVisualizer::VirtualOpticalPowerVisualizer(const std::string& name)
    : m_name("虚拟光功率器-" + name)
{
}

cv::Mat VirtualOpticalPowerVisualizer::measure(size_t loop_count, const OpticalField &optical_field)
{
    //获取x,y,total功率采样。
    Eigen::Matrix<double, 3, Eigen::Dynamic> measurement = measure(optical_field);


    //计算峰值功率和总能量
    const auto& info = optical_field.basic_info();
    const double peak_power_w = measurement.row(2).maxCoeff();
    const double energy_pj = measurement.row(2).sum() * info.sample_stride * 1.0e-3;//W*fs转换为pJ。
    
    //自适应时调整y轴，否则沿用
    if (m_auto_size.load())
    {
        m_y_axis_max_w = peak_power_w * 1.1;//顶部预留10%空间。
        m_auto_size.store(false);
    }
    const double y_axis_max_w = m_y_axis_max_w;

    //创建画布
    constexpr int width = 1280, height = 720;
    constexpr int left = 105, right = 1235, top = 125, bottom = 605;
    const cv::Scalar background(28, 28, 28), foreground(225, 225, 225), grid(58, 58, 58);
    const cv::Scalar x_color(255, 155, 70), y_color(95, 205, 110), total_color(80, 220, 255);//BGR。
    cv::Mat image(height, width, CV_8UC3, background);

    //基本注解
    cv::putText(image, "Optical field power", {left, 42}, cv::FONT_HERSHEY_SIMPLEX, 0.85, foreground, 2, cv::LINE_AA);
    cv::putText(image, "Round trip: " + std::to_string(loop_count), {left, 77}, cv::FONT_HERSHEY_SIMPLEX, 0.55, foreground, 1, cv::LINE_AA);
    cv::putText(image, cv::format("Peak: %.4g W    Energy: %.4g pJ", peak_power_w, energy_pj), {410, 77}, cv::FONT_HERSHEY_SIMPLEX, 0.55, foreground, 1, cv::LINE_AA);
    cv::putText(image, "Power (W)", {12, top - 18}, cv::FONT_HERSHEY_SIMPLEX, 0.48, foreground, 1, cv::LINE_AA);

    //时间坐标
    const Eigen::Index sample_intervals = measurement.cols() - 1;
    const double time_span_fs = static_cast<double>(sample_intervals) * info.sample_stride;
    constexpr int grid_divisions = 5;
    for (int index = 0; index <= grid_divisions; ++index)
    {
        const double ratio = static_cast<double>(index) / grid_divisions;
        const int x = left + static_cast<int>(ratio * (right - left));
        const int y = bottom - static_cast<int>(ratio * (bottom - top));
        cv::line(image, {x, top}, {x, bottom}, grid, 1, cv::LINE_AA);
        cv::line(image, {left, y}, {right, y}, grid, 1, cv::LINE_AA);
        cv::putText(image, cv::format("%.4g", info.start_time + ratio * time_span_fs), {x - 30, bottom + 27}, cv::FONT_HERSHEY_SIMPLEX, 0.42, foreground, 1, cv::LINE_AA);
        cv::putText(image, cv::format("%.4g", ratio * y_axis_max_w), {12, y + 5}, cv::FONT_HERSHEY_SIMPLEX, 0.42, foreground, 1, cv::LINE_AA);
    }
    cv::rectangle(image, {left, top}, {right, bottom}, foreground, 1, cv::LINE_AA);
    cv::putText(image, "Time (fs)", {(left + right) / 2 - 35, bottom + 57}, cv::FONT_HERSHEY_SIMPLEX, 0.48, foreground, 1, cv::LINE_AA);
    
    //把采样坐标映射为像素坐标。然后绘制
    cv::Mat plot_image = image(cv::Rect(left, top, right - left + 1, bottom - top + 1));
    const int rows[] = {2, 0, 1};
    const cv::Scalar colors[] = {total_color, x_color, y_color};
    const char* labels[] = {"Total", "X", "Y"};
    for (int curve = 0; curve < 3; ++curve)
    {
        std::vector<cv::Point> points;
        points.reserve(static_cast<std::size_t>(measurement.cols()));
        for (Eigen::Index column = 0; column < measurement.cols(); ++column)
        {
            const int x = left + static_cast<int>(static_cast<double>(column) / sample_intervals * (right - left));
            const double ratio = measurement(rows[curve], column) / y_axis_max_w;
            const int y = bottom - static_cast<int>(ratio * (bottom - top));
            points.emplace_back(x - left, y - top);//子图像使用相对绘图区左上角的坐标。
        }
        cv::polylines(plot_image, points, false, colors[curve], curve == 0 ? 2 : 1, cv::LINE_AA);

        const int legend_x = left + curve * 130;
        cv::line(image, {legend_x, 687}, {legend_x + 30, 687}, colors[curve], 2, cv::LINE_AA);
        cv::putText(image, labels[curve], {legend_x + 40, 692}, cv::FONT_HERSHEY_SIMPLEX, 0.48, foreground, 1, cv::LINE_AA);
    }
    cv::putText(image, cv::format("Y scale: %.4g W", y_axis_max_w), {650, 692}, cv::FONT_HERSHEY_SIMPLEX, 0.46, foreground, 1, cv::LINE_AA);

    //返回图像，由外部可视化管理器负责显示和处理按键。
    return image;
}


Eigen::Matrix<double, 3, Eigen::Dynamic> VirtualOpticalPowerVisualizer::measure(const OpticalField &optical_field)
{
    //准备输出容器x,y,total
    Eigen::Matrix<double, 3, Eigen::Dynamic> measurement(3, optical_field.polarization().cols());

    //遍历进行计算
    const auto& polarization = optical_field.polarization();
    for (size_t i = 0; i < optical_field.basic_info().sample_size; i++)
    {
        measurement(0, i) = std::norm(polarization(0, i));
        measurement(1, i) = std::norm(polarization(1, i));
        measurement(2, i) = measurement(0, i) + measurement(1, i);
    }

    return measurement;
}

void VirtualOpticalPowerVisualizer::auto_size()
{
    m_auto_size.store(true);
};

const std::string& VirtualOpticalPowerVisualizer::name() const
{
    return m_name;
}
