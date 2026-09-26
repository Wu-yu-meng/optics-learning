#include "OpticalWave.hpp"
#include "TimeMapping.hpp"
#include "time/time.hpp"
#include "img_viz.hpp"

#include <cmath>
#include <opencv2/imgproc.hpp>

//fft仿真

int main()
{
    const double carrier_frequency = 500;//载波频率thz
    const double init_phase = 0;//初始相位
    const double amplitude = 1;//最大电场振幅
    const double pulse_width_fs = 100;//半峰全宽时间长度fs
    const timetool::Timestamp reference_time = timetool::now();//参考时间
    
    //创建高斯脉冲
    GaussianPluse gaussian_pluse(
        carrier_frequency,
        init_phase,
        amplitude,
        pulse_width_fs,
        reference_time
    );

    //设置采样参数
    const int sample_size = 1000;//采样数目：1000
    const double sample_stride_fs = 0.9;//采样间隔（映射后）：0.5fs
    std::array<double,sample_size> gaussian_pluse_sample;//采样队列
    
    //在0位置进行采样
    for (size_t i = 0; i < sample_size; i++)
    {
        //计算时间偏移
        auto time_offset = static_cast<std::chrono::duration<double,std::micro>>(
        ((double)i - static_cast<double>(0.5 * sample_size)) * sample_stride_fs / pulsefft::default_simulation_fs_per_real_us
        );

        //递推时间
        timetool::Timestamp time = reference_time + std::chrono::duration_cast<timetool::Timestamp::duration>(time_offset);
    
        //采样
        gaussian_pluse_sample[i] = gaussian_pluse.amplitude(time,0.0);
    }
    
    //todo在此处进行变换

    // 将时域电场采样写入 OpenCV 的实数数组。
    cv::Mat time_domain_signal(1, sample_size, CV_64F);
    for (int i = 0; i < sample_size; ++i)
    {
        time_domain_signal.at<double>(0, i) = gaussian_pluse_sample[i];
    }

    // 计算 DFT。OpenCV 内部会使用快速算法完成这一变换。
    // 每个输出点都有实部和虚部，因此输出类型为 CV_64FC2。
    cv::Mat frequency_domain_signal;
    cv::dft(
        time_domain_signal,
        frequency_domain_signal,
        cv::DFT_COMPLEX_OUTPUT);

    // 输入是实数电场，正、负频率的频谱对称，所以只显示 0 到奈奎斯特频率。
    constexpr int positive_frequency_sample_size = sample_size / 2 + 1;
    std::array<double, positive_frequency_sample_size> spectral_intensity{};

    double maximum_spectral_intensity = 0.0;
    int peak_frequency_index = 0;
    for (int k = 0; k < positive_frequency_sample_size; ++k)
    {
        const cv::Vec2d coefficient = frequency_domain_signal.at<cv::Vec2d>(0, k);

        // 用离散求和近似连续傅里叶积分时，需要乘以采样间隔 Δt。
        const double real_part = coefficient[0] * sample_stride_fs;
        const double imaginary_part = coefficient[1] * sample_stride_fs;

        // 频谱强度与复频谱的模平方 |F|^2 成正比。
        spectral_intensity[k] =
            real_part * real_part + imaginary_part * imaginary_part;

        if (spectral_intensity[k] > maximum_spectral_intensity)
        {
            maximum_spectral_intensity = spectral_intensity[k];
            peak_frequency_index = k;
        }
    }

    // 将频谱强度归一化到 0 到 1，方便显示。
    if (maximum_spectral_intensity > 0.0)
    {
        for (double& intensity : spectral_intensity)
        {
            intensity /= maximum_spectral_intensity;
        }
    }

    constexpr int image_width = 1200;
    constexpr int image_height = 480;
    constexpr int left_margin = 60;
    constexpr int right_margin = 20;
    constexpr int top_margin = 20;
    constexpr int bottom_margin = 40;
    const cv::Vec3b background_color(20, 20, 20);
    const cv::Vec3b axis_color(90, 90, 90);
    const cv::Vec3b grid_color(50, 50, 50);
    const cv::Scalar text_color(210, 210, 210);
    const cv::Vec3b time_signal_color(0, 220, 255);
    const cv::Vec3b spectrum_color(80, 220, 80);

    // 不额外引入 OpenCV imgproc，在此处用 Bresenham 算法画线。
    const auto draw_line = [](cv::Mat& image,
                              int x0,
                              int y0,
                              int x1,
                              int y1,
                              const cv::Vec3b& color) {
        const int dx = std::abs(x1 - x0);
        const int sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0);
        const int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;

        while (true)
        {
            if (x0 >= 0 && x0 < image.cols && y0 >= 0 && y0 < image.rows)
            {
                image.at<cv::Vec3b>(y0, x0) = color;
            }

            if (x0 == x1 && y0 == y1)
            {
                break;
            }

            const int twice_error = 2 * error;
            if (twice_error >= dy)
            {
                error += dy;
                x0 += sx;
            }
            if (twice_error <= dx)
            {
                error += dx;
                y0 += sy;
            }
        }
    };

    // 可视化时域电场 E(t)。
    cv::Mat time_domain_image(
        image_height,
        image_width,
        CV_8UC3,
        cv::Scalar(background_color[0], background_color[1], background_color[2]));

    const int time_axis_y = image_height / 2;
    draw_line(
        time_domain_image,
        left_margin,
        time_axis_y,
        image_width - right_margin,
        time_axis_y,
        axis_color);
    draw_line(
        time_domain_image,
        left_margin,
        top_margin,
        left_margin,
        image_height - bottom_margin,
        axis_color);

    double maximum_absolute_field = 0.0;
    for (const double field : gaussian_pluse_sample)
    {
        const double absolute_field = std::abs(field);
        if (absolute_field > maximum_absolute_field)
        {
            maximum_absolute_field = absolute_field;
        }
    }
    if (maximum_absolute_field == 0.0)
    {
        maximum_absolute_field = 1.0;
    }

    int previous_time_x = left_margin;
    int previous_time_y = time_axis_y;
    for (int i = 0; i < sample_size; ++i)
    {
        const int x = left_margin
                      + i * (image_width - left_margin - right_margin)
                            / (sample_size - 1);
        const int y = static_cast<int>(std::lround(
            time_axis_y
            - gaussian_pluse_sample[i] / maximum_absolute_field
                  * (image_height - top_margin - bottom_margin) * 0.45));

        if (i > 0)
        {
            draw_line(
                time_domain_image,
                previous_time_x,
                previous_time_y,
                x,
                y,
                time_signal_color);
        }
        previous_time_x = x;
        previous_time_y = y;
    }

    // 可视化归一化的正频率频谱强度 |F(f)|^2。
    constexpr int frequency_image_width = 1200;
    constexpr int frequency_image_height = 520;
    constexpr int frequency_left_margin = 100;
    constexpr int frequency_right_margin = 30;
    constexpr int frequency_top_margin = 20;
    constexpr int frequency_bottom_margin = 80;
    cv::Mat frequency_domain_image(
        frequency_image_height,
        frequency_image_width,
        CV_8UC3,
        cv::Scalar(background_color[0], background_color[1], background_color[2]));

    const int frequency_axis_y =
        frequency_image_height - frequency_bottom_margin;

    // 横坐标范围由采样间隔决定：1 fs^(-1) = 1000 THz。
    const double sampling_frequency_thz = 1000.0 / sample_stride_fs;
    const double nyquist_frequency_thz = sampling_frequency_thz / 2.0;
    const double peak_frequency_thz =
        peak_frequency_index * sampling_frequency_thz / sample_size;

    // 画横坐标刻度：0、200、400、600、800、1000 THz。
    constexpr int frequency_tick_interval_count = 5;
    for (int tick_index = 0;
         tick_index <= frequency_tick_interval_count;
         ++tick_index)
    {
        const int x = frequency_left_margin
                      + tick_index
                            * (frequency_image_width
                               - frequency_left_margin
                               - frequency_right_margin)
                            / frequency_tick_interval_count;
        const double frequency_thz =
            nyquist_frequency_thz * tick_index / frequency_tick_interval_count;

        draw_line(
            frequency_domain_image,
            x,
            frequency_top_margin,
            x,
            frequency_axis_y,
            grid_color);
        draw_line(
            frequency_domain_image,
            x,
            frequency_axis_y,
            x,
            frequency_axis_y + 7,
            axis_color);

        const std::string tick_text =
            std::to_string(static_cast<int>(std::lround(frequency_thz)));
        int baseline = 0;
        const cv::Size text_size = cv::getTextSize(
            tick_text,
            cv::FONT_HERSHEY_SIMPLEX,
            0.55,
            1,
            &baseline);
        cv::putText(
            frequency_domain_image,
            tick_text,
            cv::Point(x - text_size.width / 2, frequency_axis_y + 28),
            cv::FONT_HERSHEY_SIMPLEX,
            0.55,
            text_color,
            1,
            cv::LINE_AA);
    }

    // 画纵坐标刻度：0、0.25、0.50、0.75、1.00。
    constexpr int intensity_tick_interval_count = 4;
    const std::array<std::string, intensity_tick_interval_count + 1>
        intensity_tick_texts{"0", "0.25", "0.50", "0.75", "1.00"};
    for (int tick_index = 0;
         tick_index <= intensity_tick_interval_count;
         ++tick_index)
    {
        const int y = frequency_axis_y
                      - tick_index
                            * (frequency_axis_y - frequency_top_margin)
                            / intensity_tick_interval_count;

        draw_line(
            frequency_domain_image,
            frequency_left_margin,
            y,
            frequency_image_width - frequency_right_margin,
            y,
            grid_color);
        draw_line(
            frequency_domain_image,
            frequency_left_margin - 7,
            y,
            frequency_left_margin,
            y,
            axis_color);

        int baseline = 0;
        const cv::Size text_size = cv::getTextSize(
            intensity_tick_texts[tick_index],
            cv::FONT_HERSHEY_SIMPLEX,
            0.55,
            1,
            &baseline);
        cv::putText(
            frequency_domain_image,
            intensity_tick_texts[tick_index],
            cv::Point(
                frequency_left_margin - text_size.width - 12,
                y + text_size.height / 2),
            cv::FONT_HERSHEY_SIMPLEX,
            0.55,
            text_color,
            1,
            cv::LINE_AA);
    }

    // 网格线之后重新绘制坐标轴，保证轴线清晰可见。
    draw_line(
        frequency_domain_image,
        frequency_left_margin,
        frequency_axis_y,
        frequency_image_width - frequency_right_margin,
        frequency_axis_y,
        axis_color);

    // 横坐标名称和单位。
    const std::string frequency_axis_label = "Frequency f (THz)";
    int frequency_label_baseline = 0;
    const cv::Size frequency_label_size = cv::getTextSize(
        frequency_axis_label,
        cv::FONT_HERSHEY_SIMPLEX,
        0.65,
        1,
        &frequency_label_baseline);
    cv::putText(
        frequency_domain_image,
        frequency_axis_label,
        cv::Point(
            frequency_left_margin
                + (frequency_image_width
                   - frequency_left_margin
                   - frequency_right_margin
                   - frequency_label_size.width) / 2,
            frequency_image_height - 18),
        cv::FONT_HERSHEY_SIMPLEX,
        0.65,
        text_color,
        1,
        cv::LINE_AA);

    // 纵坐标名称和单位。先横向写字，再将小图旋转后贴到主图左侧。
    const std::string intensity_axis_label =
        "Normalized spectral intensity |F(f)|^2 (a.u.)";
    int intensity_label_baseline = 0;
    const cv::Size intensity_label_size = cv::getTextSize(
        intensity_axis_label,
        cv::FONT_HERSHEY_SIMPLEX,
        0.55,
        1,
        &intensity_label_baseline);
    cv::Mat horizontal_intensity_label(
        intensity_label_size.height + intensity_label_baseline + 10,
        intensity_label_size.width + 10,
        CV_8UC3,
        cv::Scalar(background_color[0], background_color[1], background_color[2]));
    cv::putText(
        horizontal_intensity_label,
        intensity_axis_label,
        cv::Point(5, intensity_label_size.height + 5),
        cv::FONT_HERSHEY_SIMPLEX,
        0.55,
        text_color,
        1,
        cv::LINE_AA);

    cv::Mat vertical_intensity_label;
    cv::rotate(
        horizontal_intensity_label,
        vertical_intensity_label,
        cv::ROTATE_90_COUNTERCLOCKWISE);
    const int intensity_label_y =
        (frequency_image_height - vertical_intensity_label.rows) / 2;
    vertical_intensity_label.copyTo(
        frequency_domain_image(cv::Rect(
            5,
            intensity_label_y,
            vertical_intensity_label.cols,
            vertical_intensity_label.rows)));
    draw_line(
        frequency_domain_image,
        frequency_left_margin,
        frequency_top_margin,
        frequency_left_margin,
        frequency_axis_y,
        axis_color);

    int previous_frequency_x = frequency_left_margin;
    int previous_frequency_y = frequency_axis_y;
    for (int k = 0; k < positive_frequency_sample_size; ++k)
    {
        const int x = frequency_left_margin
                      + k * (frequency_image_width
                             - frequency_left_margin
                             - frequency_right_margin)
                            / (positive_frequency_sample_size - 1);
        const int y = static_cast<int>(std::lround(
            frequency_axis_y
            - spectral_intensity[k]
                  * (frequency_axis_y - frequency_top_margin)));

        if (k > 0)
        {
            draw_line(
                frequency_domain_image,
                previous_frequency_x,
                previous_frequency_y,
                x,
                y,
                spectrum_color);
        }
        previous_frequency_x = x;
        previous_frequency_y = y;
    }

    // 在左上角标出频谱强度最大值所对应的频率分量。
    const std::string peak_frequency_text = cv::format(
        "Peak frequency: %.1f THz",
        peak_frequency_thz);
    int peak_text_baseline = 0;
    const cv::Size peak_text_size = cv::getTextSize(
        peak_frequency_text,
        cv::FONT_HERSHEY_SIMPLEX,
        0.65,
        1,
        &peak_text_baseline);
    const cv::Point peak_text_origin(
        frequency_left_margin + 14,
        frequency_top_margin + peak_text_size.height + 14);
    cv::rectangle(
        frequency_domain_image,
        cv::Rect(
            peak_text_origin.x - 8,
            peak_text_origin.y - peak_text_size.height - 8,
            peak_text_size.width + 16,
            peak_text_size.height + peak_text_baseline + 16),
        cv::Scalar(background_color[0], background_color[1], background_color[2]),
        cv::FILLED);
    cv::putText(
        frequency_domain_image,
        peak_frequency_text,
        peak_text_origin,
        cv::FONT_HERSHEY_SIMPLEX,
        0.65,
        text_color,
        1,
        cv::LINE_AA);

    ImgViz::init(true);
    ImgViz::enqueue_image_copy(
        "Gaussian pulse: E(t), -250 fs to 249.5 fs",
        time_domain_image);
    ImgViz::enqueue_image_copy(
        "Gaussian pulse FFT: normalized |F(f)|^2, 0 THz to 1000 THz",
        frequency_domain_image);

    // 图像由 ImgViz 的工作线程显示，主线程保持运行以免窗口立即关闭。
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }


    return 0;
}
