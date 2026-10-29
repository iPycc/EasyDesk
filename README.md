# EasyDesk Windows 前端

C++17 + **Qt 5.15.12 Widgets + QSS + Svg**，原生 Windows x64 客户端界面。默认 920 × 680 DIP，最小 860 × 620。自定义标题栏，支持拖动、缩放、最小化和关闭，不提供最大化。默认浅色，可切换深色；采用 Windows 11 设置式的分组布局与滑动开关，强调色为品牌蓝 `#3478ed`。

**远程功能仍为前端预览；本机信息已经接入 Windows 读取服务。** 本机名称、主板、处理器和网卡 MAC 来自系统；远程设备、连接状态、画面与传输仍为 mock，未实现真实远控、桌面采集或键鼠注入。需求文档中的 Rust/Slint 路线、双机协议和媒体任务不属于此次交付，以用户最新指定的 C++ + Qt Widgets 为准。

## 直接运行

当前新版双击 `dist/EasyDesk-sidebar/EasyDesk.exe`，或者解压 `dist/EasyDesk-1.0.0-preview-win64.zip` 后运行其中的 EXE。发布包内已带齐 Qt DLL、Windows 平台插件和 MSVC 运行库，无需安装开发环境。

请保留整个便携文件夹。当前交付是带依赖的便携 EXE 目录，不是将所有依赖静态塞入一个 EXE。

## 页面与交互

- **我的设备**：展开显示已添加的示例设备、全部设备。设备详情含在线/离线状态、模拟桌面预览、原生工具栏、画质选择、双屏示例、文件队列和默认为空的快捷启动区。
- **全部设备／收藏设备**：按电脑、手机／平板分组的设备行，不使用表格。本机显示真实电脑名称和“本机”标记，详情展示系统硬件信息；远程行提供详情与进入设备入口，支持搜索、在线筛选、添加、重命名与移除。
- **远程控制**：开始远程、收藏设备。开始页提供本机演示 ID、显示/隐藏/刷新示例码、连接表单校验与示例填充。任意 8 位数字码仅用于模拟，不能用作身份认证。
- **模拟会话**：确认后打开独立桌面窗口。标题栏提供信号按钮和三点菜单；信号按钮展开延迟、双端 IP、帧率、丢包与传输方式。三点菜单提供帧率、画质、全屏、文件、屏幕、比例和退出控制。窗口底部不再放置操作栏，支持 F11 全屏和 Esc 返回/关闭。这些指标与选项均为模拟界面状态，不读取真实网络或控制媒体编码。
- **文件传输**：只处理固定示例文件名和模拟进度；支持暂停/继续/取消，不读取和发送实际文件。
- **设置**：常规、安全、键盘、网络四个原生标签页。设置可保存/恢复默认；不改变系统设置或连接任何服务。

远程窗口使用无父级的独立顶层窗口，拥有自己的任务栏入口，可以和主界面分别最小化、还原；Windows 任务栏按系统设置将同一应用的窗口分组。全屏会隐藏本地标题栏、信号和三点按钮，并移除所有窗口边距，只展示模拟桌面。F11 或 Esc 可返回原位置、原尺寸的窗口模式；窗口模式下再按 Esc 关闭会话。全屏下仍可用 `Ctrl+Alt+Shift+F12` 主动打开会话菜单。

字体优先使用 Windows 自带的 **Microsoft YaHei UI（微软雅黑 UI）**，其次为 Microsoft YaHei；未安装时使用系统界面字体。常规文字采用 14px、正常字重，不打包或下载字体。页面只保留必要的标题、字段、状态和操作标签，消息在右上角短暂显示，可手动关闭。

侧栏宽 240px，导航使用 36px 按钮行与 4px 行间距，左右留出 4px；二级项向右缩进 24px，图标与文字间留 12px，选中背景保持整行。顶部与侧边栏不绘制分隔边框。关于、添加、重命名、设备详情、移除确认、校验提示、连接确认和文件队列使用当前窗口内的模态面板，背后遮罩并约束键盘焦点，不创建独立提示窗口。远程桌面仍是独立窗口。

当前继续使用 Qt，按微软 Windows 11 的控件与浮层几何规范调整，尚未迁移到 WinUI 3。控件和导航不绘制焦点外框，键盘操作仍可使用；导航以中性背景和侧边标记表示选择。分组标题与箭头共用一整行，折叠后不保留额外间距。本机设备区保留独立的固定边框。设备详情标题下不显示系统、设备 ID 或位置备注，在线状态使用更明显的绿色底色。

下拉选择、三点菜单、子菜单及文本编辑的右键菜单统一采用圆角浮层、留白、选项悬停和勾选标记；支持键盘选择与 Esc 取消。页面控件采用 4px 圆角，临时菜单采用 8px 圆角。参考：[微软 Windows 几何规范](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/geometry)。

