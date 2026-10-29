EasyDesk Windows 客户端
=======================

运行：双击 EasyDesk.exe。

请保留发布目录内的 Qt DLL、platforms、imageformats 和 MSVC 运行库；本包不是单文件程序。

当前能力
--------
- 读取本机 Windows 设备信息。
- 保存用户添加的设备与界面设置。
- 显示本机设备 ID、8 位协助码和真实连接状态。
- 提供设备、远程协助与设置界面。

当前限制
--------
- 信令服务尚未连接。
- 未实现桌面采集、视频传输、键鼠输入和文件通道。
- 不包含示例设备、合成远程桌面或模拟文件进度。

数据目录
--------
默认：%LOCALAPPDATA%\EasyDesk\EasyDesk\settings.ini

设置 EASYDESK_DATA_DIR 可指定独立数据目录。旧 preview.ini 仅用于一次兼容迁移。

紧急停止快捷键预留为 Ctrl+Alt+Shift+F11；真实会话接入后必须保证停止不依赖服务器在线。
