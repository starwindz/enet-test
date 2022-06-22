// enet-test-starwindz-v2.1
#define QUICK_TEST

// include
#include <stdio.h>
#include <conio.h>
#include <thread>
#include <atomic>
#include "enet\enet.h"

// set lib
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Winmm.lib")

// connection state
#define cs_Waiting    0
#define cs_Connected  1
#define cs_Error      2

// packet command
#define pc_None  0
#define pc_Punch 1
#define pc_Ping  2
#define pc_Pong  30

#define MAX_PACKET_BUFFER_SIZE 256

__pragma(pack(push, 1))
struct MPGameSetting {
    byte pc;
    char teamName[64];
    byte tactics;
    byte gameStyle;
    byte pitchType;
    byte status;    // 0:waiting, 1:ready
    byte cmd;
};
__pragma(pack(pop))

class udp_p2p {
public:
    int mode; // 0:local-remote, 1:remote-local
    char remote_ip[40] = { 0, };
    int remote_port, local_port;

    char sent_buffer[MAX_PACKET_BUFFER_SIZE];
    char received_buffer[MAX_PACKET_BUFFER_SIZE];
    int buffer_size;

    ENetAddress local_address, remote_address;
    ENetHost* local_host;
    ENetPeer* remote_peer = nullptr;
    ENetPeer* connected_peer = nullptr;
    ENetEvent event;
    int state = cs_Waiting;
    bool send_back_ping_when_connected;
    bool loop = true;
    int loop_count = 0, punch_count = 0;
    int pong_count = 0;

public:
    int setup(int _local_port, const char* _remote_ip, int _remote_port, int _buffer_size, bool _send_back_ping_when_connected);
    int init();
    int close();

    int send_buffer();
    int receive_buffer();
    int disconnect();
};

udp_p2p g_udp_p2p;

int udp_p2p::setup(int _local_port, const char* _remote_ip, int _remote_port, int _buffer_size, bool _send_back_ping_when_connected)
{
    local_port = _local_port;
    strcpy_s(remote_ip, _remote_ip);
    remote_port = _remote_port;
    buffer_size = _buffer_size;
    send_back_ping_when_connected = _send_back_ping_when_connected;

    return 0;
}

int udp_p2p::init()
{
    // init
    // -- enet
    if (enet_initialize() != 0) {
        printf("an error occurred while initializing ENet.\n");
        return 1;
    }

    // -- local create => local
    // Only allow incoming connections from the remote ip  
    enet_address_set_host_ip(&local_address, remote_ip);
    local_address.port = local_port;
    local_host = enet_host_create(&local_address, 2, 1, 0, 0);
    if (local_host == NULL) {
        printf("an error occurred while trying to create an ENet local host\n");
        return 1;
    }
    printf("created local on port %d\n", local_port);

    // -- remote create => remote_peer
    enet_address_set_host_ip(&remote_address, remote_ip);
    remote_address.port = remote_port;
    remote_peer = enet_host_connect(local_host, &remote_address, 2, 0);
    printf("try to connect from local to remote on port %d\n", remote_port);

    // -- other init
    state = cs_Waiting;
    loop = true;
    loop_count = 0;
    punch_count = 0;
    pong_count = 0;

    printf("init done\n");

    return 0;
}

int udp_p2p::close()
{
    // close
    enet_peer_reset(remote_peer);
    enet_peer_disconnect(remote_peer, 0);
    enet_host_destroy(local_host);
    enet_deinitialize();

    return 0;
}

