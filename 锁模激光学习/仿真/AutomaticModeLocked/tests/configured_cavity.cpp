#include "Builder.hpp"
#include "optical_element/SMF.hpp"
#include "optical_element/EDF.hpp"
#include "optical_element/EPC.hpp"
#include "optical_element/OC.hpp"
#include "optical_element/PD.hpp"
#include <iostream>
#include <stdexcept>

void same_field(const OpticalField& actual, const OpticalField& expected, const std::string& name)
{
    const OpticalField::BasicInfo &a = actual.basic_info();
    const OpticalField::BasicInfo &b = expected.basic_info();
    if (a.sample_size != b.sample_size || a.sample_stride != b.sample_stride || a.start_time != b.start_time ||
        a.wave_length != b.wave_length || a.center_frequency != b.center_frequency ||
        (actual.polarization() - expected.polarization()).cwiseAbs().maxCoeff() != 0.0)
        throw std::runtime_error("配置光路与手写光路不一致：" + name);
}

int main(int argc, char **argv)
{
    try
    {
        if (argc != 2)
            throw std::runtime_error("需要默认配置路径");
        const cv::FileStorage config(argv[1],cv::FileStorage::READ);
        Builder builder{ReJson{std::string(argv[1])}};
        OpticalCavity cavity = builder.build();
        OpticalField field = init_gauss_optical_field(config);
        EPC epc(std::vector<EPC::Retarder>{{0,0.0,0.0},{1,45.0,0.0},{2,0.0,0.0},{3,45.0,0.0}});
        SMF smf(0.013,5.0,-21.7,0.25);
        EDF edf(0.4,100.0,1.0,4.0,1.0e-6,20261018,10.0);
        OC oc(0.9,0.98);
        PD pd(45.0);

        //包含运行中修改EPC、EDF连续噪声、OC两个输出和反馈覆盖。
        const double phases1[] = {0.0,347.0,325.0};
        const double phases2[] = {0.0,0.0,74.0};
        for (size_t round = 0; round < 3; ++round)
        {
            epc.set_retarder(0,phases1[round]);
            epc.set_retarder(1,phases2[round]);
            dynamic_cast<EPC&>(cavity.element("epc1")).set_retarder(0,phases1[round]);
            dynamic_cast<EPC&>(cavity.element("epc1")).set_retarder(1,phases2[round]);
            OpticalOutputs epc_output = epc.propagate({{"in",std::cref(field)}});
            OpticalOutputs smf_output = smf.propagate({{"in",std::cref(epc_output.at("out"))}});
            OpticalOutputs edf_output = edf.propagate({{"in",std::cref(smf_output.at("out"))}});
            OpticalOutputs oc_output = oc.propagate({{"in",std::cref(edf_output.at("out"))}});
            OpticalOutputs pd_output = pd.propagate({{"in",std::cref(oc_output.at("cavity"))}});
            cavity.run_once();
            same_field(cavity.field("epc1-out"),epc_output.at("out"),"EPC");
            same_field(cavity.field("smf1-out"),smf_output.at("out"),"SMF");
            same_field(cavity.field("edf1-out"),edf_output.at("out"),"EDF");
            same_field(cavity.field("oc1-cavity"),oc_output.at("cavity"),"OC cavity");
            same_field(cavity.field("oc1-extracted"),oc_output.at("extracted"),"OC extracted");
            same_field(cavity.field("pd1-out"),pd_output.at("out"),"PD");
            field = pd_output.at("out");
            if (cavity.loop_count() != round + 1)
                throw std::runtime_error("循环次数错误");
        }

        cavity.reset();
        edf.reset_noise();
        field = init_gauss_optical_field(config);
        same_field(cavity.field("pd1-out"),field,"重置后的初始光场");
        OpticalOutputs epc_output = epc.propagate({{"in",std::cref(field)}});
        OpticalOutputs smf_output = smf.propagate({{"in",std::cref(epc_output.at("out"))}});
        OpticalOutputs edf_output = edf.propagate({{"in",std::cref(smf_output.at("out"))}});
        OpticalOutputs oc_output = oc.propagate({{"in",std::cref(edf_output.at("out"))}});
        OpticalOutputs pd_output = pd.propagate({{"in",std::cref(oc_output.at("cavity"))}});
        cavity.run_once();
        same_field(cavity.field("pd1-out"),pd_output.at("out"),"重置后的种子与EPC参数");

        //另一份配置使用不同采样网格，以及各自独立的两段SMF步长。
        const std::string variant = R"({
            "version":1,"scientific_constants":{"light_speed":299792458},
            "init_gauss_optical_field":{"wave_length_nm":1545,"sample_stride_fs":8,"sample_size":1025,
                "fwhm_time_fs":130,"peak_power_w":2.3,"polarization_angle_deg":30,"relative_phase_deg":20},
            "elements":[
                {"name":"fiberA","type":"SMF","parameters":{"gamma_per_w_m":0.013,"length_m":0.2,
                    "beta2_ps2_per_km":-21.7,"step_length_m":0.02},"connections":{"in":"fiberB-out"}},
                {"name":"fiberB","type":"SMF","parameters":{"gamma_per_w_m":0.004,"length_m":0.3,
                    "beta2_ps2_per_km":17,"step_length_m":0.07},"connections":{"in":"fiberA-out"}}
            ],"observations":[{"name":"fiber","type":"VirtualOpticalPowerVisualizer","source":"fiberB-out"}]
        })";
        ReJson variant_config{std::string(argv[1])};
        variant_config.config_ = cv::FileStorage(variant,cv::FileStorage::READ | cv::FileStorage::MEMORY);
        OpticalField variant_field = init_gauss_optical_field(variant_config.config_);
        Builder variant_builder{std::move(variant_config)};
        OpticalCavity variant_cavity = variant_builder.build();
        SMF fiberA(0.013,0.2,-21.7,0.02);
        SMF fiberB(0.004,0.3,17.0,0.07);
        for (int round = 0; round < 2; ++round)
        {
            OpticalOutputs outputA = fiberA.propagate({{"in",std::cref(variant_field)}});
            OpticalOutputs outputB = fiberB.propagate({{"in",std::cref(outputA.at("out"))}});
            variant_cavity.run_once();
            same_field(variant_cavity.field("fiberA-out"),outputA.at("out"),"独立SMF步长A");
            same_field(variant_cavity.field("fiberB-out"),outputB.at("out"),"独立SMF步长B");
            variant_field = outputB.at("out");
        }
        std::cout << "PASS: configured/manual fields match exactly at all six ports; EPC/noise reset and independent config grids/SMF steps match.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
