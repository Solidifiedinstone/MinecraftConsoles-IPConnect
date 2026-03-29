// Code implemented by LCEMP, credit if used on other repos
// https://github.com/LCEMP/LCEMP
// Linux POSIX sockets networking layer - port of Windows64/Network/WinsockNetLayer.h
#pragma once

#ifdef _LINUX64

#include <vector>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../../Common/Network/NetworkPlayerInterface.h"

#define WIN64_NET_DEFAULT_PORT 25565
#define WIN64_NET_MAX_CLIENTS 255
#define WIN64_SMALLID_REJECT 0xFF
#define WIN64_NET_RECV_BUFFER_SIZE 65536
#define WIN64_NET_MAX_PACKET_SIZE (4 * 1024 * 1024)
#define WIN64_LAN_DISCOVERY_PORT 25566
#define WIN64_LAN_BROADCAST_MAGIC 0x4D434C4E

class Socket;

#pragma pack(push, 1)
struct Win64LANBroadcast
{
    uint32_t magic;
    uint16_t netVersion;
    uint16_t gamePort;
    wchar_t hostName[32];
    uint8_t playerCount;
    uint8_t maxPlayers;
    uint32_t gameHostSettings;
    uint32_t texturePackParentId;
    uint8_t subTexturePackId;
    uint8_t isJoinable;
};
#pragma pack(pop)

struct Win64LANSession
{
    char hostIP[64];
    int hostPort;
    wchar_t hostName[32];
    unsigned short netVersion;
    unsigned char playerCount;
    unsigned char maxPlayers;
    unsigned int gameHostSettings;
    unsigned int texturePackParentId;
    unsigned char subTexturePackId;
    bool isJoinable;
    uint32_t lastSeenTick;
};

struct Win64RemoteConnection
{
    int tcpSocket;
    uint8_t smallId;
    pthread_t recvThread;
    volatile bool active;
};

class WinsockNetLayer
{
public:
    static bool Initialize();
    static void Shutdown();

    static bool HostGame(int port, const char* bindIp = NULL);
    static bool JoinGame(const char* ip, int port);

    static bool SendToSmallId(uint8_t targetSmallId, const void* data, int dataSize);
    static bool SendOnSocket(int sock, const void* data, int dataSize);

    static bool IsHosting() { return s_isHost; }
    static bool IsConnected() { return s_connected; }
    static bool IsActive() { return s_active; }

    static uint8_t GetLocalSmallId() { return s_localSmallId; }
    static uint8_t GetHostSmallId() { return s_hostSmallId; }

    static int GetSocketForSmallId(uint8_t smallId);

    static void HandleDataReceived(uint8_t fromSmallId, uint8_t toSmallId, unsigned char* data, unsigned int dataSize);

    static bool PopDisconnectedSmallId(uint8_t* outSmallId);
    static void PushFreeSmallId(uint8_t smallId);
    static void CloseConnectionBySmallId(uint8_t smallId);

    static bool StartAdvertising(int gamePort, const wchar_t* hostName, unsigned int gameSettings, unsigned int texPackId, unsigned char subTexId, unsigned short netVer);
    static void StopAdvertising();
    static void UpdateAdvertisePlayerCount(uint8_t count);
    static void UpdateAdvertiseMaxPlayers(uint8_t maxPlayers);
    static void UpdateAdvertiseJoinable(bool joinable);

    static bool StartDiscovery();
    static void StopDiscovery();
    static std::vector<Win64LANSession> GetDiscoveredSessions();

    static int GetHostPort() { return s_hostGamePort; }

    static void ClearSocketForSmallId(uint8_t smallId);

private:
    static void* AcceptThreadProc(void* param);
    static void* RecvThreadProc(void* param);
    static void* ClientRecvThreadProc(void* param);
    static void* AdvertiseThreadProc(void* param);
    static void* DiscoveryThreadProc(void* param);

    static int s_listenSocket;
    static int s_hostConnectionSocket;
    static pthread_t s_acceptThread;
    static pthread_t s_clientRecvThread;
    static bool s_acceptThreadValid;
    static bool s_clientRecvThreadValid;

    static bool s_isHost;
    static bool s_connected;
    static volatile bool s_active;
    static bool s_initialized;

    static uint8_t s_localSmallId;
    static uint8_t s_hostSmallId;
    static unsigned int s_nextSmallId;

    static pthread_mutex_t s_sendLock;
    static pthread_mutex_t s_connectionsLock;

    static std::vector<Win64RemoteConnection> s_connections;

    static int s_advertiseSock;
    static pthread_t s_advertiseThread;
    static volatile bool s_advertising;
    static bool s_advertiseThreadValid;
    static Win64LANBroadcast s_advertiseData;
    static pthread_mutex_t s_advertiseLock;
    static int s_hostGamePort;

    static int s_discoverySock;
    static pthread_t s_discoveryThread;
    static volatile bool s_discovering;
    static bool s_discoveryThreadValid;
    static pthread_mutex_t s_discoveryLock;
    static std::vector<Win64LANSession> s_discoveredSessions;

    static pthread_mutex_t s_disconnectLock;
    static std::vector<uint8_t> s_disconnectedSmallIds;

    static pthread_mutex_t s_freeSmallIdLock;
    static std::vector<uint8_t> s_freeSmallIds;
    // O(1) smallId -> socket lookup so we don't scan s_connections (which never shrinks) on every send
    static int s_smallIdToSocket[256];
    static pthread_mutex_t s_smallIdToSocketLock;
};

extern bool g_Win64MultiplayerHost;
extern bool g_Win64MultiplayerJoin;
extern int g_Win64MultiplayerPort;
extern char g_Win64MultiplayerIP[256];
extern bool g_Win64DedicatedServer;
extern int g_Win64DedicatedServerPort;
extern char g_Win64DedicatedServerBindIP[256];

#endif
