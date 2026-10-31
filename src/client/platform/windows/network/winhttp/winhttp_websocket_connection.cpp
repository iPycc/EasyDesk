#include "network/winhttp/winhttp_websocket_connection.h"

#include <array>
#include <atomic>
#include <mutex>
#include <utility>
#include <windows.h>
#include <winhttp.h>

namespace {

constexpr int kMaximumMessageSize = 64 * 1024;
constexpr DWORD kReceiveBufferSize = 16 * 1024;

QString windowsErrorMessage(DWORD errorCode) {
    wchar_t *message = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, 0, reinterpret_cast<wchar_t *>(&message), 0, nullptr);
    const QString result = length > 0 && message != nullptr
                               ? QString::fromWCharArray(message, static_cast<int>(length)).trimmed()
                               : QStringLiteral("Windows error %1").arg(errorCode);
    if (message != nullptr)
        LocalFree(message);
    return result;
}

QString operationError(const QString &operation, DWORD errorCode) {
    return QStringLiteral("%1: %2").arg(operation, windowsErrorMessage(errorCode));
}

bool isLoopbackHost(const QString &host) {
    const QString normalized = host.trimmed().toLower();
    return normalized == QStringLiteral("localhost") ||
           normalized == QStringLiteral("127.0.0.1") ||
           normalized == QStringLiteral("::1");
}

void closeHandle(HINTERNET handle) {
    if (handle != nullptr)
        WinHttpCloseHandle(handle);
}

}  // namespace

struct WinHttpWebSocketConnection::Implementation {
    mutable std::mutex handlesMutex;
    std::mutex sendMutex;
    HINTERNET session = nullptr;
    HINTERNET connection = nullptr;
    HINTERNET request = nullptr;
    HINTERNET webSocket = nullptr;
    std::atomic_bool open{false};
    bool closing = false;
    bool shutdownSent = false;

    bool installHandle(HINTERNET &slot, HINTERNET handle) {
        bool accepted = false;
        {
            std::lock_guard lock(handlesMutex);
            if (!closing) {
                slot = handle;
                accepted = true;
            }
        }
        if (!accepted)
            closeHandle(handle);
        return accepted;
    }

    void closeAll() {
        std::lock_guard sendLock(sendMutex);
        HINTERNET ownedWebSocket = nullptr;
        HINTERNET ownedRequest = nullptr;
        HINTERNET ownedConnection = nullptr;
        HINTERNET ownedSession = nullptr;
        {
            std::lock_guard handlesLock(handlesMutex);
            closing = true;
            open.store(false, std::memory_order_release);
            shutdownSent = false;
            ownedWebSocket = std::exchange(webSocket, nullptr);
            ownedRequest = std::exchange(request, nullptr);
            ownedConnection = std::exchange(connection, nullptr);
            ownedSession = std::exchange(session, nullptr);
        }
        closeHandle(ownedWebSocket);
        closeHandle(ownedRequest);
        closeHandle(ownedConnection);
        closeHandle(ownedSession);
    }
};

WinHttpWebSocketConnection::WinHttpWebSocketConnection()
    : m_impl(std::make_unique<Implementation>()) {}

WinHttpWebSocketConnection::~WinHttpWebSocketConnection() {
    abort();
}

