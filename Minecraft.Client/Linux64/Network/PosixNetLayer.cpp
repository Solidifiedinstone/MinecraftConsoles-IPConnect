// Code implemented by LCEMP, credit if used on other repos
// https://github.com/LCEMP/LCEMP
// Linux POSIX sockets networking layer - port of Windows64/Network/WinsockNetLayer.cpp

#include "stdafx.h"

#ifdef _LINUX64

#include "PosixNetLayer.h"
#include "../../Common/Network/PlatformNetworkManagerStub.h"
#include "../../../Minecraft.World/Socket.h"
#include "../../../Minecraft.World/DisconnectPacket.h"
#include "../../Minecraft.h"
#include "../4JLibs/inc/4J_Profile.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <time.h>

// ============================================================================
// Helper: receive exactly len bytes, returns false on disconnect/error
// ============================================================================
static bool RecvExact(int sock, uint8_t* buf, int len);

// ============================================================================
// Helper: monotonic tick count in milliseconds
// ============================================================================
static uint32_t PosixGetTickCount()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

// ============================================================================
// Static member definitions
// ============================================================================
int WinsockNetLayer::s_listenSocket = -1;
int WinsockNetLayer::s_hostConnectionSocket = -1;
pthread_t WinsockNetLayer::s_acceptThread;
pthread_t WinsockNetLayer::s_clientRecvThread;
bool WinsockNetLayer::s_acceptThreadValid = false;
bool WinsockNetLayer::s_clientRecvThreadValid = false;

bool WinsockNetLayer::s_isHost = false;
bool WinsockNetLayer::s_connected = false;
bool WinsockNetLayer::s_active = false;
bool WinsockNetLayer::s_initialized = false;

uint8_t WinsockNetLayer::s_localSmallId = 0;
uint8_t WinsockNetLayer::s_hostSmallId = 0;
unsigned int WinsockNetLayer::s_nextSmallId = 1;

pthread_mutex_t WinsockNetLayer::s_sendLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t WinsockNetLayer::s_connectionsLock = PTHREAD_MUTEX_INITIALIZER;

std::vector<Win64RemoteConnection> WinsockNetLayer::s_connections;

int WinsockNetLayer::s_advertiseSock = -1;
pthread_t WinsockNetLayer::s_advertiseThread;
volatile bool WinsockNetLayer::s_advertising = false;
bool WinsockNetLayer::s_advertiseThreadValid = false;
Win64LANBroadcast WinsockNetLayer::s_advertiseData = {};
pthread_mutex_t WinsockNetLayer::s_advertiseLock = PTHREAD_MUTEX_INITIALIZER;
int WinsockNetLayer::s_hostGamePort = WIN64_NET_DEFAULT_PORT;

int WinsockNetLayer::s_discoverySock = -1;
pthread_t WinsockNetLayer::s_discoveryThread;
volatile bool WinsockNetLayer::s_discovering = false;
bool WinsockNetLayer::s_discoveryThreadValid = false;
pthread_mutex_t WinsockNetLayer::s_discoveryLock = PTHREAD_MUTEX_INITIALIZER;
std::vector<Win64LANSession> WinsockNetLayer::s_discoveredSessions;

pthread_mutex_t WinsockNetLayer::s_disconnectLock = PTHREAD_MUTEX_INITIALIZER;
std::vector<uint8_t> WinsockNetLayer::s_disconnectedSmallIds;

pthread_mutex_t WinsockNetLayer::s_freeSmallIdLock = PTHREAD_MUTEX_INITIALIZER;
std::vector<uint8_t> WinsockNetLayer::s_freeSmallIds;
int WinsockNetLayer::s_smallIdToSocket[256];
pthread_mutex_t WinsockNetLayer::s_smallIdToSocketLock = PTHREAD_MUTEX_INITIALIZER;

// ============================================================================
// Extern globals (same as Windows version)
// ============================================================================
bool g_Win64MultiplayerHost = false;
bool g_Win64MultiplayerJoin = false;
int g_Win64MultiplayerPort = WIN64_NET_DEFAULT_PORT;
char g_Win64MultiplayerIP[256] = "127.0.0.1";
bool g_Win64DedicatedServer = false;
int g_Win64DedicatedServerPort = WIN64_NET_DEFAULT_PORT;
char g_Win64DedicatedServerBindIP[256] = "";

// ============================================================================
// Initialize
// ============================================================================
bool WinsockNetLayer::Initialize()
{
    if (s_initialized) return true;

    // No WSAStartup needed on Linux - sockets are available immediately

    pthread_mutex_init(&s_sendLock, NULL);
    pthread_mutex_init(&s_connectionsLock, NULL);
    pthread_mutex_init(&s_advertiseLock, NULL);
    pthread_mutex_init(&s_discoveryLock, NULL);
    pthread_mutex_init(&s_disconnectLock, NULL);
    pthread_mutex_init(&s_freeSmallIdLock, NULL);
    pthread_mutex_init(&s_smallIdToSocketLock, NULL);
    for (int i = 0; i < 256; i++)
        s_smallIdToSocket[i] = -1;

    s_initialized = true;

    StartDiscovery();

    return true;
}

