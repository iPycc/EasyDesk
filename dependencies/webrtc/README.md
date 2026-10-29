# libwebrtc 依赖

EasyDesk 固定使用官方 M140 分支提交 `36ea4535a500ac137dbf1f577ce40dc1aaa774ef`。源码、DEPS 和 GN 产物保存在被忽略的 `.toolchain/webrtc/`，不把数 GiB 第三方源码提交到业务仓库。

```cmd
scripts\dependencies\fetch-webrtc.cmd
scripts\dependencies\build-webrtc.cmd
```

构建使用官方 depot_tools、GN 和 autoninja，目标为静态 `webrtc.lib`。`manifest.json` 同时固定 depot_tools；脚本关闭自动更新并核对源码 revision。

M140 的 GN 步骤即使只构建 Release 静态库也会复制 `dbghelp.dll`。如果系统未安装可选的 Windows Desktop Debuggers，脚本会把同版 SDK App Certification Kit 中由 Microsoft 签名的副本放入 `.toolchain/windows-sdk-debug`，并用目录联接引用真实 SDK 的头文件、库与工具。编译仍使用已安装的 10.0.26100.0 SDK，不修改 WebRTC 源码。

H.264 构建开关不等于硬件编解码已接入。MF 编码器工厂、运行时 codec 统计、第三方许可清单和双机媒体验证分别完成后才能声明对应能力。

`src/media/bridge` 由同一套 WebRTC Clang 工具链构建为 `easydesk_media.dll`。构建脚本临时复制桥接源码并应用 `easydesk-media-root.patch`，GN 和 Ninja 完成后立即清理暂存目录并还原固定源码。Qt 客户端只通过 `easydesk_media_abi.h` 的 C ABI 动态加载它，避免 Chromium libc++ 与 Qt/MSVC STL ABI 混用。

ABI v2 提供带 session/permission/ICE generation 的 PeerConnection 会话、offer/answer、Trickle ICE、临时 STUN/TURN 配置、单调撤权和有界事件队列。控制端预协商视频收流方向以及 `control.v1`、`input.v1`、`pointer.v1` DataChannel；被控端预协商视频发流方向。事件队列单事件上限 256 KiB，容量或数量溢出时关闭 PeerConnection，不继续无界缓存。

这里仍不代表远程桌面已经完成：DXGI 纹理尚未接入视频轨道，解码帧尚未绑定 HWND，DataChannel 尚未接入输入协议，经过签名验证的 session/2 信令也尚未调用该 ABI。只有双机身份、DTLS、首帧、输入 Ready 和真实 selected candidate pair 验收后才能显示会话已连接。
