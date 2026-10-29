# 源码模块边界

依赖方向保持为 `presentation → domain`，`infrastructure` 和 `platform` 实现领域接口，只由 `application/app/ApplicationContext` 选择并注入。页面不直接访问设置文件、网络客户端或系统 API。

| 目录 | 职责 |
| --- | --- |
| `application/app/` | 程序入口、服务生命周期与依赖装配 |
| `domain/core/` | 不依赖 QWidget 的设备、会话和传输结构 |
| `domain/features/devices/` | 设备快照与异步增删改接口 |
| `domain/features/sessions/` | 本机身份、连接校验、会话选项和遥测接口 |
| `domain/features/sessions/model/` | 会话权限、状态机与就绪事实 |
| `domain/features/sessions/input/` | 有界键鼠命令与上下文字段 |
| `domain/features/transfers/` | 文件任务队列接口 |
| `domain/features/system/` | 本机系统信息接口 |
| `infrastructure/data/local/` | 设置持久化和未连接会话状态 |
| `platform/windows/data/local/` | Windows 系统信息读取 |
| `platform/windows/media/capture/` | DXGI 桌面纹理、变更区域与光标采集 |
| `platform/windows/input/` | SendInput、输入 lease 与失联释放 |
| `presentation/ui/components/` | 通用控件、图标、菜单、选择动画和原生视频面 |
| `presentation/ui/pages/` | 设备、列表、远程协助和设置页面 |
| `presentation/ui/controllers/` | UI 事件与功能接口之间的协调 |
| `presentation/ui/dialogs/` | 应用内模态面板 |
| `presentation/ui/windows/` | 主窗口、侧边栏、标题栏和会话窗口 |

## 设备

`SettingsDeviceService` 从 `settings.ini` 读取用户添加的设备。启动不注入种子数据，远端在线状态在信令订阅接入前保持未知/离线。写操作返回请求编号，完成后通过 `operationFinished` 回传结果。

## 会话

`LocalSessionService` 负责持久化本机 ID 和生成当前进程的 8 位协助码。它明确报告“信令未连接”，不会生成对端、网络遥测或打开假会话。后续 WSS 适配器必须保留同一接口，并在真实授权、传输与首帧事实就绪后更新状态。

`DesktopView` 是原生 QWidget/HWND 渲染目标。当前只绘制离线/等待状态，不生成桌面内容。媒体实现应绑定 `winId()`，不得通过 UI 层逐帧传递像素。

## 文件传输

`LocalTransferService` 返回空任务列表并报告通道不可用。真实实现需要由已授权会话创建任务，并补齐文件选择、接收确认、背压、校验、取消和恢复。

## 后端接入

新增适配器时放入独立功能目录，只在 `ApplicationContext` 替换构造。网络回调必须通过 queued connection 回到 GUI 线程；UI 线程不能等待网络或媒体工作。
