/* 配置数据与浏览器界面分离，Node测试和直接打开的页面使用同一份模型。 */
(function (root) {
  "use strict";
  const field = (key, label, unit, value, extra = {}) => ({ key, label, unit, value, ...extra });
  const types = {
    EPC: { label: "偏振控制器", symbol: "↗", color: "#7c73f5", description: "独立设置每个延迟器的主轴和相位。", fields: [] },
    SMF: { label: "单模光纤", symbol: "∿", color: "#318bdf", description: "Kerr 非线性与二阶色散分段交替传播。", fields: [
      field("gamma_per_w_m", "非线性系数 γ", "W⁻¹ m⁻¹", .013, { min: 0 }),
      field("length_m", "光纤长度", "m", 5, { min: 0 }),
      field("beta2_ps2_per_km", "二阶色散 β₂", "ps²/km", -21.7),
      field("step_length_m", "分段长度", "m", .25, { positive: true })
    ] },
    EDF: { label: "掺铒增益光纤", symbol: "+", color: "#159f80", description: "有限增益带宽、饱和增益与带限 ASE 等效噪声。", fields: [
      field("g_0_per_m", "小信号增益", "m⁻¹", .4, { min: 0 }),
      field("saturation_energy_pj", "饱和能量", "pJ", 100, { positive: true }),
      field("length_m", "光纤长度", "m", 1, { min: 0 }),
      field("gain_bandwidth_thz", "增益带宽 FWHM", "THz", 4, { positive: true }),
      field("noise_energy_pj_per_pass", "每圈噪声能量", "pJ", 1e-6, { min: 0 }),
      field("random_seed", "随机种子", "integer", 20261018, { min: 0, max: Number.MAX_SAFE_INTEGER, integer: true }),
      field("noise_bandwidth_thz", "噪声带宽 FWHM", "THz", 10, { positive: true })
    ] },
    OC: { label: "输出耦合器", symbol: "⑂", color: "#d19029", description: "cavity 留在腔内，extracted 作为提取输出。", fields: [
      field("cavity_ratio", "留腔功率比例", "0—1", .9, { min: 0, max: 1 }),
      field("power_transmittance", "器件功率透过率", "0—1", .98, { min: 0, max: 1 })
    ] },
    PD: { label: "理想检偏器", symbol: "∥", color: "#c26786", description: "将光场投影到指定透光轴方向。", fields: [
      field("transmission_axis_angle_deg", "透光轴方向", "deg", 45)
    ] }
  };
  const gaussianFields = [
    field("wave_length_nm", "中心波长", "nm", 1550, { positive: true }),
    field("sample_stride_fs", "采样间隔 Δt", "fs", 5, { positive: true }),
    field("sample_size", "采样点数 N", "samples", 16384, { min: 2, max: 1048576, integer: true }),
    field("fwhm_time_fs", "初始脉冲 FWHM", "fs", 100, { positive: true }),
    field("peak_power_w", "初始峰值功率", "W", 1, { min: 0 }),
    field("polarization_angle_deg", "初始偏振角", "deg", 45),
    field("relative_phase_deg", "Y 相对 X 相位", "deg", 0)
  ];
  const runtimeFields = [
    field("frame_delay_ms", "播放等待时间", "ms", 20, { min: 1, max: 60000, integer: true }),
    field("render_every_n_rounds", "绘制间隔", "rounds", 1, { min: 1, max: 1000000, integer: true }),
    field("max_rounds", "停止圈数 · 0 为持续运行", "rounds", 0, { min: 0, max: Number.MAX_SAFE_INTEGER, integer: true })
  ];
  const clone = value => JSON.parse(JSON.stringify(value));
  function createElement(type, name) {
    if (!types[type]) throw new Error("不支持的器件类型：" + type);
    const parameters = Object.fromEntries(types[type].fields.map(f => [f.key, f.value]));
    if (type === "EPC") parameters.retarders = [0, 45, 0, 45].map((axis, ID) => ({ ID, orientation_deg: axis, delta_phase_deg: 0 }));
    return { name, type, parameters, connections: { in: "" } };
  }
  const ports = element => element.type === "OC" ? ["cavity", "extracted"] : ["out"];
  const outputs = config => config.elements.flatMap(element => ports(element).map(port => ({ key: element.name + "-" + port, name: element.name, port, type: element.type })));
  function connectSerial(config, forwardPorts = {}) {
    config.elements.forEach((element, index) => {
      const previous = config.elements[(index + config.elements.length - 1) % config.elements.length];
      const port = forwardPorts[previous.name] || (previous.type === "OC" ? "cavity" : "out");
      element.connections = { in: previous.name + "-" + port };
    });
    return config;
  }
  function defaultConfig() {
    const config = {
      version: 1, name: "默认五器件腔", scientific_constants: { light_speed: 299792458 },
      init_gauss_optical_field: Object.fromEntries(gaussianFields.map(f => [f.key, f.value])),
      elements: ["EPC", "SMF", "EDF", "OC", "PD"].map(type => createElement(type, type.toLowerCase() + "1")),
      observations: [
        { name: "腔内光场", type: "VirtualOpticalPowerVisualizer", source: "pd1-out" },
        { name: "耦合输出", type: "VirtualOpticalPowerVisualizer", source: "oc1-extracted" }
      ],
      runtime: { start_paused: false, frame_delay_ms: 20, epc_controls: true, render_every_n_rounds: 1, max_rounds: 0 }
    };
    return connectSerial(config);
  }
  function normalize(input) {
    let config = clone(input);
    if (!config || typeof config !== "object" || Array.isArray(config)) throw new Error("配置必须是一个 JSON 对象");
    if (!config.elements && config.init_gauss_optical_field) {
      const legacy = config;
      config = defaultConfig();
      Object.assign(config.init_gauss_optical_field, legacy.init_gauss_optical_field);
      Object.assign(config.scientific_constants, legacy.scientific_constants);
      if (legacy.SMF?.step_length_m !== undefined) config.elements[1].parameters.step_length_m = legacy.SMF.step_length_m;
    }
    if (!Array.isArray(config.elements) || !config.elements.length) throw new Error("elements 必须是非空数组");
    for (const element of config.elements) {
      if (!types[element.type] || !element.parameters || !element.connections) throw new Error("器件需要已支持的 type、parameters 和 connections");
      if (element.type === "EPC" && !Array.isArray(element.parameters.retarders)) throw new Error("EPC 需要 retarders 数组");
    }
    config.runtime = Object.assign({}, defaultConfig().runtime, config.runtime);
    config.observations ??= [];
    return config;
  }
  function validate(config) {
    const errors = [];
    if (config.version !== 1) errors.push("version 必须为 1");
    function numeric(value, definition, path) {
      if (typeof value !== "number" || !Number.isFinite(value)) errors.push(path + " 需要有限数值");
      else if ((definition.positive && value <= 0) || (definition.min !== undefined && value < definition.min) || (definition.max !== undefined && value > definition.max) || (definition.integer && !Number.isSafeInteger(value))) errors.push(path + " 超出允许范围");
    }
    numeric(config.scientific_constants?.light_speed, { positive: true }, "scientific_constants.light_speed");
    gaussianFields.forEach(f => numeric(config.init_gauss_optical_field?.[f.key], f, "初始光场 · " + f.label));
    const names = new Set();
    const available = new Set(outputs(config).map(output => output.key));
    config.elements.forEach((element, index) => {
      if (!/^[A-Za-z][A-Za-z0-9_]*$/.test(element.name) || names.has(element.name)) errors.push("第 " + (index + 1) + " 个器件的名称需要唯一，并以英文字母开头");
      names.add(element.name);
      if (!types[element.type]) { errors.push("不支持的器件：" + element.type); return; }
      types[element.type].fields.forEach(f => numeric(element.parameters?.[f.key], f, element.name + " · " + f.label));
      if (element.type === "EPC") {
        const retarders = element.parameters.retarders;
        if (!retarders.length) errors.push(element.name + " 至少需要一个延迟器");
        const ids = [...retarders].map(r => r.ID).sort((a, b) => a - b);
        if (ids.some((id, i) => id !== i || id > 1023)) errors.push(element.name + " 的通道 ID 必须从 0 连续编号");
        retarders.forEach(r => {
          numeric(r.orientation_deg, {}, element.name + " · 主轴方向");
          numeric(r.delta_phase_deg, { min: 0, max: 360 }, element.name + " · 相位延迟");
        });
      }
      if (Object.keys(element.connections).length !== 1 || !available.has(element.connections.in)) errors.push(element.name + " 的 in 输入需要连接到有效输出端口");
    });
    const observationNames = new Set();
    if (!Array.isArray(config.observations)) errors.push("observations 需要数组");
    else config.observations.forEach(observation => {
      if (!observation.name || observationNames.has(observation.name)) errors.push("观测窗口名称不能为空或重复");
      observationNames.add(observation.name);
      if (observation.type !== "VirtualOpticalPowerVisualizer") errors.push("当前支持的观测仪器为光功率可视化仪");
      if (!available.has(observation.source)) errors.push("观测端口不存在：" + observation.source);
    });
    runtimeFields.forEach(f => numeric(config.runtime?.[f.key], f, "运行 · " + f.label));
    ["start_paused", "epc_controls"].forEach(key => {
      if (![true, false, 0, 1].includes(config.runtime?.[key])) errors.push("runtime." + key + " 需要布尔值");
    });
    return errors;
  }
  function nextName(config, type) {
    let number = 1;
    while (config.elements.some(element => element.name === type.toLowerCase() + number)) ++number;
    return type.toLowerCase() + number;
  }
  function rename(config, oldName, newName) {
    if (!/^[A-Za-z][A-Za-z0-9_]*$/.test(newName) || config.elements.some(element => element.name === newName && element.name !== oldName)) throw new Error("名称需要唯一，并以英文字母开头，仅包含字母、数字或下划线");
    const element = config.elements.find(item => item.name === oldName);
    const replacements = Object.fromEntries(ports(element).map(port => [oldName + "-" + port, newName + "-" + port]));
    element.name = newName;
    config.elements.forEach(item => Object.keys(item.connections).forEach(key => { item.connections[key] = replacements[item.connections[key]] || item.connections[key]; }));
    config.observations.forEach(item => { item.source = replacements[item.source] || item.source; });
    return config;
  }
  function move(config, from, to) {
    if (to < 0 || to >= config.elements.length || from === to) return;
    config.elements.splice(to, 0, config.elements.splice(from, 1)[0]);
  }
  const api = { types, gaussianFields, runtimeFields, clone, createElement, ports, outputs, connectSerial, defaultConfig, normalize, validate, nextName, rename, move };
  root.CavityModel = api;
  if (typeof module !== "undefined") module.exports = api;
})(typeof globalThis !== "undefined" ? globalThis : this);