bool WinHttpWebSocketConnection::open(const QUrl &endpoint,
                                      bool allowInsecureLoopback,
                                      QString &error) {
    error.clear();
    const QString scheme = endpoint.scheme().toLower();
    const QString host = endpoint.host();
    const bool secure = scheme == QStringLiteral("wss");
    if ((!secure && scheme != QStringLiteral("ws")) || host.isEmpty() ||
        !endpoint.userInfo().isEmpty() || endpoint.hasFragment()) {
        error = QStringLiteral("Signaling endpoint must be a ws:// or wss:// URL without credentials or a fragment.");
        return false;
    }
    if (!secure && (!allowInsecureLoopback || !isLoopbackHost(host))) {
        error = QStringLiteral("Unencrypted WebSocket signaling is restricted to explicit loopback development mode.");
        return false;
    }

    const int endpointPort = endpoint.port(secure ? INTERNET_DEFAULT_HTTPS_PORT
                                                   : INTERNET_DEFAULT_HTTP_PORT);
    if (endpointPort < 1 || endpointPort > 65535) {
        error = QStringLiteral("Signaling endpoint contains an invalid port.");
        return false;
    }

    {
        std::lock_guard lock(m_impl->handlesMutex);
        if (m_impl->session != nullptr || m_impl->connection != nullptr ||
            m_impl->request != nullptr || m_impl->webSocket != nullptr) {
            error = QStringLiteral("WebSocket connection is already active.");
            return false;
        }
        m_impl->closing = false;
        m_impl->shutdownSent = false;
    }

    const HINTERNET session = WinHttpOpen(
        L"EasyDesk/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        error = operationError(QStringLiteral("WinHTTP initialization failed"), GetLastError());
        return false;
    }
    if (!m_impl->installHandle(m_impl->session, session)) {
        error = QStringLiteral("WebSocket connection was cancelled.");
        return false;
    }
    WinHttpSetTimeouts(session, 10000, 10000, 10000, 15000);
#ifdef WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL
    DWORD keepAliveInterval = 30000;
    WinHttpSetOption(session, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL,
                     &keepAliveInterval, sizeof(keepAliveInterval));
#endif

    const std::wstring hostName = host.toStdWString();
    const HINTERNET connection = WinHttpConnect(
        session, hostName.c_str(), static_cast<INTERNET_PORT>(endpointPort), 0);
    if (connection == nullptr) {
        error = operationError(QStringLiteral("Signaling connection failed"), GetLastError());
        abort();
        return false;
    }
    if (!m_impl->installHandle(m_impl->connection, connection)) {
        error = QStringLiteral("WebSocket connection was cancelled.");
        abort();
        return false;
    }

    QString target = endpoint.path(QUrl::FullyEncoded);
    if (target.isEmpty())
        target = QStringLiteral("/");
    const QString query = endpoint.query(QUrl::FullyEncoded);
    if (!query.isEmpty())
        target += QStringLiteral("?") + query;
    const std::wstring requestTarget = target.toStdWString();
    const DWORD requestFlags = secure ? WINHTTP_FLAG_SECURE : 0;
    const HINTERNET request = WinHttpOpenRequest(
        connection, L"GET", requestTarget.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, requestFlags);
    if (request == nullptr) {
        error = operationError(QStringLiteral("WebSocket request creation failed"), GetLastError());
        abort();
        return false;
    }
    if (!m_impl->installHandle(m_impl->request, request)) {
        error = QStringLiteral("WebSocket connection was cancelled.");
        abort();
        return false;
    }

    if (!WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) {
        error = operationError(QStringLiteral("WebSocket upgrade setup failed"), GetLastError());
        abort();
        return false;
    }
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        error = operationError(QStringLiteral("WebSocket upgrade request failed"), GetLastError());
        abort();
        return false;
    }
    if (!WinHttpReceiveResponse(request, nullptr)) {
        error = operationError(QStringLiteral("WebSocket upgrade response failed"), GetLastError());
        abort();
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(request,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                             &statusCodeSize, WINHTTP_NO_HEADER_INDEX) ||
        statusCode != HTTP_STATUS_SWITCH_PROTOCOLS) {
        error = statusCode == 0
                    ? operationError(QStringLiteral("WebSocket status query failed"), GetLastError())
                    : QStringLiteral("WebSocket upgrade returned HTTP %1.").arg(statusCode);
        abort();
        return false;
    }

    const HINTERNET webSocket = WinHttpWebSocketCompleteUpgrade(request, 0);
    if (webSocket == nullptr) {
        error = operationError(QStringLiteral("WebSocket upgrade failed"), GetLastError());
        abort();
        return false;
    }

    bool accepted = false;
    bool closeRequest = false;
    {
        std::lock_guard lock(m_impl->handlesMutex);
        if (!m_impl->closing && m_impl->request == request) {
            m_impl->request = nullptr;
            m_impl->webSocket = webSocket;
            m_impl->open.store(true, std::memory_order_release);
            accepted = true;
            closeRequest = true;
        }
    }
    if (closeRequest)
        closeHandle(request);
    if (!accepted) {
        closeHandle(webSocket);
        error = QStringLiteral("WebSocket connection was cancelled.");
        abort();
        return false;
    }
    return true;
}

bool WinHttpWebSocketConnection::sendText(const QByteArray &message, QString &error) {
    error.clear();
    if (message.size() > kMaximumMessageSize) {
        error = QStringLiteral("WebSocket message exceeds the 64 KiB signaling limit.");
        return false;
    }

    std::lock_guard sendLock(m_impl->sendMutex);
    HINTERNET webSocket = nullptr;
    {
        std::lock_guard handlesLock(m_impl->handlesMutex);
        if (!m_impl->open.load(std::memory_order_acquire) || m_impl->closing ||
            m_impl->shutdownSent || m_impl->webSocket == nullptr) {
            error = QStringLiteral("WebSocket is not connected.");
            return false;
        }
        webSocket = m_impl->webSocket;
    }
    const DWORD status = WinHttpWebSocketSend(
        webSocket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
        const_cast<char *>(message.constData()), static_cast<DWORD>(message.size()));
    if (status != NO_ERROR) {
        error = operationError(QStringLiteral("WebSocket send failed"), status);
        return false;
    }
    return true;
}