// ============================================================================
// Shutdown
// ============================================================================
void WinsockNetLayer::Shutdown()
{
    StopAdvertising();
    StopDiscovery();

    s_active = false;
    s_connected = false;

    if (s_listenSocket != -1)
    {
        close(s_listenSocket);
        s_listenSocket = -1;
    }

    if (s_hostConnectionSocket != -1)
    {
        close(s_hostConnectionSocket);
        s_hostConnectionSocket = -1;
    }

    pthread_mutex_lock(&s_connectionsLock);
    for (size_t i = 0; i < s_connections.size(); i++)
    {
        s_connections[i].active = false;
        if (s_connections[i].tcpSocket != -1)
        {
            close(s_connections[i].tcpSocket);
        }
    }
    s_connections.clear();
    pthread_mutex_unlock(&s_connectionsLock);

    if (s_acceptThreadValid)
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        pthread_timedjoin_np(s_acceptThread, NULL, &ts);
        s_acceptThreadValid = false;
    }

    if (s_clientRecvThreadValid)
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        pthread_timedjoin_np(s_clientRecvThread, NULL, &ts);
        s_clientRecvThreadValid = false;
    }

    if (s_initialized)
    {
        pthread_mutex_destroy(&s_sendLock);
        pthread_mutex_destroy(&s_connectionsLock);
        pthread_mutex_destroy(&s_advertiseLock);
        pthread_mutex_destroy(&s_discoveryLock);
        pthread_mutex_destroy(&s_disconnectLock);
        s_disconnectedSmallIds.clear();
        pthread_mutex_destroy(&s_freeSmallIdLock);
        s_freeSmallIds.clear();
        pthread_mutex_destroy(&s_smallIdToSocketLock);
        // No WSACleanup needed on Linux
        s_initialized = false;
    }
}

