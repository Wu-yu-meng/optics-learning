"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { spawnSync } = require("node:child_process");
const M = require("../editor/model.js");
const root = path.resolve(__dirname, "..");
let count = 0;
function test(name, action) { action(); ++count; console.log("PASS: " + name); }

test("浏览器默认配置与程序默认JSON完全一致", () => {
  const shipped = JSON.parse(fs.readFileSync(path.join(root, "config/config.json"), "utf8"));
  assert.deepEqual(M.defaultConfig(), shipped);
  assert.deepEqual(M.validate(shipped), []);
});
test("移动器件后自动更新串联顺序与反馈", () => {
  const config = M.defaultConfig();
  M.move(config, 2, 0); M.connectSerial(config);
  assert.equal(config.elements[0].name, "edf1");
  assert.equal(config.elements[0].connections.in, "pd1-out");
  assert.equal(config.elements[1].connections.in, "edf1-out");
  assert.deepEqual(M.validate(config), []);
});
test("OC可选择提取端口继续传播，观测点保持独立", () => {
  const config = M.defaultConfig();
  M.connectSerial(config, { oc1: "extracted" });
  assert.equal(config.elements[4].connections.in, "oc1-extracted");
  assert.equal(config.observations[1].source, "oc1-extracted");
  assert.deepEqual(M.validate(config), []);
});
test("重命名器件同时更新所有连接和观测引用", () => {
  const config = M.defaultConfig();
  M.rename(config, "oc1", "output_coupler");
  assert.equal(config.elements[4].connections.in, "output_coupler-cavity");
  assert.equal(config.observations[1].source, "output_coupler-extracted");
  assert.throws(() => M.rename(config, "smf1", "pd1"));
  assert.deepEqual(M.validate(config), []);
});
test("旧版采样与SMF步长可以迁移", () => {
  const config = M.normalize({ scientific_constants: { light_speed: 299792458 }, init_gauss_optical_field: { sample_size: 1025, sample_stride_fs: 8 }, SMF: { step_length_m: .125 } });
  assert.equal(config.init_gauss_optical_field.sample_size, 1025);
  assert.equal(config.elements[1].parameters.step_length_m, .125);
  assert.deepEqual(M.validate(config), []);
});
test("无效端口、重复名称、零步长、负噪声和非法种子被识别", () => {
  for (const mutate of [
    c => { c.elements[1].connections.in = "missing-out"; },
    c => { c.elements[1].name = "epc1"; },
    c => { c.elements[1].parameters.step_length_m = 0; },
    c => { c.elements[2].parameters.noise_energy_pj_per_pass = -1; },
    c => { c.elements[2].parameters.random_seed = 1.5; },
    c => { c.elements[0].parameters.retarders[1].ID = 0; },
    c => { c.observations[0].source = "oc1-out"; },
    c => { c.init_gauss_optical_field.sample_size = 1; }
  ]) { const config = M.defaultConfig(); mutate(config); assert.ok(M.validate(config).length); }
});
test("消融参数和无观测配置可导出", () => {
  const config = M.defaultConfig();
  config.elements[1].parameters.gamma_per_w_m = 0;
  config.elements[1].parameters.beta2_ps2_per_km = 0;
  config.elements[2].parameters.noise_energy_pj_per_pass = 0;
  config.observations = [];
  assert.deepEqual(M.validate(config), []);
});

const binary = process.argv[2] || path.resolve(root, "../../../build/AutomaticModeLocked/AutomaticModeLocked");
if (fs.existsSync(binary)) {
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), "cavity-config-workflow-"));
  function save(name, config) { const target = path.join(temporary, name + ".json"); fs.writeFileSync(target, JSON.stringify(config, null, 4)); return target; }
  function run(args) {
    const result = spawnSync(binary, args, { encoding: "utf8", timeout: 30000 });
    if (result.error) throw result.error;
    return result;
  }
  test("编辑器导出的重排、新器件及独立参数被C++直接加载", () => {
    const config = M.defaultConfig();
    M.move(config, 2, 0);
    const secondSMF = M.createElement("SMF", "smf2");
    secondSMF.parameters.length_m = .4;
    secondSMF.parameters.step_length_m = .1;
    config.elements.push(secondSMF);
    config.init_gauss_optical_field.sample_size = 1025;
    config.init_gauss_optical_field.sample_stride_fs = 8;
    config.elements.find(e => e.type === "EDF").parameters.random_seed = 23;
    config.runtime.start_paused = true;
    M.connectSerial(config);
    config.observations[0].source = "smf2-out";
    assert.deepEqual(M.validate(config), []);
    const file = save("reordered", config);
    const validated = run(["--config", file, "--validate-only"]);
    assert.equal(validated.status, 0, validated.stderr);
    assert.match(validated.stdout, /Grid: 1025 samples, 8 fs\/sample/);
    assert.match(validated.stdout, /epc1.in <- edf1-out/);
    const csv1 = path.join(temporary, "first.csv"), csv2 = path.join(temporary, "repeat.csv");
    assert.equal(run(["--config", file, "--headless", "--rounds", "3", "--metrics", csv1]).status, 0);
    assert.equal(run(["--config", file, "--headless", "--rounds", "3", "--metrics", csv2]).status, 0);
    assert.equal(fs.readFileSync(csv1, "utf8"), fs.readFileSync(csv2, "utf8"));
    const rows = fs.readFileSync(csv1, "utf8").trim().split("\n");
    assert.equal(rows.length, 1 + 4 * 2);
    for (const row of rows.slice(1)) assert.ok(row.split(",").slice(2).every(value => Number.isFinite(Number(value))));
  });
  test("后端拒绝无效连接、零步长、非法种子与未知器件", () => {
    for (const mutate of [
      c => { c.elements[1].connections.in = "smf1.out"; },
      c => { c.elements[1].parameters.step_length_m = 0; },
      c => { c.elements[2].parameters.random_seed = -1; },
      c => { c.elements[0].type = "Unknown"; }
    ]) { const config = M.defaultConfig(); mutate(config); const result = run(["--config", save("invalid", config), "--validate-only"]); assert.notEqual(result.status, 0); assert.match(result.stderr, /配置或运行错误/); }
  });
  console.log("Workflow artifacts: " + temporary);
} else console.log("SKIP: 尚未构建C++程序，仅运行配置模型测试。");
console.log(count + " tests passed.");