界面图标来自 `assets/icons/*.svg`，由 QtSvg 按当前主题及显示尺寸渲染；下拉箭头与复选标记同样使用 SVG。EXE 文件图标保留 Windows 所需的 ICO 格式。最小化调用 Windows 原生路径，动画遵循系统设置，不修改系统动画开关。

设备改动在本次进程内保留，重启恢复种子数据。设置保存到 `QStandardPaths::AppLocalDataLocation/preview.ini`；Windows 默认是 `%LOCALAPPDATA%/EasyDesk/EasyDesk/preview.ini`。可用 `EASYDESK_DATA_DIR` 指定隔离目录。

顶部原有设备卡显示真实电脑名称，图标根据本机系统使用 Windows、Apple 或 Linux 标志，不在设置上方额外添加本机卡。侧栏“我的设备”“远程控制”分组采用 16px 半粗文字与 22px 图标。侧栏的 Windows、macOS、Linux SVG 图标始终为蓝色，选中指示条和设置标签下划线采用 200ms 动画。标题栏辅助按钮保留中性色。

`WindowsSystemService` 使用原生 API 读取本机信息，不启动 PowerShell。应用内重命名只保存显示名称，不修改 Windows 计算机名。快捷启动配置按设备保存，可添加或移除；实际远程执行尚未接入。

## 开发环境

本机已检测到 Visual Studio 2022 C++ 工具链（MSVC 14.44）和 Windows SDK。Qt 5.15.12 从官方 QtBase 源码构建并安装在本项目中：

```text
.toolchain/qt-5.15.12/       Qt SDK（qmake、头文件、库、工具、插件）
.toolchain/jom/             Qt 官方并行构建工具
.toolchain/src/             Qt 5.15.12 原始源码
.toolchain/qt-build/        Qt 编译目录
.toolchain/qt-build.log     Qt 配置、编译和安装日志
```

没有修改全局 PATH，开发脚本自动加载 Visual Studio 环境。`scripts/qt-env.cmd` 可用于现有 cmd 开发终端。Qt Creator 可直接打开 `EasyDesk.pro`，选择本地 Qt SDK 的 qmake 与 MSVC x64 编译器。

构建客户端：

```bat
scripts\build.cmd
```

首次构建匹配版本的 SVG 模块（源码保留在 `.toolchain/src/qtsvg-everywhere-src-5.15.12`）：

```bat
scripts\build-svg.cmd
```

重新构建 Qt（已完成，通常不需要重复）：

```bat
scripts\build-qt.cmd
```

打包：

```powershell
.\scripts\package.ps1
# 原程序被占用时可输出到独立目录
.\scripts\package.ps1 -PackageName EasyDesk-latest
```

工程在 qmake 和 C++ 编译阶段均检查 Qt **必须为 5.15.12**。链接 Core、Gui、Widgets、Svg；不使用 QML、WebView、Electron、Python 或 Rust 运行时。

## 代码位置

```text
src/app/                   启动与依赖组装，选择具体服务实现
src/core/                  设备、会话、传输数据结构
src/features/              设备、会话、传输、本机信息的服务接口
src/data/mock/             mock 数据、模拟队列与模拟指标实现
src/data/local/            Windows 本机信息读取服务
src/ui/pages/              设备、列表、连接、设置页面布局
src/ui/controllers/        交互事件、服务调用与设置映射
src/ui/dialogs/            设备操作、文件队列和关于窗口
src/ui/windows/            主窗口、导航、独立远程窗口和标题栏
src/ui/components/         通用控件、SVG、桌面占位视图
src/ui/theme/              深浅色主题与控件基础样式
src/diagnostics/           --verify 检查与运行截图
assets/style.qss           全局 QSS 与颜色
assets/icons/              SVG 图标源文件
assets/resources.qrc       图标和主题资源
```

未来接入真实媒体时，可在 `DesktopView::winId()` 对应的原生句柄上适配渲染器；当前没有实现视频解码或 D3D 渲染。界面通过 `DeviceService`、`SessionService` 和 `TransferService` 访问数据与操作，由 `ApplicationContext` 注入具体实现。设备写操作使用异步请求编号和完成通知，UI 不直接修改 mock 容器。

右侧页面使用独立底板：浅色为淡蓝灰的白色，深色保留 Windows 中性灰黑配色（侧栏 #202020、内容 #292929），统一覆盖设备、列表、连接和设置页。

后端接入边界、目录职责和异步约定见 [src/README.md](src/README.md)。当前没有预设后端地址、API、鉴权协议或媒体通道，远程服务运行时仍使用 mock。

## 验证与边界

见 `docs/VALIDATION.md`。`EasyDesk.exe --verify <输出目录>` 会执行前端交互检查，生成页面截图和 JSON 报告。执行时应使用隔离的 `EASYDESK_DATA_DIR`，以免覆盖正在使用的界面偏好。

本次交付不宣称 PRD 中真实远控、双机连接、安全会话或文件传输的产品验收完成。
