#include "EDF.hpp"
#include "FourierTransform.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace
{

//高斯功率谱：在frequency_thz=±bandwidth_thz/2处取值1/2。
double gaussian_power_profile(
    double frequency_thz,
    double bandwidth_thz)
{
    return std::exp(
        -4.0 * std::log(2.0) * frequency_thz * frequency_thz /
        (bandwidth_thz * bandwidth_thz));
}

}

EDF::EDF(
    double g_0,
    double E_sat,
    double length,
    double ase_noise_energy_pj_per_pass,
    double gain_bandwidth_thz,
    std::uint64_t random_seed)
    : m_g_0(g_0),
      m_E_sat(E_sat),
      m_length(length),
      m_ase_noise_energy_pj_per_pass(ase_noise_energy_pj_per_pass),
      m_gain_bandwidth_thz(gain_bandwidth_thz),
      m_random_seed(random_seed),
      m_random_engine(random_seed)
{
    if (m_g_0 < 0.0)
        throw std::invalid_argument("[EDF]小信号增益系数不能小于0");
    if (m_E_sat <= 0.0)
        throw std::invalid_argument("[EDF]饱和能量必须大于0");
    if (m_length < 0.0)
        throw std::invalid_argument("[EDF]长度不能小于0");
    if (m_ase_noise_energy_pj_per_pass < 0.0)
        throw std::invalid_argument("[EDF]ASE噪声能量不能小于0");
    if (m_gain_bandwidth_thz <= 0.0)
        throw std::invalid_argument("[EDF]增益带宽必须大于0");
}

OpticalField EDF::propagate(const OpticalField &input_optical_field)
{
    
    //按照g= g_0 * (1 + E/E_t)^(-1)的模型进行增益系数计算
    //然后计算功率开根，乘到电场上

    //计算光场的能量
    double energy_j = 0;//光场能量J
    double sample_stride_s = input_optical_field.basic_info().sample_stride * 1.0e-15;//采样步长s
    const OpticalField::Polarization &polarization = input_optical_field.polarization();//偏振
    for (size_t i = 0; i < input_optical_field.basic_info().sample_size; i++)
    {
        //瞬时功率w P(t)=|Ax(t)|²+|Ay(t)|²
        double power_w = std::norm(polarization(0,i)) + std::norm(polarization(1,i));
        
        //求和(离散积分)得到能量
        energy_j += power_w * sample_stride_s;
    }

    //计算增益系数
    const double g = m_g_0 / (1.0 + energy_j / (m_E_sat * 1.0e-12));

    //转到频域后施加有限带宽增益。这里令增益系数g(f)呈高斯分布：
    //g(f)=g_center*G_profile(f)，因此振幅增益为exp[g(f)*L/2]。
    //频带外的G_profile趋近0，它们在EDF中不再获得增益，会由OC和PD等腔损耗逐圈淘汰。
    OpticalField::Polarization output_polarization = polarization;
    fourier::forward_in_place(output_polarization);
    for (std::size_t i = 0; i < input_optical_field.basic_info().sample_size; ++i)
    {
        const double frequency_thz = fourier::bin_frequency_thz(
            i,
            input_optical_field.basic_info().sample_size,
            input_optical_field.basic_info().sample_stride);
        const double gain_profile = gaussian_power_profile(
            frequency_thz,
            m_gain_bandwidth_thz);
        const double spectral_amplitude_gain = std::exp(
            0.5 * g * m_length * gain_profile);
        output_polarization.col(static_cast<Eigen::Index>(i)) *=
            spectral_amplitude_gain;
    }
    fourier::inverse_in_place(output_polarization);

    if (m_ase_noise_energy_pj_per_pass > 0.0)
    {
        //在频域生成零均值复高斯噪声，并用高斯光谱把它限制在EDF的有效带宽内。
        //如果直接在时域加白噪声，当前采样率会令噪声占满数百THz，不符合掺铒光纤的实际增益带宽。
        OpticalField::Polarization noise_spectrum(
            2,
            static_cast<Eigen::Index>(input_optical_field.basic_info().sample_size));
        std::normal_distribution<double> normal_distribution(0.0, 1.0);

        for (std::size_t i = 0; i < input_optical_field.basic_info().sample_size; ++i)
        {
            const double frequency_thz = fourier::bin_frequency_thz(
                i,
                input_optical_field.basic_info().sample_size,
                input_optical_field.basic_info().sample_stride);

            //ASE功率谱与EDF增益谱使用相同带宽，复振幅包络是功率谱的平方根。
            const double spectral_amplitude = std::sqrt(
                gaussian_power_profile(frequency_thz, m_gain_bandwidth_thz));

            for (Eigen::Index row = 0; row < noise_spectrum.rows(); ++row)
            {
                noise_spectrum(row, static_cast<Eigen::Index>(i)) =
                    spectral_amplitude * std::complex<double>{
                        normal_distribution(m_random_engine),
                        normal_distribution(m_random_engine)};
            }
        }

        fourier::inverse_in_place(noise_spectrum);

        //将本次随机噪声归一化到设定的总能量。这使噪声强度不会随采样点数改变。
        const double sample_stride_s = input_optical_field.basic_info().sample_stride * 1.0e-15;
        double raw_noise_energy_j = 0.0;
        for (Eigen::Index column = 0; column < noise_spectrum.cols(); ++column)
        {
            raw_noise_energy_j +=
                (std::norm(noise_spectrum(0, column)) +
                 std::norm(noise_spectrum(1, column))) *
                sample_stride_s;
        }

        if (raw_noise_energy_j > 0.0)
        {
            const double target_noise_energy_j = m_ase_noise_energy_pj_per_pass * 1.0e-12;
            noise_spectrum *= std::sqrt(target_noise_energy_j / raw_noise_energy_j);
            output_polarization += noise_spectrum;
        }
    }

    //返回
    return OpticalField(std::move(output_polarization),input_optical_field.basic_info());

}

void EDF::reset_noise()
{
    m_random_engine.seed(m_random_seed);
}