// ============================================================================
// HostGame
// ============================================================================
bool WinsockNetLayer::HostGame(int port, const char* bindIp)
{
    if (!s_initialized && !Initialize()) return false;

    s_isHost = true;
    s_localSmallId = 0;
    s_hostSmallId = 0;
    s_nextSmallId = 1;
    s_hostGamePort = port;

    pthread_mutex_lock(&s_freeSmallIdLock);
    s_freeSmallIds.clear();
    pthread_mutex_unlock(&s_freeSmallIdLock);
    pthread_mutex_lock(&s_smallIdToSocketLock);
    for (int i = 0; i < 256; i++)
        s_smallIdToSocket[i] = -1;
    pthread_mutex_unlock(&s_smallIdToSocketLock);

    struct addrinfo hints = {};
    struct addrinfo* result = NULL;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = (bindIp == NULL || bindIp[0] == 0) ? AI_PASSIVE : 0;

    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%d", port);

    const char* resolvedBindIp = (bindIp != NULL && bindIp[0] != 0) ? bindIp : NULL;
    int iResult = getaddrinfo(resolvedBindIp, portStr, &hints, &result);
    if (iResult != 0)
    {
        app.DebugPrintf("getaddrinfo failed for %s:%d - %d\n",
            resolvedBindIp != NULL ? resolvedBindIp : "*",
            port,
            iResult);
        return false;
    }

    s_listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (s_listenSocket == -1)
    {
        app.DebugPrintf("socket() failed: %d\n", errno);
        freeaddrinfo(result);
        return false;
    }

    int opt = 1;
    setsockopt(s_listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    iResult = ::bind(s_listenSocket, result->ai_addr, (socklen_t)result->ai_addrlen);
    freeaddrinfo(result);
    if (iResult == -1)
    {
        app.DebugPrintf("bind() failed: %d\n", errno);
        close(s_listenSocket);
        s_listenSocket = -1;
        return false;
    }

    iResult = listen(s_listenSocket, SOMAXCONN);
    if (iResult == -1)
    {
        app.DebugPrintf("listen() failed: %d\n", errno);
        close(s_listenSocket);
        s_listenSocket = -1;
        return false;
    }

    s_active = true;
    s_connected = true;

    if (pthread_create(&s_acceptThread, NULL, AcceptThreadProc, NULL) == 0)
        s_acceptThreadValid = true;

    app.DebugPrintf("Linux LAN: Hosting on %s:%d\n",
        resolvedBindIp != NULL ? resolvedBindIp : "*",
        port);
    return true;
}

// ============================================================================
// JoinGame
// ============================================================================
bool WinsockNetLayer::JoinGame(const char* ip, int port)
{
    fprintf(stderr, "[PosixNetLayer] JoinGame called: %s:%d\n", ip, port);
    if (!s_initialized && !Initialize()) return false;

    s_isHost = false;
    s_hostSmallId = 0;
    s_connected = false;
    s_active = false;

    if (s_hostConnectionSocket != -1)
    {
        close(s_hostConnectionSocket);
        s_hostConnectionSocket = -1;
    }

    struct addrinfo hints = {};
    struct addrinfo* result = NULL;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%d", port);

    int iResult = getaddrinfo(ip, portStr, &hints, &result);
    if (iResult != 0)
    {
        app.DebugPrintf("getaddrinfo failed for %s:%d - %d\n", ip, port, iResult);
        return false;
    }

    bool connected = false;
    uint8_t assignedSmallId = 0;
    const int maxAttempts = 12;

    for (int attempt = 0; attempt < maxAttempts; ++attempt)
    {
        s_hostConnectionSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (s_hostConnectionSocket == -1)
        {
            app.DebugPrintf("socket() failed: %d\n", errno);
            break;
        }

        int noDelay = 1;
        setsockopt(s_hostConnectionSocket, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));

        iResult = connect(s_hostConnectionSocket, result->ai_addr, (socklen_t)result->ai_addrlen);
        if (iResult == -1)
        {
            int err = errno;
            app.DebugPrintf("connect() to %s:%d failed (attempt %d/%d): %d\n", ip, port, attempt + 1, maxAttempts, err);
            close(s_hostConnectionSocket);
            s_hostConnectionSocket = -1;
            usleep(200 * 1000);
            continue;
        }

        uint8_t assignBuf[1];
        int bytesRecv = recv(s_hostConnectionSocket, (char*)assignBuf, 1, 0);
        if (bytesRecv != 1)
        {
            app.DebugPrintf("Failed to receive small ID assignment from host (attempt %d/%d)\n", attempt + 1, maxAttempts);
            close(s_hostConnectionSocket);
            s_hostConnectionSocket = -1;
            usleep(200 * 1000);
            continue;
        }

        if (assignBuf[0] == WIN64_SMALLID_REJECT)
        {
            uint8_t rejectBuf[5];
            if (!RecvExact(s_hostConnectionSocket, rejectBuf, 5))
            {
                app.DebugPrintf("Failed to receive reject reason from host\n");
                close(s_hostConnectionSocket);
                s_hostConnectionSocket = -1;
                usleep(200 * 1000);
                continue;
            }
            // rejectBuf[0] = packet id (255), rejectBuf[1..4] = 4-byte big-endian reason
            int reason = ((rejectBuf[1] & 0xff) << 24) | ((rejectBuf[2] & 0xff) << 16) |
                ((rejectBuf[3] & 0xff) << 8) | (rejectBuf[4] & 0xff);
            Minecraft::GetInstance()->connectionDisconnected(ProfileManager.GetPrimaryPad(), (DisconnectPacket::eDisconnectReason)reason);
            close(s_hostConnectionSocket);
            s_hostConnectionSocket = -1;
            freeaddrinfo(result);
            return false;
        }

        assignedSmallId = assignBuf[0];
        connected = true;
        break;
    }
    freeaddrinfo(result);

    if (!connected)
    {
        return false;
    }
    s_localSmallId = assignedSmallId;

    fprintf(stderr, "[PosixNetLayer] Connected to %s:%d, assigned smallId=%d\n", ip, port, s_localSmallId);
    app.DebugPrintf("Linux LAN: Connected to %s:%d, assigned smallId=%d\n", ip, port, s_localSmallId);

    s_active = true;
    s_connected = true;

    if (pthread_create(&s_clientRecvThread, NULL, ClientRecvThreadProc, NULL) == 0)
        s_clientRecvThreadValid = true;

    return true;
}

// ============================================================================
// SendOnSocket - send length-prefixed data on a TCP socket
// ============================================================================
bool WinsockNetLayer::SendOnSocket(int sock, const void* data, int dataSize)
{
    if (sock == -1 || dataSize <= 0) return false;

    pthread_mutex_lock(&s_sendLock);

    uint8_t header[4];
    header[0] = (uint8_t)((dataSize >> 24) & 0xFF);
    header[1] = (uint8_t)((dataSize >> 16) & 0xFF);
    header[2] = (uint8_t)((dataSize >> 8) & 0xFF);
    header[3] = (uint8_t)(dataSize & 0xFF);

    int totalSent = 0;
    int toSend = 4;
    while (totalSent < toSend)
    {
        int sent = send(sock, (const char*)header + totalSent, toSend - totalSent, MSG_NOSIGNAL);
        if (sent == -1 || sent == 0)
        {
            pthread_mutex_unlock(&s_sendLock);
            return false;
        }
        totalSent += sent;
    }

    totalSent = 0;
    while (totalSent < dataSize)
    {
        int sent = send(sock, (const char*)data + totalSent, dataSize - totalSent, MSG_NOSIGNAL);
        if (sent == -1 || sent == 0)
        {
            pthread_mutex_unlock(&s_sendLock);
            return false;
        }
        totalSent += sent;
    }

    pthread_mutex_unlock(&s_sendLock);
    return true;
}

