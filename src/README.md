# 源码分层与后端接入

当前是 Qt Widgets 前端预览。远程设备、会话、文件队列仍使用 mock；本机信息由 Windows 原生服务读取，不包含真实远程认证、网络协议或媒体传输。

| 目录 | 职责 |
| --- | --- |
| `app/` | 程序入口、依赖组装、服务生命周期与本地设置文件位置 |
| `core/` | 设备、会话、传输数据结构，不依赖 QWidget |
| `features/devices/` | 设备快照与异步增删改、收藏接口 |
| `features/sessions/` | 本机身份、连接表单校验、会话选项与指标接口 |
| `features/transfers/` | 文件队列、暂停、继续与取消接口 |
| `features/system/` | 本机信息快照、刷新与应用显示名称接口 |
| `data/local/` | 计算机名、主板、处理器、网卡 MAC 的 Windows 原生读取实现 |
| `data/mock/` | 示例设备、模拟指标和模拟传输计时器的实现 |
| `ui/pages/` | 设备详情、列表、开始远程、设置页的控件布局 |
| `ui/controllers/` | 页面事件与服务接口之间的协调、设置读写映射 |
| `ui/dialogs/` | 添加／修改／移除、文件队列和关于窗口 |
| `ui/windows/` | 主窗口、导航、独立远程窗口、自定义标题栏与窗口行为 |
| `ui/components/` | 通用控件、布局辅助、SVG 引擎和桌面占位视图 |
| `ui/theme/` | 主题令牌与控件样式；具体 QSS 在 `assets/`，字体与高 DPI 初始化在 `app/main.cpp` |
| `diagnostics/` | 既有 `--verify` 运行检查与截图，不参与业务决策 |

依赖方向为 `ui → features → core`；`data/mock → features + core`。只有 `app/application_context.cpp` 选择具体服务实现。UI 不包含 mock 实现头文件，也不直接修改设备容器。窗口和服务由不同对象管理，服务存活时间覆盖主窗口与独立远程窗口。

## 设备接口

`DeviceService::devices()` 是 GUI 线程上的只读缓存；`find()` 返回的指针仅供当前调用读取，不跨事件保存。界面需要等待用户操作时，应复制设备 ID 和名称。

写操作返回请求编号，不代表已经完成。实现必须在调用返回之后，通过 `operationFinished(requestId, success, error)` 报告一次结果；成功后先更新缓存并发送 `changed()`。当前 mock 通过事件队列交付结果，UI 已按请求编号处理成功和失败，添加窗口会等待结果后再关闭。

后端适配器应保存相同语义：异步请求成功后更新本地快照；失败时保留快照并返回错误。不应在 UI 线程等待网络。跨线程结果要通过 queued connection 送回 GUI 线程后再更新快照。

## 会话接口

`SessionService` 提供本机身份、临时码、连接校验、开始／结束会话、画质／帧率请求，以及 `telemetryChanged`、`optionsChanged`、`failed` 通知。`SessionTelemetry` 与 `SessionOptions` 已注册 Qt 元类型，可用于 queued connection。

标题栏与指标面板从服务读取数据，不再保存示例 IP 或延迟。`simulated` 决定模拟标识。画质与帧率操作发送给服务，界面根据服务通知更新。当前实现不会进行认证或真实连接，`start()` 的 mock 实现只初始化模拟状态。

接真实后端时还需按实际协议完善连接中／已连接／失败／重连状态、认证与取消语义。媒体解码、画面投递和键鼠输入也尚未实现：目前 `ui/components/desktop_view.cpp` 仍绘制示例桌面，后续可在其原生 `winId()` 窗口接入渲染器。仅替换会话指标服务不会产生真实远控能力。

## 传输与本地设置

文件队列的数据和进度由 `TransferService` 提供，UI 只显示并发送暂停、继续、取消命令。模拟文件名和计时器位于 `data/mock/mock_transfer_service.cpp`。后端接入时需再补真实文件选择、传输任务标识、路径和错误恢复；当前没有读取或发送真实文件。

`ApplicationContext` 创建和持有 QSettings。`ui/controllers/settings_controller.cpp` 负责将页面控件映射到设置项；保存设置不等于服务器已应用设置，网络地址目前也只保存在本地。

## 更换实现的位置

1. 在 `data/` 下新增实际适配器，实现对应 `features/` 接口。
2. 只在 `app/application_context.cpp` 替换服务构造；不要让页面直接包含网络客户端或 mock 实现。
3. 保持快照、异步完成与错误通知的约定，再按后端协议补会话生命周期和媒体通道。
4. 使用 `.clang-format` 保持源码格式，新增文件同步加入 `EasyDesk.pro`。

尚未约定后端进程、端口、API 路径、鉴权和消息格式，此处没有预设协议或写入虚构网络实现。

## 本机信息与模态面板

`LocalSystemService` 的 `information()` 返回本机快照，`refresh()` 重新读取，`setDisplayName()` 只保存应用内别名。`WindowsSystemService` 读取 GetComputerNameW、注册表只读键和 GetAdaptersAddresses；网卡列表包含符合类型的以太网与 WLAN 适配器，可能包含虚拟网卡。未读取到字段时 UI 明确显示“未读取到”。没有发送硬件数据，也不修改系统名称或网卡设置。

设备行由 `ui/components/device_list.*` 显示，使用设备分类和系统类型，不依赖 QTableView。`ui/components/selection_indicator.*` 负责侧栏与设置标签动画。`ui/windows/dialog.*` 是嵌入宿主窗口的 ContentDialog 式面板与遮罩，负责焦点、Esc 和快捷键恢复；它不是独立 QDialog。远程窗口仍由 `SessionWindow` 管理。

快捷启动配置由 `quick_launch_controller.cpp` 按设备保存在 QSettings；当前仅保存名称和远程应用路径，没有调用本机或远程进程执行。
