/* 无网络依赖：直接打开 index.html 即可编辑、导入和导出配置。 */
(function () {
  "use strict";
  const M = window.CavityModel;
  const $ = selector => document.querySelector(selector);
  const escape = value => String(value ?? "").replace(/[&<>"']/g, char => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[char]));
  const state = { config: M.defaultConfig(), selected: "epc1", tab: "device", autoConnect: true, forwardPorts: {} };
  const history = [], future = [];
  let dragName = "", toastTimer;
  function toast(message) {
    $("#toast").textContent = message;
    $("#toast").classList.add("visible");
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => $("#toast").classList.remove("visible"), 3200);
  }
  function snapshot() { return M.clone(state); }
  function restore(value) { Object.assign(state, M.clone(value)); }
  function commit(action, message, rewire = false) {
    const before = snapshot();
    try {
      action();
      if (rewire && state.autoConnect) M.connectSerial(state.config, state.forwardPorts);
      history.push(before);
      if (history.length > 60) history.shift();
      future.length = 0;
      render();
      if (message) toast(message);
    } catch (error) { restore(before); toast(error.message); render(); }
  }
  function inferRoutes(config) {
    const routes = {};
    const available = M.outputs(config);
    config.elements.forEach(element => {
      const source = available.find(output => output.key === element.connections.in);
      if (source) routes[source.name] = source.port;
    });
    return routes;
  }
  function loadConfig(input, message) {
    const config = M.normalize(input);
    const errors = M.validate(config);
    if (errors.length) throw new Error(errors[0]);
    const forwardPorts = inferRoutes(config);
    const serial = M.clone(config);
    M.connectSerial(serial, forwardPorts);
    commit(() => {
      state.config = config;
      state.selected = config.elements[0].name;
      state.forwardPorts = forwardPorts;
      state.autoConnect = config.elements.every((element, index) => element.connections.in === serial.elements[index].connections.in);
    }, message);
  }
  function selected() { return state.config.elements.find(element => element.name === state.selected) || state.config.elements[0]; }
  function options(current, includeMissing = true) {
    const available = M.outputs(state.config);
    const missing = includeMissing && !available.some(output => output.key === current) ? `<option value="${escape(current)}">${escape(current)} · 无效端口</option>` : "";
    return missing + available.map(output => `<option value="${escape(output.key)}" ${output.key === current ? "selected" : ""}>${escape(output.name)} / ${escape(output.port)}</option>`).join("");
  }
  function numericField(definition, value, scope) {
    return `<label class="form-field"><span>${escape(definition.label)}<span class="unit">${escape(definition.unit)}</span></span><input type="number" step="${definition.integer ? "1" : "any"}" ${definition.min !== undefined ? `min="${definition.min}"` : ""} ${definition.max !== undefined ? `max="${definition.max}"` : ""} value="${escape(value)}" data-scope="${scope}" data-key="${definition.key}" aria-label="${escape(definition.label)}"></label>`;
  }
  function renderList() {
    $("#element-list").innerHTML = state.config.elements.map((element, index) => `<li class="element-row ${element.name === state.selected ? "selected" : ""}" draggable="true" tabindex="0" role="button" aria-label="选择 ${escape(element.name)}" data-name="${escape(element.name)}" style="--device-color:${M.types[element.type].color}"><span class="drag-handle" aria-hidden="true">⠿</span><span class="element-symbol" aria-hidden="true">${M.types[element.type].symbol}</span><span class="element-text"><strong>${escape(element.name)}</strong><small>${M.types[element.type].label}</small></span><span class="order-index">${String(index + 1).padStart(2, "0")}</span></li>`).join("");
    $("#element-count").textContent = state.config.elements.length;
  }
  function renderDiagram() {
    const elements = state.config.elements;
    const width = Math.max(750, elements.length * 146 + 46, $("#diagram-scroll").clientWidth);
    const height = window.innerWidth >= 1500 ? 270 : 244;
    const middle = height / 2;
    const x = index => elements.length === 1 ? (width - 108) / 2 : 32 + index * (width - 172) / (elements.length - 1);
    const available = M.outputs(state.config);
    let paths = "";
    elements.forEach((element, index) => {
      const source = available.find(output => output.key === element.connections.in);
      if (!source) return;
      const from = elements.findIndex(item => item.name === source.name);
      const start = x(from) + 108, end = x(index);
      const feedback = from >= index;
      let path;
      if (from + 1 === index) path = `M${start},${middle} H${end - 4}`;
      else {
        const lane = feedback ? height - 24 - (index % 3) * 8 : 27 + (index % 3) * 8;
        path = `M${start},${middle} H${start + 14} V${lane} H${end - 15} V${middle} H${end - 4}`;
      }
      paths += `<path d="${path}" fill="none" stroke="${feedback ? "#526a91" : "#7693b8"}" stroke-width="1.4" ${feedback ? 'stroke-dasharray="4 4"' : ""} marker-end="url(#arrow)"/>`;
    });
    const observed = new Set(state.config.observations.map(observation => observation.source));
    $("#diagram").style.width = width + "px";
    $("#diagram").innerHTML = `<svg viewBox="0 0 ${width} ${height}" aria-hidden="true"><defs><marker id="arrow" markerWidth="5" markerHeight="5" refX="4" refY="2.5" orient="auto"><path d="M0 0L5 2.5L0 5" fill="#7693b8"/></marker></defs>${paths}</svg>` + elements.map((element, index) => `<button class="diagram-node ${element.name === state.selected ? "selected" : ""}" style="left:${x(index)}px;--device-color:${M.types[element.type].color}" data-name="${escape(element.name)}" aria-label="编辑 ${escape(element.name)}"><span class="node-index">${String(index + 1).padStart(2, "0")} / ${element.type}</span><span class="port-dot input"></span><span class="port-dot output"></span>${M.ports(element).some(port => observed.has(element.name + "-" + port)) ? '<span class="observer-dot" title="已设置观测点"></span>' : ""}<div class="node-symbol">${M.types[element.type].symbol}</div><strong>${escape(element.name)}</strong><small>${M.types[element.type].label}</small></button>`).join("");
  }
  function renderSummary() {
    const gaussian = state.config.init_gauss_optical_field;
    const format = number => Number.isFinite(number) ? Number(number.toPrecision(5)).toString() : "—";
    const cards = [
      ["时间窗口", format(gaussian.sample_size * gaussian.sample_stride_fs / 1000), "ps", "N × Δt · 局部快时间窗口"],
      ["频率间隔", format(1000 / gaussian.sample_size / gaussian.sample_stride_fs), "THz", "1 / 时间窗口"],
      ["中心波长", format(gaussian.wave_length_nm), "nm", "统一载波参考"],
      ["观测窗口", state.config.observations.length, "个", "光功率 · X / Y / Total"]
    ];
    $("#summary").innerHTML = cards.map(card => `<div class="summary-item"><label>${card[0]}</label><strong>${card[1]}<small>${card[2]}</small></strong><div class="summary-caption">${card[3]}</div></div>`).join("");
    $("#observation-count").textContent = state.config.observations.length;
  }
  function renderDevice() {
    const element = selected();
    if (!element) return '<div class="empty-state">从左侧器件库添加一个器件。</div>';
    const type = M.types[element.type], index = state.config.elements.indexOf(element);
    const header = `<div class="detail-header"><span class="element-symbol" style="--device-color:${type.color}">${type.symbol}</span><div><h2>${escape(element.name)} <span class="hint">/ ${element.type}</span></h2><p>${type.description}</p></div><div class="detail-actions"><button class="icon-button" data-action="up" aria-label="向前移动器件" ${index === 0 ? "disabled" : ""}>↑</button><button class="icon-button" data-action="down" aria-label="向后移动器件" ${index === state.config.elements.length - 1 ? "disabled" : ""}>↓</button><button class="button small ghost" data-action="duplicate">复制</button><button class="button small danger" data-action="remove" ${state.config.elements.length === 1 ? "disabled" : ""}>移除</button></div></div>`;
    let fields = `<div class="form-grid"><label class="form-field"><span>器件名称<span class="unit">unique ID</span></span><input data-action="rename" value="${escape(element.name)}" aria-label="器件名称" spellcheck="false"></label>${type.fields.map(f => numericField(f, element.parameters[f.key], "parameter")).join("")}</div>`;
    if (element.type === "EPC") fields += `<table class="retarder-table"><thead><tr><th>通道</th><th>主轴方向 / deg</th><th>相位延迟 / deg</th><th></th></tr></thead><tbody>${element.parameters.retarders.map((retarder, i) => `<tr><td>CH ${retarder.ID + 1}</td><td><input type="number" step="any" value="${retarder.orientation_deg}" data-retarder="${i}" data-retarder-key="orientation_deg" aria-label="CH${retarder.ID + 1}主轴方向"></td><td><div class="phase-cell"><input type="range" min="0" max="360" step="1" value="${retarder.delta_phase_deg}" data-retarder="${i}" data-retarder-key="delta_phase_deg" aria-label="CH${retarder.ID + 1}相位滑条"><input type="number" min="0" max="360" step="any" value="${retarder.delta_phase_deg}" data-retarder="${i}" data-retarder-key="delta_phase_deg" aria-label="CH${retarder.ID + 1}相位延迟"></div></td><td><button class="quiet-button" data-remove-retarder="${i}" aria-label="移除CH${retarder.ID + 1}" ${element.parameters.retarders.length === 1 ? "disabled" : ""}>×</button></td></tr>`).join("")}</tbody></table><button class="text-button" data-action="add-retarder">＋ 添加延迟器</button>`;
    const continuing = element.type === "OC" ? `<label class="form-field"><span>串联接续端口</span><select data-action="forward-port" aria-label="串联接续端口"><option value="cavity" ${(state.forwardPorts[element.name] || "cavity") === "cavity" ? "selected" : ""}>cavity · 腔内</option><option value="extracted" ${state.forwardPorts[element.name] === "extracted" ? "selected" : ""}>extracted · 提取输出</option></select></label>` : "";
    return header + fields + `<div class="connection-section"><label class="form-field"><span>输入 in ← 来源光场<span class="unit">${state.autoConnect ? "自动串联" : "自定义连接"}</span></span><select data-action="input-source" aria-label="输入来源" ${state.autoConnect ? "disabled" : ""}>${options(element.connections.in)}</select></label>${continuing}</div><p class="help-text">${state.autoConnect ? "移动或添加器件时，自动更新串联与末端反馈连接。关闭右上方自动串联可手动选择来源。" : "执行顺序与连接关系独立保存。程序按左侧顺序推进，未更新的反馈端口沿用缓存光场。"}${element.type === "EDF" ? " 噪声能量设为 0 可关闭噪声。" : ""}</p>`;
  }
  function renderSampling() {
    const config = state.config;
    return `<div class="detail-header"><div><h2>采样与初始光场</h2><p>所有器件共享同一采样网格，初始状态使用高斯光场。</p></div></div><div class="form-grid">${M.gaussianFields.map(f => numericField(f, config.init_gauss_optical_field[f.key], "gaussian")).join("")}</div><h3 class="subsection-title">运行设置</h3><div class="form-grid">${M.runtimeFields.map(f => numericField(f, config.runtime[f.key], "runtime")).join("")}</div><div class="checkbox-row"><label><input type="checkbox" data-runtime-check="start_paused" ${config.runtime.start_paused ? "checked" : ""}>暂停启动</label><label><input type="checkbox" data-runtime-check="epc_controls" ${config.runtime.epc_controls ? "checked" : ""}>显示 EPC 调节滑条</label></div><p class="help-text">播放等待时间不改变传播步长。绘制间隔只改变显示频率，不改变光场计算和随机数序列。</p>`;
  }
  function renderObservations() {
    return `<div class="detail-header"><div><h2>在任意输出端口观察光场</h2><p>每个观测点打开独立的光功率窗口，观测不会改变光场能量。</p></div></div>` + state.config.observations.map((observation, i) => `<div class="observation-row"><label class="form-field"><span>窗口名称</span><input value="${escape(observation.name)}" data-observation="${i}" data-observation-key="name" aria-label="观测点${i + 1}名称"></label><label class="form-field"><span>观测输出</span><select data-observation="${i}" data-observation-key="source" aria-label="观测点${i + 1}输出">${options(observation.source)}</select></label><button class="icon-button" data-remove-observation="${i}" aria-label="删除观测点${i + 1}">×</button></div>`).join("") + `<button class="button ghost" data-action="add-observation">＋ 添加观测点</button><p class="help-text">当前接入光功率可视化仪。示波器和光谱仪仍是原项目中的待实现接口。窗口纵坐标固定，按 A 才重新调整量程。</p>`;
  }
  function renderValidation() {
    const errors = M.validate(state.config);
    $("#validation-status").innerHTML = errors.length ? `<span class="valid-symbol invalid-symbol">!</span><span>${errors.length} 项待修正 · ${escape(errors[0])}</span>` : '<span class="valid-symbol">✓</span><span>配置完整，可直接导出运行</span>';
    $("#export-button").disabled = !!errors.length;
    $("#validation-details").innerHTML = errors.length ? `<ul>${errors.map(error => `<li>${escape(error)}</li>`).join("")}</ul>` : '<p class="help-text">✓ 器件参数、名称、输入连接与观测端口校验通过。</p>';
  }
  function render() {
    $("#project-name").value = state.config.name || "未命名实验";
    $("#project-label").textContent = state.config.name || "未命名实验";
    $("#auto-connect").checked = state.autoConnect;
    $("#undo-button").disabled = !history.length;
    $("#redo-button").disabled = !future.length;
    document.querySelectorAll("[data-tab]").forEach(button => button.setAttribute("aria-selected", String(button.dataset.tab === state.tab)));
    renderList(); renderDiagram(); renderSummary();
    $("#detail-panel").innerHTML = state.tab === "sampling" ? renderSampling() : state.tab === "observations" ? renderObservations() : renderDevice();
    renderValidation();
    try { if (!M.validate(state.config).length) localStorage.setItem("cavity-lab-v1", JSON.stringify({ config: state.config, autoConnect: state.autoConnect, forwardPorts: state.forwardPorts })); } catch (_) { /*浏览器禁用存储时仍可编辑和导出。*/ }
  }
  function select(name) { state.selected = name; state.tab = "device"; render(); }
  function moveSelected(delta) {
    const index = state.config.elements.findIndex(element => element.name === state.selected);
    commit(() => M.move(state.config, index, index + delta), "器件顺序已更新", true);
  }
  function addElement(type) {
    commit(() => {
      const element = M.createElement(type, M.nextName(state.config, type));
      element.connections.in = M.outputs(state.config).at(-1).key;
      state.config.elements.push(element);
      state.selected = element.name; state.tab = "device";
    }, "已添加 " + type, true);
  }
  function showDialog(id) { $(id).showModal(); }
  function exportConfig() {
    if (M.validate(state.config).length) { showDialog("#validation-dialog"); return; }
    const blob = new Blob([JSON.stringify(state.config, null, 4) + "\n"], { type: "application/json" });
    const url = URL.createObjectURL(blob), anchor = document.createElement("a");
    anchor.href = url; anchor.download = "config.json"; document.body.append(anchor); anchor.click(); anchor.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
    toast("已导出 config.json · 使用程序 --config 参数加载");
  }
  $("#library").innerHTML = Object.entries(M.types).map(([type, info]) => `<button data-add-type="${type}" aria-label="添加${info.label}"><span style="color:${info.color}">${info.symbol}</span>${type}<span class="plus">＋</span></button>`).join("");
  $("#library").addEventListener("click", event => { const button = event.target.closest("[data-add-type]"); if (button) addElement(button.dataset.addType); });
  $("#element-list").addEventListener("click", event => { const row = event.target.closest("[data-name]"); if (row) select(row.dataset.name); });
  $("#element-list").addEventListener("keydown", event => { const row = event.target.closest("[data-name]"); if (row && ["Enter", " "].includes(event.key)) { event.preventDefault(); select(row.dataset.name); } });
  $("#diagram").addEventListener("click", event => { const node = event.target.closest("[data-name]"); if (node) select(node.dataset.name); });
  $("#element-list").addEventListener("dragstart", event => { const row = event.target.closest("[data-name]"); dragName = row.dataset.name; event.dataTransfer.setData("text/plain", dragName); event.dataTransfer.effectAllowed = "move"; row.classList.add("dragging"); });
  $("#element-list").addEventListener("dragover", event => { event.preventDefault(); const row = event.target.closest("[data-name]"); if (row) row.classList.add("drag-over"); });
  $("#element-list").addEventListener("dragleave", event => event.target.closest("[data-name]")?.classList.remove("drag-over"));
  $("#element-list").addEventListener("drop", event => { event.preventDefault(); const row = event.target.closest("[data-name]"); if (!row) return; const from = state.config.elements.findIndex(element => element.name === dragName), to = state.config.elements.findIndex(element => element.name === row.dataset.name); if (from >= 0) commit(() => M.move(state.config, from, to), "器件顺序已更新", true); });
  $("#element-list").addEventListener("dragend", () => { dragName = ""; renderList(); });
  document.querySelectorAll("[data-tab]").forEach(button => button.addEventListener("click", () => { state.tab = button.dataset.tab; render(); }));
  $("#detail-panel").addEventListener("change", event => {
    const input = event.target, element = selected();
    if (input.dataset.scope) commit(() => { const target = input.dataset.scope === "parameter" ? element.parameters : input.dataset.scope === "gaussian" ? state.config.init_gauss_optical_field : state.config.runtime; target[input.dataset.key] = input.value === "" ? null : Number(input.value); });
    else if (input.dataset.retarder !== undefined) commit(() => { element.parameters.retarders[Number(input.dataset.retarder)][input.dataset.retarderKey] = input.value === "" ? null : Number(input.value); });
    else if (input.dataset.runtimeCheck) commit(() => { state.config.runtime[input.dataset.runtimeCheck] = input.checked; });
    else if (input.dataset.observation !== undefined) commit(() => { state.config.observations[Number(input.dataset.observation)][input.dataset.observationKey] = input.value; });
    else if (input.dataset.action === "rename") commit(() => { const old = element.name, next = input.value.trim(); M.rename(state.config, old, next); state.selected = next; if (old !== next) { state.forwardPorts[next] = state.forwardPorts[old]; delete state.forwardPorts[old]; } }, "器件及相关连接名称已更新");
    else if (input.dataset.action === "input-source") commit(() => { element.connections.in = input.value; });
    else if (input.dataset.action === "forward-port") commit(() => { state.forwardPorts[element.name] = input.value; }, "接续端口已更新", true);
  });
  $("#detail-panel").addEventListener("input", event => {
    if (event.target.type === "range") event.target.closest("td").querySelector('input[type="number"]').value = event.target.value;
  });
  $("#detail-panel").addEventListener("click", event => {
    const button = event.target.closest("button"); if (!button) return;
    const action = button.dataset.action, element = selected();
    if (action === "up") moveSelected(-1);
    else if (action === "down") moveSelected(1);
    else if (action === "duplicate") commit(() => { const copy = M.clone(element); copy.name = M.nextName(state.config, element.type); state.config.elements.splice(state.config.elements.indexOf(element) + 1, 0, copy); state.selected = copy.name; }, "器件已复制", true);
    else if (action === "remove") commit(() => {
      state.config.elements.splice(state.config.elements.indexOf(element), 1);
      const fallback = M.outputs(state.config).at(-1).key;
      const removed = new Set(M.ports(element).map(port => element.name + "-" + port));
      state.config.observations.forEach(observation => { if (removed.has(observation.source)) observation.source = fallback; });
      state.selected = state.config.elements[0].name;
    }, "已移除器件，相关观测点已转到末端输出", true);
    else if (action === "add-retarder") commit(() => { const retarders = element.parameters.retarders; retarders.push({ ID: retarders.length, orientation_deg: retarders.length % 2 ? 45 : 0, delta_phase_deg: 0 }); });
    else if (button.dataset.removeRetarder !== undefined) commit(() => { element.parameters.retarders.splice(Number(button.dataset.removeRetarder), 1); element.parameters.retarders.forEach((retarder, ID) => { retarder.ID = ID; }); });
    else if (action === "add-observation") commit(() => { let number = 1; while (state.config.observations.some(observation => observation.name === "观测窗口 " + number)) ++number; state.config.observations.push({ name: "观测窗口 " + number, type: "VirtualOpticalPowerVisualizer", source: M.outputs(state.config).at(-1).key }); });
    else if (button.dataset.removeObservation !== undefined) commit(() => { state.config.observations.splice(Number(button.dataset.removeObservation), 1); });
  });
  $("#auto-connect").addEventListener("change", event => commit(() => { state.autoConnect = event.target.checked; }, event.target.checked ? "已按当前顺序重新串联" : "已启用自定义输入连接", true));
  $("#project-name").addEventListener("change", event => commit(() => { state.config.name = event.target.value; }));
  $("#undo-button").addEventListener("click", () => { if (history.length) { future.push(snapshot()); restore(history.pop()); render(); } });
  $("#redo-button").addEventListener("click", () => { if (future.length) { history.push(snapshot()); restore(future.pop()); render(); } });
  $("#reset-button").addEventListener("click", () => commit(() => { state.config = M.defaultConfig(); state.selected = "epc1"; state.forwardPorts = {}; state.autoConnect = true; }, "已恢复默认示例，可撤销"));
  $("#export-button").addEventListener("click", exportConfig);
  $("#import-button").addEventListener("click", () => $("#import-file").click());
  $("#import-file").addEventListener("change", async event => { const file = event.target.files[0]; if (!file) return; try { loadConfig(JSON.parse(await file.text()), "已导入 " + file.name); } catch (error) { toast("导入失败：" + error.message); } event.target.value = ""; });
  $("#json-button").addEventListener("click", () => { $("#json-editor").value = JSON.stringify(state.config, null, 4); $("#json-error").textContent = ""; showDialog("#json-dialog"); });
  $("#apply-json-button").addEventListener("click", () => { try { loadConfig(JSON.parse($("#json-editor").value), "已应用 JSON 配置"); $("#json-dialog").close(); } catch (error) { $("#json-error").textContent = error.message; } });
  $("#copy-json-button").addEventListener("click", async () => { try { await navigator.clipboard.writeText($("#json-editor").value); toast("JSON 已复制"); } catch (_) { $("#json-editor").focus(); $("#json-editor").select(); toast("已选中 JSON，可按 Ctrl+C 复制"); } });
  $("#validation-detail-button").addEventListener("click", () => { renderValidation(); showDialog("#validation-dialog"); });
  document.querySelectorAll("[data-close]").forEach(button => button.addEventListener("click", () => $("#" + button.dataset.close).close()));
  document.addEventListener("keydown", event => { if (!(event.ctrlKey || event.metaKey)) return; if (event.key.toLowerCase() === "s") { event.preventDefault(); exportConfig(); } else if (event.key.toLowerCase() === "z" && !["INPUT", "TEXTAREA"].includes(event.target.tagName)) { event.preventDefault(); $(event.shiftKey ? "#redo-button" : "#undo-button").click(); } });
  window.addEventListener("resize", renderDiagram);
  try { const saved = JSON.parse(localStorage.getItem("cavity-lab-v1")); if (saved) { const config = M.normalize(saved.config); if (!M.validate(config).length) { state.config = config; state.autoConnect = saved.autoConnect ?? true; state.forwardPorts = saved.forwardPorts || {}; state.selected = config.elements[0].name; } } } catch (_) { /*忽略旧版本或损坏的本地草稿。*/ }
  render();
})();