// ============================================================================
// SendToSmallId
// ============================================================================
bool WinsockNetLayer::SendToSmallId(uint8_t targetSmallId, const void* data, int dataSize)
{
    if (!s_active) return false;

    if (s_isHost)
    {
        int sock = GetSocketForSmallId(targetSmallId);
        if (sock == -1) return false;
        fprintf(stderr, "[PosixNetLayer] SendToSmallId host->%d: %d bytes\n", targetSmallId, dataSize);
        return SendOnSocket(sock, data, dataSize);
    }
    else
    {
        fprintf(stderr, "[PosixNetLayer] SendToSmallId client->host: %d bytes\n", dataSize);
        return SendOnSocket(s_hostConnectionSocket, data, dataSize);
    }
}

// ============================================================================
// GetSocketForSmallId
// ============================================================================
int WinsockNetLayer::GetSocketForSmallId(uint8_t smallId)
{
    pthread_mutex_lock(&s_smallIdToSocketLock);
    int sock = s_smallIdToSocket[smallId];
    pthread_mutex_unlock(&s_smallIdToSocketLock);
    return sock;
}

// ============================================================================
// ClearSocketForSmallId
// ============================================================================
void WinsockNetLayer::ClearSocketForSmallId(uint8_t smallId)
{
    pthread_mutex_lock(&s_smallIdToSocketLock);
    s_smallIdToSocket[smallId] = -1;
    pthread_mutex_unlock(&s_smallIdToSocketLock);
}

// ============================================================================
// SendRejectWithReason - Send reject handshake: sentinel 0xFF + DisconnectPacket
// wire format (1 byte id 255 + 4 byte big-endian reason). Then caller closes socket.
// ============================================================================
static void SendRejectWithReason(int clientSocket, DisconnectPacket::eDisconnectReason reason)
{
    uint8_t buf[6];
    buf[0] = WIN64_SMALLID_REJECT;
    buf[1] = (uint8_t)255; // DisconnectPacket packet id
    int r = (int)reason;
    buf[2] = (uint8_t)((r >> 24) & 0xff);
    buf[3] = (uint8_t)((r >> 16) & 0xff);
    buf[4] = (uint8_t)((r >> 8) & 0xff);
    buf[5] = (uint8_t)(r & 0xff);
    send(clientSocket, (const char*)buf, sizeof(buf), MSG_NOSIGNAL);
}

// ============================================================================
// RecvExact - receive exactly len bytes
// ============================================================================
static bool RecvExact(int sock, uint8_t* buf, int len)
{
    int totalRecv = 0;
    while (totalRecv < len)
    {
        int r = recv(sock, (char*)buf + totalRecv, len - totalRecv, 0);
        if (r <= 0) return false;
        totalRecv += r;
    }
    return true;
}

// ============================================================================
// HandleDataReceived
// ============================================================================
void WinsockNetLayer::HandleDataReceived(uint8_t fromSmallId, uint8_t toSmallId, unsigned char* data, unsigned int dataSize)
{
    INetworkPlayer* pPlayerFrom = g_NetworkManager.GetPlayerBySmallId(fromSmallId);
    INetworkPlayer* pPlayerTo = g_NetworkManager.GetPlayerBySmallId(toSmallId);

    if (pPlayerFrom == NULL || pPlayerTo == NULL) return;

    if (s_isHost)
    {
        ::Socket* pSocket = pPlayerFrom->GetSocket();
        if (pSocket != NULL)
            pSocket->pushDataToQueue(data, dataSize, false);
    }
    else
    {
        ::Socket* pSocket = pPlayerTo->GetSocket();
        if (pSocket != NULL)
            pSocket->pushDataToQueue(data, dataSize, true);
    }
}

