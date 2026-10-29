# libwebrtc 依赖

EasyDesk 固定使用官方 M140 分支提交 `36ea4535a500ac137dbf1f577ce40dc1aaa774ef`。源码、DEPS 和 GN 产物保存在被忽略的 `.toolchain/webrtc/`，不把数 GiB 第三方源码提交到业务仓库。

```cmd
scripts\dependencies\fetch-webrtc.cmd
scripts\dependencies\build-webrtc.cmd
```

构建使用官方 depot_tools、GN 和 autoninja，目标为静态 `webrtc.lib`。`manifest.json` 同时固定 depot_tools；脚本关闭自动更新并核对源码 revision。

M140 的 GN 步骤即使只构建 Release 静态库也会复制 `dbghelp.dll`。如果系统未安装可选的 Windows Desktop Debuggers，脚本会把同版 SDK App Certification Kit 中由 Microsoft 签名的副本放入 `.toolchain/windows-sdk-debug`，并用目录联接引用真实 SDK 的头文件、库与工具。编译仍使用已安装的 10.0.26100.0 SDK，不修改 WebRTC 源码。

H.264 构建开关不等于硬件编解码已接入。MF 编码器工厂、运行时 codec 统计、第三方许可清单和双机媒体验证分别完成后才能声明对应能力。
