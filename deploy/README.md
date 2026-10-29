# EasyDesk Linux 部署

该 Compose 栈把职责分开：`signal` 提供 C++ HTTP/WebSocket 信令，`caddy` 提供公网 HTTPS/WSS，`coturn` 只在直连失败时转发加密的媒体或 DataChannel 流量。

## 准备

1. 将域名 A/AAAA 记录指向服务器，复制 `.env.example` 为 `.env` 并填写真实值。
2. 创建 TURN secret，不能提交到 Git：

   ```sh
   install -m 700 -d deploy/secrets
   openssl rand -hex 32 > deploy/secrets/turn_shared_secret
   chmod 600 deploy/secrets/turn_shared_secret
   ```

3. 在云防火墙和系统防火墙放行：TCP 80/443、UDP 443、TCP/UDP 3478、UDP 49160–49259。不要公开 8080。
4. 如果服务器位于 NAT 后，`TURN_EXTERNAL_IP` 使用 coturn 的 `公网IP/内网IP` 格式；直连公网主机只填公网 IP。

## 启动

```sh
docker compose --env-file deploy/.env -f deploy/compose.yaml config
docker compose --env-file deploy/.env -f deploy/compose.yaml build --pull signal
docker compose --env-file deploy/.env -f deploy/compose.yaml up -d
docker compose --env-file deploy/.env -f deploy/compose.yaml ps
curl --fail https://你的域名/healthz
```

`https://域名/ws` 是客户端 WSS 地址。SQLite 位于 `signal-data` volume；TURN secret 通过 Docker secret 注入，Compose 配置和镜像内均不包含实际值。

## 验收边界

- `/healthz`、容器 healthy 或 3478 可连接只证明服务在运行。
- TURN 必须用两台真实设备做强制 relay，检查选中的 ICE candidate pair 为 relay，并实际传输画面/控制或文件后才算中继通过。
- 本基线开启 TURN/UDP 与 TURN/TCP 3478，未开启 TURN/TLS 5349。TURN/TLS 需要单独 PEM 证书，并避免和同一 IP 上的 HTTPS 443 冲突。
- 部署前确认云服务商允许把实例用于流量中继，并按中继并发、码率与出口流量核算成本。
