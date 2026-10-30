# 一加 5（cheeseburger）

[DinnerABC/UniXNU-cheeseburger](https://github.com/DinnerABC/UniXNU-cheeseburger)

<div v-pre>

Original iPadOS interface with time visible on hardware \(UP1, user-confirmed\); touch pending, guarded build migration in progress.

源记录: [porting.toml](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/porting.toml), `950b0df3bf326129c743f3415eececaceb2e4bec`.

本页按设备记录自动生成。

| 部分 | 状态 |
| --- | --- |
| [配置](#configuration) | `planned` |
| [固件与启动](#firmware) | `planned` |
| [CPU 与中断](#cpu) | `planned` |
| [内存与 DMA](#memory) | `planned` |
| [存储与根卷](#storage) | `planned` |
| [显示](#display) | `planned` |
| [输入](#input) | `planned` |
| [GPU](#gpu) | `planned` |
| [网络](#network) | `planned` |
| [音频](#audio) | `planned` |
| [电源](#power) | `planned` |
| [镜像组装](#packaging) | `planned` |
| [诊断](#diagnostics) | `planned` |

## 配置 {#configuration}

`planned`

组件: `ArmDTPlatform`, `ArmCpu`, `SoftwareAES`, `XnuPrelink`.

### 硬件与依赖

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### 配置

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### 操作步骤

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### 开发经验

- 官方 format2 四层配置解析成功，本地SoC override如实标记未发布；构建guard仍开启。

### 完成条件

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### 当前范围

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### 下一步

- 补齐板级archive与patch计划后再解除guard。

### 证据

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## 固件与启动 {#firmware}

`planned`

组件: `DarwinBoot`.

### 硬件与依赖

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### 配置

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### 操作步骤

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### 开发经验

- 现有edk2-msm8998曾完成handoff；其不使用Mu-Silicium模板，官方固件入口尚待适配。

### 完成条件

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### 当前范围

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### 下一步

- 核对真实DSC/FDF和payload handoff，建立官方firmware recipe。

### 证据

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## CPU 与中断 {#cpu}

`planned`

### 硬件与依赖

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### 配置

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### 操作步骤

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### 开发经验

- 八核拓扑与19.2MHz timer来自捕获FDT，运行基线cpus=1；不能沿用SM8550 GIC传输假设。

### 完成条件

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### 当前范围

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### 下一步

- 导出已验证P8适配并解决CPU compatible匹配。

### 证据

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## 内存与 DMA {#memory}

`planned`

### 硬件与依赖

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### 配置

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### 操作步骤

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### 开发经验

- 捕获库存含两段RAM和保留区，common converter要求连续交接范围。

### 完成条件

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### 当前范围

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### 下一步

- 明确库存与交接范围关系，验证所有loader分配及保留区。

### 证据

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## 存储与根卷 {#storage}

`planned`

组件: `VerifiedRoot`, `RamBlock`.

### 硬件与依赖

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### 配置

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### 操作步骤

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### 开发经验

- 历史root/SystemOS/owner验证与RAM Data存在，持久化未启用。

### 完成条件

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### 当前范围

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### 下一步

- 将warm-link UFS、只读边界和验证根recipe纳入新构建。

### 证据

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## 显示 {#display}

`planned`

组件: `GopFramebuffer`.

### 硬件与依赖

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### 配置

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### 操作步骤

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### 开发经验

- R4曾显示UI且有实际缩放拷贝；这不证明当前候选提交链已完成。

### 完成条件

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### 当前范围

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### 下一步

- 保持依赖加载/构造/注册顺序与swap retirement，完成新构建ABI门禁。

### 证据

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## 输入 {#input}

`planned`

### 硬件与依赖

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### 配置

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### 操作步骤

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### 开发经验

- 当前仅power/volume和合成Home chord有历史响应；不是完整触屏或原生Home。

### 完成条件

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### 当前范围

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### 下一步

- 将官方HID ABI及实际PMIC读适配纳入设备recipe。

### 证据

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## GPU {#gpu}

`planned`

### 硬件与依赖

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### 配置

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### 操作步骤

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### 开发经验

- 当前CPU GOP路径，不选择Adreno740/AdrenoMetal；本机为MSM8998。

### 完成条件

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### 当前范围

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### 下一步

- CPU桌面稳定后另行审计Adreno540适用实现。

### 证据

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## 网络 {#network}

`planned`

### 硬件与依赖

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### 配置

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### 操作步骤

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### 开发经验

- 当前没有WiFi/蓝牙完整硬件支持证据。

### 完成条件

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### 当前范围

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### 下一步

- 取得本板控制器/固件/传输事实后再选择组件。

### 证据

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## 音频 {#audio}

`planned`

### 硬件与依赖

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### 配置

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### 操作步骤

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### 开发经验

- 原始音频模块装入不等于实际codec/路由支持。

### 完成条件

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### 当前范围

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### 下一步

- 收集本机codec/时钟/路由事实，不复制sheng音频地址。

### 证据

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## 电源 {#power}

`planned`

组件: `AppleKeyStoreState`.

### 硬件与依赖

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### 配置

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### 操作步骤

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### 开发经验

- PMGR板卡识别与iPad产品身份已解耦并通过340项ABI编译检查；AKS仅临时状态服务。

### 完成条件

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### 当前范围

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### 下一步

- 核对新archive最终链接与UUID就绪；不宣称SEP密钥或深睡唤醒支持。

### 证据

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## 镜像组装 {#packaging}

`planned`

### 硬件与依赖

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### 配置

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### 操作步骤

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### 开发经验

- 历史compressed RAM payload有门禁；官方payload/FAT入口与板卡loader需明确适配。

### 完成条件

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### 当前范围

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### 下一步

- 保持已验证解码/边界和Android header，完成新candidate门禁。

### 证据

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## 诊断 {#diagnostics}

`planned`

### 硬件与依赖

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### 配置

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### 操作步骤

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### 开发经验

- R7观察器与新官方审计独立保留，USB及持久日志当前不启用。

### 完成条件

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### 当前范围

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### 下一步

- 新镜像离线检查全过后才进入身份固定的单次临时启动。

### 证据

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

</div>