int udp_p2p::receive_buffer()
{
    Sleep(1);
    loop_count++;

    while (enet_host_service(local_host, &event, 0) > 0) {
        char ip[40];
        enet_address_get_host_ip(&event.peer->address, ip, 40);

        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            if (!connected_peer) {
                if (remote_peer == event.peer) {
                    printf("we connected to %s:%u\n",
                        ip, event.peer->address.port);
                }
                else {
                    printf("we accepted a connection from %s:%u\n",
                        ip, event.peer->address.port);
                }
                connected_peer = event.peer;

                if (send_back_ping_when_connected) {
                    send_buffer();
                    printf("sent send_back_ping when connected\n");
                }
            }
            break;
        }
        case ENET_EVENT_TYPE_RECEIVE: {
            // -- receive ping packet
            memcpy(received_buffer, (char*)(event.packet->data), buffer_size);
            auto pc = received_buffer[0];
            if (pc == pc_Ping) {
                printf("received ping from %s:%d\n", ip, (int)event.peer->address.port);

                // -- send pong packet
                sent_buffer[0] = pc_Pong;
                ENetPacket* packet_pong = enet_packet_create(sent_buffer, buffer_size, ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(event.peer, 0, packet_pong);
                enet_packet_destroy(event.packet);
                printf("sent pong\n");
            }
            else if (pc == pc_Pong) {
                printf("received pong number %d from %s:%d\n", ++pong_count, ip, (int)event.peer->address.port);
            }
            else if (pc == pc_Punch) {
                // unexpected, punch messages should happen before the connection,
                // in order to help establish it              
                printf("received punch from %s:%d\n", ip, (int)event.peer->address.port);
            }
            enet_packet_destroy(event.packet);
            break;
        }
        case ENET_EVENT_TYPE_DISCONNECT: {
            if (connected_peer == event.peer) {
                printf("established connection lost\n");
                loop = false;
                connected_peer = nullptr;
            }
            event.peer->data = NULL;
            break;
        }
        default: {
            printf("received event type %d\n", event.type);
        }
        }
    }

    // until we are connected, send a packet approx 10 times a second
    // in order to try and punch a hole through the NAT
    if (!connected_peer && (loop_count % 100 == 0)) {
        sent_buffer[0] = pc_Punch;
        ENetPacket* packet_punch = enet_packet_create(sent_buffer, buffer_size, 0);
        enet_peer_send(remote_peer, 0, packet_punch);
        printf("send punch %d to remote peer\n", ++punch_count);
    }

    return event.type;
}

int udp_p2p::send_buffer()
{
    if (connected_peer) {
        sent_buffer[0] = pc_Ping;
        ENetPacket* packet_ping = enet_packet_create(sent_buffer, buffer_size, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(remote_peer, 0, packet_ping);
        printf("sent ping\n");
    }

    return 0;
}

int udp_p2p::disconnect()
{
    enet_peer_disconnect(connected_peer, 0);

    return 0;
}

int main(int argc, char** argv)
{
    // general setting
#if defined(QUICK_TEST)
    if (argc != 2) {
        printf("invalid parameter\n");
        printf("usage: udt-test [option]\n");
        printf("option: 0(local-remote), 1(remote-local)\n");
        return 0;
    }
#else
    if (argc != 4) {
        printf("invalid parameters\n");
        printf("usage: udt-test [local_port] [remote_ip] [remote_port]\n");
        return 0;
    }
#endif

    // set up ip address, port and buffer size
#if defined(QUICK_TEST)
    if (strcmp(argv[1], "0") == 0) {      // local-remote mode
        g_udp_p2p.setup(27885, "127.0.0.1", 27886, sizeof(MPGameSetting), true);
    }
    else if (strcmp(argv[1], "1") == 0) { // remote-local mode
        g_udp_p2p.setup(27886, "127.0.0.1", 27885, sizeof(MPGameSetting), true);
    }
    else {
        return 0;
    }
#else 
    g_udp_p2p.setup(atoi(argv[1]), argv[2], atoi(argv[3]), sizeof(MPGameSetting), true);
#endif

    // init reliable udp
    g_udp_p2p.init();

    // menu loop (onDraw loop of swos menu)
    while (g_udp_p2p.loop) {
        if (g_udp_p2p.receive_buffer() == ENET_EVENT_TYPE_DISCONNECT) {
            // exit menu loop
            break;
        }

        if (_kbhit()) {
            auto c = _getch();
            if (c == '1') {
                g_udp_p2p.send_buffer();
            }
            else if (c == 'q') {
                g_udp_p2p.disconnect();
                printf("q pressed, exit app\n");
            }
        }
    }

    // close reliable udp
    g_udp_p2p.close();
}