// enet-test-new-update-v2
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
struct PeerPacket {
    char pc;
    char teamName[64];
    byte tactics;
    byte gameStyle;
    byte pitchType;
    byte status;    // 0:waiting, 1:ready
    byte sendMeYourSetting;
};
__pragma(pack(pop))

class udp_p2p {
public:
    int mode; // 0:local-remote, 1:remote-local
    char remote_ip[32] = { 0, };
    int remote_port, local_port;

    char sent_buffer[MAX_PACKET_BUFFER_SIZE];
    char received_buffer[MAX_PACKET_BUFFER_SIZE];
    int buffer_size;

    ENetAddress localAddress, remoteAddress;
    ENetHost* local;
    ENetPeer* remotePeer;
    ENetEvent event;
    int state = cs_Waiting;
    bool loop = true;
    int pongs = 0;

public:
    int init();
    int close();

    int send_buffer();
    int receive_buffer();

    void connecting();
};

udp_p2p g_udp_p2p;

int udp_p2p::init()
{
    // init
    // -- enet
    if (enet_initialize() != 0) {
        printf("an error occurred while initializing ENet.\n");
        return 1;
    }

    // -- vars
    state = cs_Waiting;
    loop = true;
    pongs = 0;

    // -- local create => local
    localAddress.host = ENET_HOST_ANY;
    localAddress.port = local_port;
    local = enet_host_create(&localAddress, 32, 2, 0, 0);
    if (local == NULL) {
        printf("an error occurred while trying to create an ENet local.\n");
        return 1;
    }
    printf("created local on port %d\n", local_port);

    // -- remote create => remotePeer
    enet_address_set_host_ip(&remoteAddress, remote_ip);
    remoteAddress.port = remote_port;
    remotePeer = enet_host_connect(local, &remoteAddress, 2, 0);
    printf("try to connect from local to remote on port %d\n", remote_port);

    // -- misc
    buffer_size = sizeof(PeerPacket);

    return 0;
}

int udp_p2p::close()
{
    // close
    enet_peer_reset(remotePeer);
    enet_peer_disconnect(remotePeer, 0);
    enet_host_destroy(local);
    enet_deinitialize();

    return 0;
}

int udp_p2p::send_buffer()
{
    if (state == cs_Connected) {
        sent_buffer[0] = pc_Ping;
        ENetPacket* packetPing = enet_packet_create(sent_buffer, buffer_size, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(remotePeer, 0, packetPing);
        printf("sent ping\n");
    }

    return 0;
}

int udp_p2p::receive_buffer()
{
    // receive packet: begin
    Sleep(1);
    while (enet_host_service(local, &event, 10) > 0) {
        char ip[40];        
        enet_address_get_host_ip(&event.peer->address, ip, 40);
		
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                state = cs_Connected;
                printf("a new client connected from %x:%u.\n",
                    event.peer->address.host,
                    event.peer->address.port);
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
                    ENetPacket* packetPong = enet_packet_create(sent_buffer, buffer_size, ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, packetPong);
                    enet_packet_destroy(event.packet);
                    printf("sent pong\n");
                }
                else if (pc == pc_Pong) {
                    printf("received pong number %d from %s:%d\n", ++pongs, ip, (int)event.peer->address.port);
                    enet_packet_destroy(event.packet);
                }
                else if (pc == pc_Punch) {
                    printf("received punch from %s:%d\n", ip, (int)event.peer->address.port);
                    enet_packet_destroy(event.packet);
                }
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                printf("client %s:%u.disconncted\n", ip, event.peer->address.port);
                event.peer->data = NULL;
                loop = false;
                break;
            }
            default: {
                printf("received event type %d\n", event.type);
            }
        }
    }

    connecting();

    return event.type;
}

void udp_p2p::connecting()
{
    if (state == cs_Waiting) {
        Sleep(16);
        sent_buffer[0] = pc_Punch;
        ENetPacket* packetPing = enet_packet_create(sent_buffer, buffer_size, 0);
        enet_peer_send(remotePeer, 0, packetPing);
    }
}

int main(int argc, char** argv)
{
    // general setting
#if defined(QUICK_TEST)
    if (argc != 2) {
        printf("invalid parameter\n");
        printf("usage: udt-test [option]\n");
        printf("option: 0(server), 1(client)\n");
        return 0;
    }
#else
    if (argc != 4) {
        printf("invalid parameter\n");
        printf("usage: udt-test [localPort] [remoteIP] [remotePort]\n");
        return 0;
    }
#endif 

    // set ip address and port
#if defined(QUICK_TEST)
    int prog_mode;
    if (strcmp(argv[1], "0") == 0)
        prog_mode = 0;  // local-remote mode
    else if (strcmp(argv[1], "1") == 0)
        prog_mode = 1;  // remote-local mode
    else
        return 0;
    if (prog_mode == 0) {
        g_udp_p2p.local_port = 27885;
        g_udp_p2p.remote_port = 27886;
    }
    else {
        g_udp_p2p.local_port = 27886;
        g_udp_p2p.remote_port = 27885;
    }
	strcpy_s(g_udp_p2p.remote_ip, "127.0.0.1");
#else   
    g_udp_p2p.local_port = atoi(argv[1]);
    strcpy_s(g_udp_p2p.remote_ip, argv[2]);
    g_udp_p2p.remote_port = atoi(argv[3]);
#endif

    g_udp_p2p.init();
    // loop (onDraw loop of swos menu)
    while (g_udp_p2p.loop) {
        g_udp_p2p.receive_buffer();

        if (_kbhit()) {
            auto c = _getch();
            if (c == '1') {
                g_udp_p2p.send_buffer();
            }
            else if (c == 'q') {
                g_udp_p2p.loop = false;
                printf("q pressed, exit app\n");
            }
        }
    }

    g_udp_p2p.close();
}
