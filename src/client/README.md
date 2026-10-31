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
| `infrastructure/data/local/` | 本地设备列表和文件队列适配 |
| `infrastructure/data/settings/identity/` | 9 位设备索引和设备名持久化 |
| `infrastructure/network/signaling/` | 注册协议、启动消息和信令状态机 |
| `platform/windows/data/local/` | Windows 系统信息读取 |
| `platform/windows/identity/` | DPAPI 设备私钥种子存储 |
| `platform/windows/network/winhttp/` | WSS/回环开发 WS 传输 |
| `platform/windows/media/capture/` | DXGI 桌面纹理、变更区域与光标采集 |
| `platform/windows/media/webrtc/` | WebRTC C ABI 动态加载与运行时生命周期 |
| `platform/windows/input/` | SendInput、输入 lease 与失联释放 |
| `presentation/ui/components/` | 通用控件、图标、菜单、选择动画和原生视频面 |
| `presentation/ui/pages/` | 设备、列表、远程协助和设置页面 |
| `presentation/ui/controllers/` | UI 事件与功能接口之间的协调 |
| `presentation/ui/dialogs/` | 应用内模态面板 |
| `presentation/ui/windows/` | 主窗口、侧边栏、标题栏和会话窗口 |

## 设备

`SettingsDeviceService` 从 `settings.ini` 读取用户添加的设备。启动不注入种子数据，远端在线状态在信令订阅接入前保持未知/离线。写操作返回请求编号，完成后通过 `operationFinished` 回传结果。

## 会话

`SignalingClient` 使用 DPAPI 持久身份完成 Ed25519 挑战注册，协助码只采用信令服务签发的租约。`SignalingSessionService` 在 session/2 申请链接入前保持连接入口禁用，不生成本地随机协助码或打开假会话。

`DesktopView` 是原生 QWidget/HWND 渲染目标。当前只绘制离线/等待状态，不生成桌面内容。媒体实现应绑定 `winId()`，不得通过 UI 层逐帧传递像素。

`MediaRuntimeLibrary` 只从应用目录动态加载 `easydesk_media.dll`，一次校验 ABI v3 的全部导出后创建唯一 WebRTC 运行时。`MediaSession` 将 16 字节 session ID、epoch、权限、ICE server 和事件缓冲映射为 C POD；身份模块只交换固定大小的 Ed25519 字节缓冲；Qt/MSVC 与 WebRTC/Clang 之间不交换 C++ 对象。运行时由会话共享持有，最后一个会话销毁前不会卸载 DLL。媒体 DLL 缺失、导出不全或初始化失败时保持未就绪，不伪造连接能力。

## 文件传输

`LocalTransferService` 返回空任务列表并报告通道不可用。真实实现需要由已授权会话创建任务，并补齐文件选择、接收确认、背压、校验、取消和恢复。

## 后端接入

新增适配器时放入独立功能目录，只在 `ApplicationContext` 替换构造。网络回调必须通过 queued connection 回到 GUI 线程；UI 线程不能等待网络或媒体工作。
