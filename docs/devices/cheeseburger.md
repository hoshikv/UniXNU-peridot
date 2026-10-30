# OnePlus 5 \(cheeseburger\)

[DinnerABC/UniXNU-cheeseburger](https://github.com/DinnerABC/UniXNU-cheeseburger)

<div v-pre>

Original iPadOS interface with time visible on hardware \(UP1, user-confirmed\); touch pending, guarded build migration in progress.

Source record: [porting.toml](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/porting.toml), `950b0df3bf326129c743f3415eececaceb2e4bec`.

This page is generated from the device record.

| Area | Status |
| --- | --- |
| [Configuration](#configuration) | `planned` |
| [Firmware and boot](#firmware) | `planned` |
| [CPU and interrupts](#cpu) | `planned` |
| [Memory and DMA](#memory) | `planned` |
| [Storage and root](#storage) | `planned` |
| [Display](#display) | `planned` |
| [Input](#input) | `planned` |
| [GPU](#gpu) | `planned` |
| [Network](#network) | `planned` |
| [Audio](#audio) | `planned` |
| [Power](#power) | `planned` |
| [Image assembly](#packaging) | `planned` |
| [Diagnostics](#diagnostics) | `planned` |

## Configuration {#configuration}

`planned`

Components: `ArmDTPlatform`, `ArmCpu`, `SoftwareAES`, `XnuPrelink`.

### Hardware and dependencies

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### Configuration

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### Procedure

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### Development findings

- 官方 format2 四层配置解析成功，本地SoC override如实标记未发布；构建guard仍开启。

### Completion criteria

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### Current limits

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### Next action

- 补齐板级archive与patch计划后再解除guard。

### Evidence

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## Firmware and boot {#firmware}

`planned`

Components: `DarwinBoot`.

### Hardware and dependencies

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### Configuration

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### Procedure

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### Development findings

- 现有edk2-msm8998曾完成handoff；其不使用Mu-Silicium模板，官方固件入口尚待适配。

### Completion criteria

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### Current limits

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### Next action

- 核对真实DSC/FDF和payload handoff，建立官方firmware recipe。

### Evidence

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## CPU and interrupts {#cpu}

`planned`

### Hardware and dependencies

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### Configuration

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### Procedure

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### Development findings

- 八核拓扑与19.2MHz timer来自捕获FDT，运行基线cpus=1；不能沿用SM8550 GIC传输假设。

### Completion criteria

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### Current limits

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### Next action

- 导出已验证P8适配并解决CPU compatible匹配。

### Evidence

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## Memory and DMA {#memory}

`planned`

### Hardware and dependencies

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### Configuration

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### Procedure

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### Development findings

- 捕获库存含两段RAM和保留区，common converter要求连续交接范围。

### Completion criteria

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### Current limits

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### Next action

- 明确库存与交接范围关系，验证所有loader分配及保留区。

### Evidence

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## Storage and root {#storage}

`planned`

Components: `VerifiedRoot`, `RamBlock`.

### Hardware and dependencies

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### Configuration

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### Procedure

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### Development findings

- 历史root/SystemOS/owner验证与RAM Data存在，持久化未启用。

### Completion criteria

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### Current limits

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### Next action

- 将warm-link UFS、只读边界和验证根recipe纳入新构建。

### Evidence

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## Display {#display}

`planned`

Components: `GopFramebuffer`.

### Hardware and dependencies

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### Configuration

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### Procedure

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### Development findings

- R4曾显示UI且有实际缩放拷贝；这不证明当前候选提交链已完成。

### Completion criteria

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### Current limits

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### Next action

- 保持依赖加载/构造/注册顺序与swap retirement，完成新构建ABI门禁。

### Evidence

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## Input {#input}

`planned`

### Hardware and dependencies

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### Configuration

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### Procedure

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### Development findings

- 当前仅power/volume和合成Home chord有历史响应；不是完整触屏或原生Home。

### Completion criteria

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### Current limits

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### Next action

- 将官方HID ABI及实际PMIC读适配纳入设备recipe。

### Evidence

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## GPU {#gpu}

`planned`

### Hardware and dependencies

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### Configuration

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### Procedure

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### Development findings

- 当前CPU GOP路径，不选择Adreno740/AdrenoMetal；本机为MSM8998。

### Completion criteria

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### Current limits

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### Next action

- CPU桌面稳定后另行审计Adreno540适用实现。

### Evidence

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## Network {#network}

`planned`

### Hardware and dependencies

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### Configuration

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### Procedure

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### Development findings

- 当前没有WiFi/蓝牙完整硬件支持证据。

### Completion criteria

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### Current limits

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### Next action

- 取得本板控制器/固件/传输事实后再选择组件。

### Evidence

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## Audio {#audio}

`planned`

### Hardware and dependencies

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### Configuration

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### Procedure

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### Development findings

- 原始音频模块装入不等于实际codec/路由支持。

### Completion criteria

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### Current limits

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### Next action

- 收集本机codec/时钟/路由事实，不复制sheng音频地址。

### Evidence

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## Power {#power}

`planned`

Components: `AppleKeyStoreState`.

### Hardware and dependencies

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### Configuration

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### Procedure

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### Development findings

- PMGR板卡识别与iPad产品身份已解耦并通过340项ABI编译检查；AKS仅临时状态服务。

### Completion criteria

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### Current limits

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### Next action

- 核对新archive最终链接与UUID就绪；不宣称SEP密钥或深睡唤醒支持。

### Evidence

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## Image assembly {#packaging}

`planned`

### Hardware and dependencies

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### Configuration

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### Procedure

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### Development findings

- 历史compressed RAM payload有门禁；官方payload/FAT入口与板卡loader需明确适配。

### Completion criteria

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### Current limits

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### Next action

- 保持已验证解码/边界和Android header，完成新candidate门禁。

### Evidence

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

## Diagnostics {#diagnostics}

`planned`

### Hardware and dependencies

- 捕获FDT及R7/P20历史证据；官方固定提交16e592abee8a550dec8154ed1f38b5ebc4f16213。

### Configuration

- 见board.toml及docs/P21\_OFFICIAL\_ADAPTATION.md；未通过的配置保留guard。

### Procedure

- 按官方对应porting子系统文档，先来源与接口核对，再编译/打包门禁，最后实机验证。

### Development findings

- R7观察器与新官方审计独立保留，USB及持久日志当前不启用。

### Completion criteria

- 所选实现及全部板级依赖具有可重复构建记录与明确硬件证据等级。

### Current limits

- 本记录描述迁移目标；planned不否认历史阶段已有实现，也不把历史结果冒充新源码验证。

### Next action

- 新镜像离线检查全过后才进入身份固定的单次临时启动。

### Evidence

- `source`: [当前适配与待办记录，非新镜像实机成功声明](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/950b0df3bf326129c743f3415eececaceb2e4bec/docs/P21_OFFICIAL_ADAPTATION.md)

</div>