// ============================================================================
// AcceptThreadProc - host: accept incoming client connections
// ============================================================================
void* WinsockNetLayer::AcceptThreadProc(void* param)
{
    (void)param;

    while (s_active)
    {
        int clientSocket = accept(s_listenSocket, NULL, NULL);
        if (clientSocket == -1)
        {
            if (s_active)
                app.DebugPrintf("accept() failed: %d\n", errno);
            break;
        }

        int noDelay = 1;
        setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));

        extern QNET_STATE _iQNetStubState;
        if (_iQNetStubState != QNET_STATE_GAME_PLAY)
        {
            app.DebugPrintf("Linux LAN: Rejecting connection, game not ready\n");
            close(clientSocket);
            continue;
        }

        extern CPlatformNetworkManagerStub* g_pPlatformNetworkManager;
        if (g_pPlatformNetworkManager != NULL && !g_pPlatformNetworkManager->CanAcceptMoreConnections())
        {
            app.DebugPrintf("Linux LAN: Rejecting connection, server at max players\n");
            SendRejectWithReason(clientSocket, DisconnectPacket::eDisconnect_ServerFull);
            close(clientSocket);
            continue;
        }

        uint8_t assignedSmallId;
        pthread_mutex_lock(&s_freeSmallIdLock);
        if (!s_freeSmallIds.empty())
        {
            assignedSmallId = s_freeSmallIds.back();
            s_freeSmallIds.pop_back();
        }
        else if (s_nextSmallId < (unsigned int)MINECRAFT_NET_MAX_PLAYERS)
        {
            assignedSmallId = (uint8_t)s_nextSmallId++;
        }
        else
        {
            pthread_mutex_unlock(&s_freeSmallIdLock);
            app.DebugPrintf("Linux LAN: Server full, rejecting connection\n");
            SendRejectWithReason(clientSocket, DisconnectPacket::eDisconnect_ServerFull);
            close(clientSocket);
            continue;
        }
        pthread_mutex_unlock(&s_freeSmallIdLock);

        uint8_t assignBuf[1] = { assignedSmallId };
        int sent = send(clientSocket, (const char*)assignBuf, 1, MSG_NOSIGNAL);
        if (sent != 1)
        {
            app.DebugPrintf("Failed to send small ID to client\n");
            close(clientSocket);
            continue;
        }

        Win64RemoteConnection conn;
        conn.tcpSocket = clientSocket;
        conn.smallId = assignedSmallId;
        conn.active = true;
        memset(&conn.recvThread, 0, sizeof(conn.recvThread));

        pthread_mutex_lock(&s_connectionsLock);
        s_connections.push_back(conn);
        int connIdx = (int)s_connections.size() - 1;
        pthread_mutex_unlock(&s_connectionsLock);

        app.DebugPrintf("Linux LAN: Client connected, assigned smallId=%d\n", assignedSmallId);

        pthread_mutex_lock(&s_smallIdToSocketLock);
        s_smallIdToSocket[assignedSmallId] = clientSocket;
        pthread_mutex_unlock(&s_smallIdToSocketLock);

        IQNetPlayer* qnetPlayer = &IQNet::m_player[assignedSmallId];

        extern void Win64_SetupRemoteQNetPlayer(IQNetPlayer* player, BYTE smallId, bool isHost, bool isLocal);
        Win64_SetupRemoteQNetPlayer(qnetPlayer, assignedSmallId, false, false);

        extern CPlatformNetworkManagerStub* g_pPlatformNetworkManager;
        g_pPlatformNetworkManager->NotifyPlayerJoined(qnetPlayer);

        int* threadParam = new int;
        *threadParam = connIdx;
        pthread_t hThread;
        if (pthread_create(&hThread, NULL, RecvThreadProc, threadParam) == 0)
        {
            pthread_detach(hThread);
            pthread_mutex_lock(&s_connectionsLock);
            if (connIdx < (int)s_connections.size())
                s_connections[connIdx].recvThread = hThread;
            pthread_mutex_unlock(&s_connectionsLock);
        }
        else
        {
            delete threadParam;
        }
    }
    return NULL;
}

// ============================================================================
// RecvThreadProc - host: receive data from a specific client
// ============================================================================
void* WinsockNetLayer::RecvThreadProc(void* param)
{
    int connIdx = *(int*)param;
    delete (int*)param;

    pthread_mutex_lock(&s_connectionsLock);
    if (connIdx >= (int)s_connections.size())
    {
        pthread_mutex_unlock(&s_connectionsLock);
        return NULL;
    }
    int sock = s_connections[connIdx].tcpSocket;
    uint8_t clientSmallId = s_connections[connIdx].smallId;
    pthread_mutex_unlock(&s_connectionsLock);

    std::vector<uint8_t> recvBuf;
    recvBuf.resize(WIN64_NET_RECV_BUFFER_SIZE);

    while (s_active)
    {
        uint8_t header[4];
        if (!RecvExact(sock, header, 4))
        {
            app.DebugPrintf("Linux LAN: Client smallId=%d disconnected (header)\n", clientSmallId);
            break;
        }

        int packetSize =
            ((uint32_t)header[0] << 24) |
            ((uint32_t)header[1] << 16) |
            ((uint32_t)header[2] << 8) |
            ((uint32_t)header[3]);

        if (packetSize <= 0 || packetSize > WIN64_NET_MAX_PACKET_SIZE)
        {
            app.DebugPrintf("Linux LAN: Invalid packet size %d from client smallId=%d (max=%d)\n",
                packetSize,
                clientSmallId,
                (int)WIN64_NET_MAX_PACKET_SIZE);
            break;
        }

        if ((int)recvBuf.size() < packetSize)
        {
            recvBuf.resize(packetSize);
            app.DebugPrintf("Linux LAN: Resized host recv buffer to %d bytes for client smallId=%d\n", packetSize, clientSmallId);
        }

        if (!RecvExact(sock, &recvBuf[0], packetSize))
        {
            app.DebugPrintf("Linux LAN: Client smallId=%d disconnected (body)\n", clientSmallId);
            break;
        }

        HandleDataReceived(clientSmallId, s_hostSmallId, &recvBuf[0], packetSize);
    }

    pthread_mutex_lock(&s_connectionsLock);
    for (size_t i = 0; i < s_connections.size(); i++)
    {
        if (s_connections[i].smallId == clientSmallId)
        {
            s_connections[i].active = false;
            if (s_connections[i].tcpSocket != -1)
            {
                close(s_connections[i].tcpSocket);
                s_connections[i].tcpSocket = -1;
            }
            break;
        }
    }
    pthread_mutex_unlock(&s_connectionsLock);

    pthread_mutex_lock(&s_disconnectLock);
    s_disconnectedSmallIds.push_back(clientSmallId);
    pthread_mutex_unlock(&s_disconnectLock);

    return NULL;
}

