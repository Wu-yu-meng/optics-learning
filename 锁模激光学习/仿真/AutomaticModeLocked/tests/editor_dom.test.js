"use strict";
//可选DOM交互测试：通过NODE_PATH提供jsdom，不给编辑器增加运行依赖。
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const os = require("node:os");
const { spawnSync } = require("node:child_process");
const { JSDOM } = require("jsdom");
const root = path.resolve(__dirname, "..");
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), "cavity-editor-dom-"));

async function main() {
  const dom = new JSDOM(fs.readFileSync(path.join(root, "editor/index.html"), "utf8"), {
    url: "http://cavity.local/", runScripts: "outside-only", pretendToBeVisual: true
  });
  const window = dom.window, document = window.document;
  const errors = [];
  window.addEventListener("error", event => errors.push(event.error));
  //下载及原生弹窗使用测试替身，编辑器的事件处理与配置生成保持原实现。
  window.HTMLDialogElement.prototype.showModal = function () { this.setAttribute("open", ""); };
  window.HTMLDialogElement.prototype.close = function () { this.removeAttribute("open"); };
  let downloadedBlob;
  window.URL.createObjectURL = blob => { downloadedBlob = blob; return "blob:cavity-test"; };
  window.URL.revokeObjectURL = () => {};
  window.HTMLAnchorElement.prototype.click = function () { assert.equal(this.download, "config.json"); };
  window.eval(fs.readFileSync(path.join(root, "editor/model.js"), "utf8"));
  window.eval(fs.readFileSync(path.join(root, "editor/app.js"), "utf8"));
  const query = selector => { const element = document.querySelector(selector); assert.ok(element, selector); return element; };
  const click = selector => query(selector).click();
  const change = (selector, value) => { const element = query(selector); if (element.type === "checkbox") element.checked = value; else element.value = value; element.dispatchEvent(new window.Event("change", { bubbles: true })); };
  const configuration = () => {
    click("#json-button");
    const config = JSON.parse(query("#json-editor").value);
    click('[data-close="json-dialog"]');
    return config;
  };
  const names = () => [...document.querySelectorAll("#element-list [data-name]")].map(element => element.dataset.name);

  assert.equal(document.title, "Cavity Lab · 光学腔配置工作台");
  assert.equal(query("#export-button").disabled, false);
  assert.deepEqual(names(), ["epc1", "smf1", "edf1", "oc1", "pd1"]);
  assert.equal(document.querySelectorAll(".diagram-node").length, 5);
  change('[aria-label="CH1相位延迟"]', "325.5");
  assert.equal(configuration().elements[0].parameters.retarders[0].delta_phase_deg, 325.5);

  click('#element-list [data-name="smf1"]');
  change('[data-key="gamma_per_w_m"]', ".02");
  change('[data-key="step_length_m"]', ".125");
  change('[data-action="rename"]', "fiberA");
  assert.equal(configuration().elements[2].connections.in, "fiberA-out");
  click('[data-action="down"]');
  assert.deepEqual(names(), ["epc1", "edf1", "fiberA", "oc1", "pd1"]);
  assert.equal(configuration().elements[2].connections.in, "edf1-out");

  click('[data-action="duplicate"]');
  assert.equal(names()[3], "smf1");
  change('[data-key="gamma_per_w_m"]', ".04");
  assert.equal(configuration().elements.find(element => element.name === "fiberA").parameters.gamma_per_w_m, .02);
  click('[data-add-type="EPC"]');
  assert.equal(names().at(-1), "epc2");
  click('[data-action="remove"]');
  assert.equal(names().includes("epc2"), false);
  click("#undo-button"); assert.equal(names().includes("epc2"), true);
  click("#redo-button"); assert.equal(names().includes("epc2"), false);

  click('[data-tab="observations"]');
  click('[data-action="add-observation"]');
  change('[data-observation="2"][data-observation-key="source"]', "fiberA-out");
  change('[data-observation="2"][data-observation-key="name"]', "光纤末端");
  click('#element-list [data-name="fiberA"]');
  change('[data-action="rename"]', "fiberB");
  assert.equal(configuration().observations[2].source, "fiberB-out");

  change("#auto-connect", false);
  click('#element-list [data-name="oc1"]');
  assert.equal(query('[data-action="input-source"]').disabled, false);
  change('[data-action="input-source"]', "pd1-out");
  assert.equal(configuration().elements.find(element => element.name === "oc1").connections.in, "pd1-out");
  change("#auto-connect", true);
  assert.equal(configuration().elements.find(element => element.name === "oc1").connections.in, "smf1-out");

  click('#element-list [data-name="fiberB"]');
  change('[data-key="step_length_m"]', "0");
  assert.equal(query("#export-button").disabled, true);
  click("#undo-button");
  assert.equal(query("#export-button").disabled, false);
  click('[data-tab="sampling"]');
  change('[data-key="sample_size"]', "2049");
  change('[data-key="sample_stride_fs"]', "10");
  change('[data-runtime-check="start_paused"]', true);

  //模拟拖动列表，检查实际drop事件更新顺序与连接。
  const transfer = { setData() {}, effectAllowed: "" };
  const first = query('#element-list [data-name="epc1"]');
  const last = query('#element-list [data-name="pd1"]');
  const start = new window.Event("dragstart", { bubbles: true });
  Object.defineProperty(start, "dataTransfer", { value: transfer });
  first.dispatchEvent(start);
  last.dispatchEvent(new window.Event("drop", { bubbles: true, cancelable: true }));
  assert.equal(names().at(-1), "epc1");
  assert.equal(configuration().elements[0].connections.in, "epc1-out");
  assert.equal(window.CavityModel.validate(configuration()).length, 0);

  click("#export-button");
  assert.equal(downloadedBlob.type, "application/json");
  const exportedText = await new Promise((resolve, reject) => {
    const reader = new window.FileReader(); reader.onload = () => resolve(reader.result); reader.onerror = reject; reader.readAsText(downloadedBlob);
  });
  const exported = JSON.parse(exportedText);
  assert.equal(exported.elements.at(-1).name, "epc1");
  assert.equal(exported.observations[2].source, "fiberB-out");
  assert.equal(exported.init_gauss_optical_field.sample_size, 2049);
  assert.equal(exported.runtime.start_paused, true);
  const exportedPath = path.join(temporary, "config.json");
  fs.writeFileSync(exportedPath, exportedText);

  click("#json-button");
  click("#copy-json-button");
  assert.equal(query("#json-editor").selectionStart, 0);
  assert.equal(query("#json-editor").selectionEnd, query("#json-editor").value.length);
  query("#json-editor").value = "{invalid json";
  click("#apply-json-button");
  assert.ok(query("#json-error").textContent.length);
  assert.equal(query("#json-dialog").hasAttribute("open"), true);
  query("#json-editor").value = exportedText;
  click("#apply-json-button");
  assert.equal(query("#json-dialog").hasAttribute("open"), false);

  //同一个文件输入事件支持导入旧配置并迁移，且不会覆盖已导出的文件。
  const input = query("#import-file");
  Object.defineProperty(input, "files", { configurable: true, value: [{ name: "legacy.json", text: async () => JSON.stringify({ init_gauss_optical_field: { sample_size: 1025, sample_stride_fs: 8 }, SMF: { step_length_m: .5 } }) }] });
  input.dispatchEvent(new window.Event("change", { bubbles: true }));
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(configuration().init_gauss_optical_field.sample_size, 1025);
  assert.equal(configuration().elements[1].parameters.step_length_m, .5);
  assert.ok(window.localStorage.getItem("cavity-lab-v1"));
  assert.equal(errors.length, 0, errors.map(error => error?.stack).join("\n"));
  dom.window.close();

  const binary = process.argv[2] || path.resolve(root, "../../../build/AutomaticModeLocked/AutomaticModeLocked");
  if (fs.existsSync(binary)) {
    for (const args of [["--validate-only"], ["--headless", "--rounds", "2", "--metrics", path.join(temporary, "metrics.csv")]]) {
      const result = spawnSync(binary, ["--config", exportedPath, ...args], { encoding: "utf8", timeout: 30000 });
      assert.equal(result.status, 0, result.stderr);
    }
  }
  console.log("PASS: DOM selection, parameter editing, rename, reorder/drop, duplicate/add/remove, undo/redo, observations, manual routing, validation, JSON editing/import/export, local draft, exported config C++ execution.");
  console.log("DOM workflow artifacts: " + temporary);
}
main().catch(error => { console.error(error); process.exitCode = 1; });
