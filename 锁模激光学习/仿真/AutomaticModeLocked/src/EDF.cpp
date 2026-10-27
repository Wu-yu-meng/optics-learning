#include "EDF.hpp"
#include "DFT.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

EDF::EDF(double g_0, double E_sat, double length, double gain_bandwidth_thz,
         double ase_noise_energy_pj_per_pass, std::uint64_t random_seed)
    : m_g_0(g_0),
      m_E_sat(E_sat),
      m_length(length),
      m_gain_bandwidth_thz(gain_bandwidth_thz),
      m_ase_noise_energy_pj_per_pass(ase_noise_energy_pj_per_pass),
      m_random_seed(random_seed),
      m_random_engine(random_seed)
{
    if (!std::isfinite(m_gain_bandwidth_thz) || m_gain_bandwidth_thz <= 0.0)
        throw std::invalid_argument("[EDF]增益带宽必须为有限正数，单位THz");
    if (!std::isfinite(m_ase_noise_energy_pj_per_pass) || m_ase_noise_energy_pj_per_pass < 0.0)
        throw std::invalid_argument("[EDF]噪声能量必须为有限非负数，单位pJ/次传播");
}

OpticalField EDF::propagate(const OpticalField &input_optical_field)
{
    
    //按照g= g_0 * (1 + E/E_t)^(-1)的模型进行增益系数计算
    //然后计算功率开根，乘到电场上

    //计算光场的能量
    double energy_j = 0;//光场能量J
    const double sample_stride_fs = input_optical_field.basic_info().sample_stride;//采样步长fs
    if (!std::isfinite(sample_stride_fs) || sample_stride_fs <= 0.0)
        throw std::invalid_argument("[EDF]采样步长必须为有限正数，单位fs");
    double sample_stride_s = sample_stride_fs * 1.0e-15;//采样步长s
    const OpticalField::Polarization &polarization = input_optical_field.polarization();//偏振
    for (size_t i = 0; i < input_optical_field.basic_info().sample_size; i++)
    {
        //瞬时功率w P(t)=|Ax(t)|²+|Ay(t)|²
        double power_w = std::norm(polarization(0,i)) + std::norm(polarization(1,i));
        
        //求和(离散积分)得到能量
        energy_j += power_w * sample_stride_s;
    }

    //计算增益系数
    double g = m_g_0 / (1 + energy_j / (m_E_sat * 1.0e-12));

    //转到频域，按各频率相对中心频率的偏移施加有限带宽增益。
    DFT::Spectrum spectrum = DFT::forward(input_optical_field);
    const Eigen::Index sample_size = spectrum.cols();
    const double frequency_stride_thz = 1000.0 / (static_cast<double>(sample_size) * sample_stride_fs);
    for (Eigen::Index column = 0; column < sample_size; ++column)
    {
        //原始DFT顺序为0、正频率、负频率；exp(i*beta*z-i*omega*t)约定下的物理频偏与DFT坐标反号。
        const Eigen::Index frequency_index = column < (sample_size + 1) / 2 ? column : column - sample_size;
        const double frequency_offset_thz = -static_cast<double>(frequency_index) * frequency_stride_thz;

        //增益系数g(f)=g*exp[-4*ln(2)*(f/B)^2]；f=±B/2时，增益系数为中心值的一半。
        //两个偏振使用相同的振幅增益exp[g(f)*L/2]，带外不再明显放大，而不是将光场直接置零。
        const double normalized_frequency = frequency_offset_thz / m_gain_bandwidth_thz;
        const double gain_profile = std::exp(-4.0 * std::log(2.0) * normalized_frequency * normalized_frequency);
        const double amplitude_gain = std::exp(0.5 * g * m_length * gain_profile);
        spectrum.col(column) *= amplitude_gain;
    }

    //恢复增益作用后的时域复包络。
    OpticalField::Polarization output_polarization = DFT::inverse(spectrum);

    if (m_ase_noise_energy_pj_per_pass > 0.0)
    {
        //沿用test的简化ASE等效模型：在频域为两个偏振分别生成复高斯噪声。
        //噪声功率谱与增益系数谱使用相同带宽，因此噪声复振幅乘高斯谱的平方根。
        DFT::Spectrum noise_spectrum(2, sample_size);
        std::normal_distribution<double> normal_distribution(0.0, 1.0);
        for (Eigen::Index column = 0; column < sample_size; ++column)
        {
            const Eigen::Index frequency_index = column < (sample_size + 1) / 2 ? column : column - sample_size;
            const double frequency_offset_thz = -static_cast<double>(frequency_index) * frequency_stride_thz;
            const double normalized_frequency = frequency_offset_thz / m_gain_bandwidth_thz;
            const double noise_power_profile = std::exp(-4.0 * std::log(2.0) * normalized_frequency * normalized_frequency);
            const double noise_amplitude = std::sqrt(noise_power_profile);
            for (Eigen::Index row = 0; row < 2; ++row)
            {
                noise_spectrum(row, column) = noise_amplitude * std::complex<double>{
                    normal_distribution(m_random_engine), normal_distribution(m_random_engine)};
            }
        }

        OpticalField::Polarization noise_polarization = DFT::inverse(noise_spectrum);
        double raw_noise_energy_j = 0.0;
        for (Eigen::Index column = 0; column < sample_size; ++column)
        {
            const double noise_power_w = std::norm(noise_polarization(0, column)) + std::norm(noise_polarization(1, column));
            raw_noise_energy_j += noise_power_w * sample_stride_s;
        }
        if (raw_noise_energy_j > 0.0)
        {
            //按双偏振、整个窗口的总能量归一化，避免噪声能量随采样点数改变。
            const double target_noise_energy_j = m_ase_noise_energy_pj_per_pass * 1.0e-12;
            noise_polarization *= std::sqrt(target_noise_energy_j / raw_noise_energy_j);
            output_polarization += noise_polarization;//增益之后加噪声，不再额外放大本次噪声
        }
    }

    //返回
    return OpticalField(std::move(output_polarization),input_optical_field.basic_info());

}

void EDF::reset_noise()
{
    m_random_engine.seed(m_random_seed);
}