// ============================================================================
// PopDisconnectedSmallId
// ============================================================================
bool WinsockNetLayer::PopDisconnectedSmallId(uint8_t* outSmallId)
{
    bool found = false;
    pthread_mutex_lock(&s_disconnectLock);
    if (!s_disconnectedSmallIds.empty())
    {
        *outSmallId = s_disconnectedSmallIds.back();
        s_disconnectedSmallIds.pop_back();
        found = true;
    }
    pthread_mutex_unlock(&s_disconnectLock);
    return found;
}

// ============================================================================
// PushFreeSmallId
// ============================================================================
void WinsockNetLayer::PushFreeSmallId(uint8_t smallId)
{
    pthread_mutex_lock(&s_freeSmallIdLock);
    s_freeSmallIds.push_back(smallId);
    pthread_mutex_unlock(&s_freeSmallIdLock);
}

// ============================================================================
// CloseConnectionBySmallId
// ============================================================================
void WinsockNetLayer::CloseConnectionBySmallId(uint8_t smallId)
{
    pthread_mutex_lock(&s_connectionsLock);
    for (size_t i = 0; i < s_connections.size(); i++)
    {
        if (s_connections[i].smallId == smallId && s_connections[i].active && s_connections[i].tcpSocket != -1)
        {
            close(s_connections[i].tcpSocket);
            s_connections[i].tcpSocket = -1;
            app.DebugPrintf("Linux LAN: Force-closed TCP connection for smallId=%d\n", smallId);
            break;
        }
    }
    pthread_mutex_unlock(&s_connectionsLock);
}

// ============================================================================
// ClientRecvThreadProc - client: receive data from host
// ============================================================================
void* WinsockNetLayer::ClientRecvThreadProc(void* param)
{
    (void)param;

    std::vector<uint8_t> recvBuf;
    recvBuf.resize(WIN64_NET_RECV_BUFFER_SIZE);

    while (s_active && s_hostConnectionSocket != -1)
    {
        uint8_t header[4];
        if (!RecvExact(s_hostConnectionSocket, header, 4))
        {
            app.DebugPrintf("Linux LAN: Disconnected from host (header)\n");
            break;
        }

        int packetSize = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];

        if (packetSize <= 0 || packetSize > WIN64_NET_MAX_PACKET_SIZE)
        {
            app.DebugPrintf("Linux LAN: Invalid packet size %d from host (max=%d)\n",
                packetSize,
                (int)WIN64_NET_MAX_PACKET_SIZE);
            break;
        }

        if ((int)recvBuf.size() < packetSize)
        {
            recvBuf.resize(packetSize);
            app.DebugPrintf("Linux LAN: Resized client recv buffer to %d bytes\n", packetSize);
        }

        if (!RecvExact(s_hostConnectionSocket, &recvBuf[0], packetSize))
        {
            app.DebugPrintf("Linux LAN: Disconnected from host (body)\n");
            break;
        }

        HandleDataReceived(s_hostSmallId, s_localSmallId, &recvBuf[0], packetSize);
    }

    s_connected = false;
    return NULL;
}