bool WinHttpWebSocketConnection::receive(WinHttpWebSocketReceiveResult &result,
                                         QString &error) {
    result = {};
    error.clear();
    QByteArray message;
    std::array<char, kReceiveBufferSize> buffer{};

    while (true) {
        HINTERNET webSocket = nullptr;
        {
            std::lock_guard lock(m_impl->handlesMutex);
            if (!m_impl->open.load(std::memory_order_acquire) || m_impl->closing ||
                m_impl->webSocket == nullptr) {
                error = QStringLiteral("WebSocket is not connected.");
                return false;
            }
            webSocket = m_impl->webSocket;
        }

        DWORD bytesRead = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType =
            WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
        const DWORD status = WinHttpWebSocketReceive(
            webSocket, buffer.data(), static_cast<DWORD>(buffer.size()),
            &bytesRead, &bufferType);
        if (status != NO_ERROR) {
            error = operationError(QStringLiteral("WebSocket receive failed"), status);
            return false;
        }

        if (bufferType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
            USHORT closeStatus = 0;
            std::array<char, 256> reason{};
            DWORD reasonLength = 0;
            const DWORD closeQueryStatus = WinHttpWebSocketQueryCloseStatus(
                webSocket, &closeStatus, reason.data(), static_cast<DWORD>(reason.size()),
                &reasonLength);
            result.kind = WinHttpWebSocketReceiveResult::Kind::Closed;
            if (closeQueryStatus == NO_ERROR) {
                result.closeStatus = closeStatus;
                result.closeReason = QString::fromUtf8(reason.data(),
                                                       static_cast<int>(reasonLength));
            }
            std::lock_guard sendLock(m_impl->sendMutex);
            bool acknowledgeClose = false;
            {
                std::lock_guard lock(m_impl->handlesMutex);
                acknowledgeClose = !m_impl->shutdownSent && !m_impl->closing &&
                                   m_impl->webSocket == webSocket;
            }
            if (acknowledgeClose) {
                const DWORD closeStatusResult = WinHttpWebSocketClose(
                    webSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                    nullptr, 0);
                if (closeStatusResult != NO_ERROR) {
                    error = operationError(QStringLiteral("WebSocket close acknowledgement failed"),
                                           closeStatusResult);
                    return false;
                }
                std::lock_guard lock(m_impl->handlesMutex);
                if (m_impl->webSocket == webSocket)
                    m_impl->open.store(false, std::memory_order_release);
            }
            return true;
        }
        if (bufferType == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE ||
            bufferType == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
            error = QStringLiteral("Binary WebSocket messages are not allowed on signaling.");
            return false;
        }

        if (bytesRead > 0)
            message.append(buffer.data(), static_cast<int>(bytesRead));
        if (message.size() > kMaximumMessageSize) {
            error = QStringLiteral("WebSocket message exceeds the 64 KiB signaling limit.");
            return false;
        }
        if (bufferType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
            result.kind = WinHttpWebSocketReceiveResult::Kind::Text;
            result.payload = std::move(message);
            return true;
        }
        if (bufferType != WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE) {
            error = QStringLiteral("WebSocket returned an unsupported frame type.");
            return false;
        }
    }
}

bool WinHttpWebSocketConnection::shutdown(QString &error) {
    error.clear();
    std::lock_guard sendLock(m_impl->sendMutex);
    HINTERNET webSocket = nullptr;
    {
        std::lock_guard handlesLock(m_impl->handlesMutex);
        if (!m_impl->open.load(std::memory_order_acquire) || m_impl->closing ||
            m_impl->webSocket == nullptr) {
            error = QStringLiteral("WebSocket is not connected.");
            return false;
        }
        if (m_impl->shutdownSent)
            return true;
        webSocket = m_impl->webSocket;
    }
    const DWORD shutdownStatus = WinHttpWebSocketShutdown(
        webSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
    if (shutdownStatus != NO_ERROR) {
        error = operationError(QStringLiteral("WebSocket shutdown failed"), shutdownStatus);
        return false;
    }
    {
        std::lock_guard handlesLock(m_impl->handlesMutex);
        if (!m_impl->closing && m_impl->webSocket == webSocket)
            m_impl->shutdownSent = true;
    }
    return true;
}

void WinHttpWebSocketConnection::abort() {
    m_impl->closeAll();
}

bool WinHttpWebSocketConnection::isOpen() const {
    return m_impl->open.load(std::memory_order_acquire);
}
