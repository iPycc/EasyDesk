# 服务端模块边界

依赖方向保持为 `transport → application → protocol`。`infrastructure` 实现设备与存储能力，`security` 提供无业务状态的密码学实现。协议层不依赖 HTTP、WebSocket 或数据库。

| 目录 | 职责 |
| --- | --- |
| `src/application/config/` | 启动配置解析 |
| `src/application/hub/` | 在线连接、订阅与消息路由 |
| `src/infrastructure/devices/` | 设备身份仓储 |
| `src/infrastructure/storage/` | SQLite 连接与迁移 |
| `src/protocol/encoding/` | 协议编码工具 |
| `src/protocol/registration/` | 设备注册验证 |
| `src/protocol/session/` | `session/2` 会话信封与消息边界 |
| `src/protocol/signaling/` | 点对点信令信封验证 |
| `src/security/crypto/` | Ed25519 与安全随机数 |
| `src/transport/net/` | HTTP、WebSocket 会话与监听器 |

新功能优先在现有层内增加独立子目录；只有跨层装配进入 `application`，网络和数据库细节不得进入 `protocol`。