// ============================================================================
// StartAdvertising
// ============================================================================
bool WinsockNetLayer::StartAdvertising(int gamePort, const wchar_t* hostName, unsigned int gameSettings, unsigned int texPackId, unsigned char subTexId, unsigned short netVer)
{
    if (s_advertising) return true;
    if (!s_initialized) return false;

    pthread_mutex_lock(&s_advertiseLock);
    memset(&s_advertiseData, 0, sizeof(s_advertiseData));
    s_advertiseData.magic = WIN64_LAN_BROADCAST_MAGIC;
    s_advertiseData.netVersion = netVer;
    s_advertiseData.gamePort = (uint16_t)gamePort;
    wcsncpy(s_advertiseData.hostName, hostName, 31);
    s_advertiseData.hostName[31] = L'/0';
    s_advertiseData.playerCount = 1;
    s_advertiseData.maxPlayers = MINECRAFT_NET_MAX_PLAYERS;
    s_advertiseData.gameHostSettings = gameSettings;
    s_advertiseData.texturePackParentId = texPackId;
    s_advertiseData.subTexturePackId = subTexId;
    s_advertiseData.isJoinable = 0;
    s_hostGamePort = gamePort;
    pthread_mutex_unlock(&s_advertiseLock);

    s_advertiseSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_advertiseSock == -1)
    {
        app.DebugPrintf("Linux LAN: Failed to create advertise socket: %d\n", errno);
        return false;
    }

    int broadcast = 1;
    setsockopt(s_advertiseSock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    s_advertising = true;
    if (pthread_create(&s_advertiseThread, NULL, AdvertiseThreadProc, NULL) == 0)
        s_advertiseThreadValid = true;

    app.DebugPrintf("Linux LAN: Started advertising on UDP port %d\n", WIN64_LAN_DISCOVERY_PORT);
    return true;
}

// ============================================================================
// StopAdvertising
// ============================================================================
void WinsockNetLayer::StopAdvertising()
{
    s_advertising = false;

    if (s_advertiseSock != -1)
    {
        close(s_advertiseSock);
        s_advertiseSock = -1;
    }

    if (s_advertiseThreadValid)
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        pthread_timedjoin_np(s_advertiseThread, NULL, &ts);
        s_advertiseThreadValid = false;
    }
}

// ============================================================================
// UpdateAdvertisePlayerCount
// ============================================================================
void WinsockNetLayer::UpdateAdvertisePlayerCount(uint8_t count)
{
    pthread_mutex_lock(&s_advertiseLock);
    s_advertiseData.playerCount = count;
    pthread_mutex_unlock(&s_advertiseLock);
}

// ============================================================================
// UpdateAdvertiseMaxPlayers
// ============================================================================
void WinsockNetLayer::UpdateAdvertiseMaxPlayers(uint8_t maxPlayers)
{
    pthread_mutex_lock(&s_advertiseLock);
    s_advertiseData.maxPlayers = maxPlayers;
    pthread_mutex_unlock(&s_advertiseLock);
}

// ============================================================================
// UpdateAdvertiseJoinable
// ============================================================================
void WinsockNetLayer::UpdateAdvertiseJoinable(bool joinable)
{
    pthread_mutex_lock(&s_advertiseLock);
    s_advertiseData.isJoinable = joinable ? 1 : 0;
    pthread_mutex_unlock(&s_advertiseLock);
}

// ============================================================================
// AdvertiseThreadProc - broadcast LAN game presence
// ============================================================================
void* WinsockNetLayer::AdvertiseThreadProc(void* param)
{
    (void)param;

    struct sockaddr_in broadcastAddr;
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(WIN64_LAN_DISCOVERY_PORT);
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;

    while (s_advertising)
    {
        pthread_mutex_lock(&s_advertiseLock);
        Win64LANBroadcast data = s_advertiseData;
        pthread_mutex_unlock(&s_advertiseLock);

        int sent = sendto(s_advertiseSock, (const char*)&data, sizeof(data), 0,
            (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));

        if (sent == -1 && s_advertising)
        {
            app.DebugPrintf("Linux LAN: Broadcast sendto failed: %d\n", errno);
        }

        usleep(1000 * 1000); // 1 second
    }

    return NULL;
}

// ============================================================================
// StartDiscovery
// ============================================================================
bool WinsockNetLayer::StartDiscovery()
{
    if (s_discovering) return true;
    if (!s_initialized) return false;

    s_discoverySock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_discoverySock == -1)
    {
        app.DebugPrintf("Linux LAN: Failed to create discovery socket: %d\n", errno);
        return false;
    }

    int reuseAddr = 1;
    setsockopt(s_discoverySock, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));

    struct sockaddr_in bindAddr;
    memset(&bindAddr, 0, sizeof(bindAddr));
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(WIN64_LAN_DISCOVERY_PORT);
    bindAddr.sin_addr.s_addr = INADDR_ANY;

    if (::bind(s_discoverySock, (struct sockaddr*)&bindAddr, sizeof(bindAddr)) == -1)
    {
        app.DebugPrintf("Linux LAN: Discovery bind failed: %d\n", errno);
        close(s_discoverySock);
        s_discoverySock = -1;
        return false;
    }

    // Set receive timeout to 500ms using timeval (POSIX)
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500 * 1000;
    setsockopt(s_discoverySock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    s_discovering = true;
    if (pthread_create(&s_discoveryThread, NULL, DiscoveryThreadProc, NULL) == 0)
        s_discoveryThreadValid = true;

    app.DebugPrintf("Linux LAN: Listening for LAN games on UDP port %d\n", WIN64_LAN_DISCOVERY_PORT);
    return true;
}

