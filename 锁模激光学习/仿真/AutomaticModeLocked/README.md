# 配置驱动验证版

本版将器件创建、参数、执行顺序、输入连接和观测点统一放入 JSON。浏览器编辑器是本地静态页面，不需要安装前端依赖，也不会上传配置。

## 使用

1. 在浏览器中打开本目录的 `editor/index.html`。也可以在本目录运行 `python3 -m http.server 8765 --directory editor`，然后访问 `http://127.0.0.1:8765`。
2. 点击器件编辑参数；拖动左侧列表或使用上下箭头调整顺序。默认“自动串联”会重新连接相邻器件并闭合末端反馈；关闭后可手动指定每个输入的来源。
3. 在“采样与运行”设置初始光场、采样和播放参数，在“观测点”选择需要打开的光功率窗口。
4. 导出 `config.json`，然后通过 `--config` 指向下载文件。导出不会自动覆盖仓库中的配置。

从仓库根目录构建和运行：

```bash
cmake -S 锁模激光学习/仿真 -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target AutomaticModeLocked -j 2
./build/AutomaticModeLocked/AutomaticModeLocked
./build/AutomaticModeLocked/AutomaticModeLocked --config /绝对路径/config.json
```

不带参数时使用模块自带的配置，与当前工作目录无关。默认配置的数值参数与改造前一致：EPC → SMF → EDF → OC → PD，SMF γ=0.013、L=5 m、β₂=-21.7 ps²/km、步长0.25 m；EDF 增益带宽4 THz、噪声带宽10 THz、噪声能量1e-6 pJ/圈、种子20261018。它是原工作参数的配置化版本，不承诺启动后自动进入锁模。

GUI 按键：Space 运行/暂停，D 单步，A 重新调整所有观测窗口量程，R 重置光场与 EDF 噪声种子，Q/Esc 退出。A 在暂停时也生效，不推进光场。R 保留当前 EPC 设置。EPC 使用独立控制窗口，内部窗口ID使用ASCII以兼容OpenCV Qt；配置中的小数相位在滑条未移动时保持原值。

## 配置约定

- `version` 固定为1。
- `elements` 数组顺序就是执行顺序，每项含 `name`、`type`、`parameters`、`connections`。
- 器件名称使用英文字母开头的字母、数字、下划线；当前支持 SMF、EDF、EPC、OC、PD。
- 全局输出名称为 `器件名-输出端口`，普通器件输出为 `out`，OC 输出为 `cavity`、`extracted`。连接示例：`"connections": {"in": "oc1-cavity"}`。
- `observations` 含 `name`、`type: "VirtualOpticalPowerVisualizer"`、`source`，可以在任何有效输出设置多个观测窗口。示波器、光谱仪的原有桩接口没有在本次实现。
- `runtime` 控制暂停启动、播放等待时间、是否显示EPC滑条、绘图间隔和停止圈数。`max_rounds: 0` 表示持续运行。
- 每个SMF有独立的 `step_length_m`；初始光场和器件均使用本次 `--config` 指定的文件，不再依赖SMF内部的全局配置读取。
- 可关闭噪声（能量设0）、Kerr（γ设0）、色散（β₂设0）、增益（g₀设0）做消融；增益带宽与噪声带宽独立设置。

浏览器支持导入、JSON直接编辑、复制/新增/移除器件、重命名时更新连接、观测点编辑、撤销/重做和本地草稿。旧版仅含初始光场及SMF步长的配置可导入编辑器迁移为版本1。

## 执行语义

沿用当前 `OpticalCavity::run_once()` 的顺序模型：各连接的初始缓存使用相同高斯光场；按数组顺序更新输出，已执行器件提供本次结果，尚未执行的反馈来源提供缓存结果。编辑器的排列顺序会影响这种模型的更新顺序。

本版没有加入不同环长的物理时延同步，也没有实现双输入耦合器。端口映射框架可扩展多输入，但当前五类器件各有一个 `in`。因此，自定义反馈连接是顺序映射验证，不能直接当作精确的多环时域仿真。初始画面中的未传播端口也只是初始化缓存。

## 验证和无窗口实验

```bash
./build/AutomaticModeLocked/AutomaticModeLocked --config /路径/config.json --validate-only
./build/AutomaticModeLocked/AutomaticModeLocked --config /路径/config.json --headless --rounds 100 --metrics /tmp/cavity.csv
cmake --build build --target AutomaticModeLocked_config_test -j 2
ctest --test-dir build/AutomaticModeLocked --output-on-failure
node 锁模激光学习/仿真/AutomaticModeLocked/tests/editor_model.test.js
```

无窗口模式忽略播放等待和暂停设置；没有指定停止圈数时默认运行100圈。指标CSV保存初始状态及每圈各观测端口的峰值功率W、总能量pJ。相同初始光场、种子、器件参数及操作历史才能复现实验。配置在启动时读取，运行中修改文件不自动重建光路。

回归测试逐圈比较配置光路与原手写光路在六个输出端口的完整复光场，并检查运行中EPC修改及重置噪声状态。

可选DOM交互测试（只给测试环境安装jsdom，页面本身不需要任何依赖）：

```bash
npm install --prefix /tmp/cavity-editor-qa jsdom
NODE_PATH=/tmp/cavity-editor-qa/node_modules node 锁模激光学习/仿真/AutomaticModeLocked/tests/editor_dom.test.js
```

## 原开发计划

需要移植的内容：
1. Kerr效应和二阶色散分段进行（完成）
2. EDF的增益带宽（完成）
3. EDF中的随机噪声（完成）
4. 加大窗口（测试发现到16192已经是勉强能接受的极限了，后续看加入其他的部分是否会导致窗不够大）
5. 拨片(测试发现不需要)
6. 可视化系统（完成了虚拟的光功率器）
7. 优化速度（暂时够用）

下一步：
1. 脉冲识别系统
2. 反馈系统（可以尝试随机、遗传、强化学习、神经网络）
3. 加入一个环境扰动的双折射矩阵，和额外噪声矩阵
4. 观察反馈系统的鲁棒性

下下步：
1. 编写实际环境中的数据读取算法
2. 移植反馈算法
3. 测试
