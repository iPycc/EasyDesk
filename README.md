# EasyDesk Windows 客户端

EasyDesk 是 C++17 + Qt 5.15.12 Widgets + QSS + SVG 的 Windows x64 原生客户端。当前主窗口、侧边栏、设备列表、远程协助表单、设置与独立会话窗口已经按功能拆分。

## 当前状态

- 本机名称、系统、主板、处理器和网卡信息来自 Windows 原生 API。
- 设备列表启动时为空，只保存用户实际添加的设备；数据写入 `settings.ini`。
- 本机设备 ID 和 8 位协助码由本地会话服务生成，界面明确显示“信令未连接”。
- 没有种子设备、假在线状态、假 IP/延迟、合成桌面或固定文件队列。
- WSS 信令、身份签名、ICE/STUN/TURN、桌面采集、媒体、键鼠输入和真实文件通道尚未接入；对应入口不会伪装成可用。

实际仓库继续使用 Qt Widgets。PRD 中的 Rust/Slint 路线作为历史方案保留，不在同一迭代重写 UI 技术栈。

## 构建

```bat
scripts\build.cmd
```

工程固定使用项目内的 Qt 5.15.12 与 MSVC x64 工具链，不修改全局 PATH。

## 打包

```powershell
.\scripts\package.ps1 -PackageName EasyDesk-latest
```

发布目录包含 `EasyDesk.exe`、Qt 运行库、平台插件和 MSVC 运行库。请保留完整目录，不要只复制 EXE。

## 数据

默认配置位于：

```text
%LOCALAPPDATA%\EasyDesk\EasyDesk\settings.ini
```

若旧版存在 `preview.ini` 且尚未生成 `settings.ini`，启动时会复制旧配置。开发时可用 `EASYDESK_DATA_DIR` 指向隔离目录。

## 目录

```text
src/app/                 启动与依赖装配
src/core/                设备、会话、传输数据结构
src/features/            功能服务接口
src/data/local/          本地持久化和 Windows 系统读取
src/ui/components/       通用控件、SVG 与原生视频渲染面
src/ui/pages/            页面布局
src/ui/controllers/      页面交互与服务协调
src/ui/dialogs/          应用内模态面板
src/ui/windows/          主窗口、侧边栏和独立会话窗口
src/ui/theme/            主题与 QSS 装载
assets/                  图标、样式和程序资源
```

详细模块边界见 [src/README.md](src/README.md)，产品要求见 [docs/EasyDesk_1.0.0_MVP_PRD_Agent.md](docs/EasyDesk_1.0.0_MVP_PRD_Agent.md)。