// ============================================================================
// StopDiscovery
// ============================================================================
void WinsockNetLayer::StopDiscovery()
{
    s_discovering = false;

    if (s_discoverySock != -1)
    {
        close(s_discoverySock);
        s_discoverySock = -1;
    }

    if (s_discoveryThreadValid)
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        pthread_timedjoin_np(s_discoveryThread, NULL, &ts);
        s_discoveryThreadValid = false;
    }

    pthread_mutex_lock(&s_discoveryLock);
    s_discoveredSessions.clear();
    pthread_mutex_unlock(&s_discoveryLock);
}

// ============================================================================
// GetDiscoveredSessions
// ============================================================================
std::vector<Win64LANSession> WinsockNetLayer::GetDiscoveredSessions()
{
    std::vector<Win64LANSession> result;
    pthread_mutex_lock(&s_discoveryLock);
    result = s_discoveredSessions;
    pthread_mutex_unlock(&s_discoveryLock);
    return result;
}

// ============================================================================
// DiscoveryThreadProc - listen for LAN broadcast packets
// ============================================================================
void* WinsockNetLayer::DiscoveryThreadProc(void* param)
{
    (void)param;

    char recvBuf[512];

    while (s_discovering)
    {
        struct sockaddr_in senderAddr;
        socklen_t senderLen = sizeof(senderAddr);

        int recvLen = recvfrom(s_discoverySock, recvBuf, sizeof(recvBuf), 0,
            (struct sockaddr*)&senderAddr, &senderLen);

        if (recvLen == -1)
        {
            // Timeout or transient error - just continue
            continue;
        }

        if (recvLen < (int)sizeof(Win64LANBroadcast))
            continue;

        Win64LANBroadcast* broadcast = (Win64LANBroadcast*)recvBuf;
        if (broadcast->magic != WIN64_LAN_BROADCAST_MAGIC)
            continue;

        char senderIP[64];
        inet_ntop(AF_INET, &senderAddr.sin_addr, senderIP, sizeof(senderIP));

        uint32_t now = PosixGetTickCount();

        pthread_mutex_lock(&s_discoveryLock);

        bool found = false;
        for (size_t i = 0; i < s_discoveredSessions.size(); i++)
        {
            if (strcmp(s_discoveredSessions[i].hostIP, senderIP) == 0 &&
                s_discoveredSessions[i].hostPort == (int)broadcast->gamePort)
            {
                s_discoveredSessions[i].netVersion = broadcast->netVersion;
                wcsncpy(s_discoveredSessions[i].hostName, broadcast->hostName, 31);
                s_discoveredSessions[i].hostName[31] = L'/0';
                s_discoveredSessions[i].playerCount = broadcast->playerCount;
                s_discoveredSessions[i].maxPlayers = broadcast->maxPlayers;
                s_discoveredSessions[i].gameHostSettings = broadcast->gameHostSettings;
                s_discoveredSessions[i].texturePackParentId = broadcast->texturePackParentId;
                s_discoveredSessions[i].subTexturePackId = broadcast->subTexturePackId;
                s_discoveredSessions[i].isJoinable = (broadcast->isJoinable != 0);
                s_discoveredSessions[i].lastSeenTick = now;
                found = true;
                break;
            }
        }

        if (!found)
        {
            Win64LANSession session;
            memset(&session, 0, sizeof(session));
            strncpy(session.hostIP, senderIP, sizeof(session.hostIP) - 1);
            session.hostIP[sizeof(session.hostIP) - 1] = '/0';
            session.hostPort = (int)broadcast->gamePort;
            session.netVersion = broadcast->netVersion;
            wcsncpy(session.hostName, broadcast->hostName, 31);
            session.hostName[31] = L'/0';
            session.playerCount = broadcast->playerCount;
            session.maxPlayers = broadcast->maxPlayers;
            session.gameHostSettings = broadcast->gameHostSettings;
            session.texturePackParentId = broadcast->texturePackParentId;
            session.subTexturePackId = broadcast->subTexturePackId;
            session.isJoinable = (broadcast->isJoinable != 0);
            session.lastSeenTick = now;
            s_discoveredSessions.push_back(session);

            app.DebugPrintf("Linux LAN: Discovered game \"%ls\" at %s:%d\n",
                session.hostName, session.hostIP, session.hostPort);
        }

        // Expire sessions not seen in 5 seconds
        for (size_t i = s_discoveredSessions.size(); i > 0; i--)
        {
            if (now - s_discoveredSessions[i - 1].lastSeenTick > 5000)
            {
                app.DebugPrintf("Linux LAN: Session \"%ls\" at %s timed out\n",
                    s_discoveredSessions[i - 1].hostName, s_discoveredSessions[i - 1].hostIP);
                s_discoveredSessions.erase(s_discoveredSessions.begin() + (i - 1));
            }
        }

        pthread_mutex_unlock(&s_discoveryLock);
    }

    return NULL;
}

#endif
