#include <stdio.h>
#include <conio.h>
#include <thread>
#include <atomic>
#include "enet.h"

enum class ConnectionState {
    Waiting,
    Connected,
    Error
};

int main(int argc, char **argv) 
{
    // general setting
    if (argc != 2) {
        printf("invalid parameter\n");
        printf("usesage: udt-test [option]\n");
        printf("option: 0(server), 1(client)\n");
        return 0;
    }
    int prog_mode;
    if (strcmp(argv[1], "0") == 0)
        prog_mode = 0;  // server
    else if (strcmp(argv[1], "1") == 0)
        prog_mode = 1;  // client
    else
        return 0;

    // set ip address and port
    char local_port[32];
    char remote_ip[32] = { "127.0.0.1" };
    char remote_port[32];
    if (prog_mode == 0)
        strcpy_s(local_port, "27885");
    else
        strcpy_s(local_port, "27886");
    if (prog_mode == 0)
        strcpy_s(remote_port, "27886");
    else
        strcpy_s(remote_port, "27885");

    // init
    // -- enet
    if (enet_initialize() != 0) {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        return EXIT_FAILURE;
    }

    // -- vars
    ENetAddress address;
    ENetHost* local;
    ENetHost* remote = { 0 };
    ENetEvent event = { 0 };
    ENetPeer* peer = { 0 };
    uint8_t disconnected = false;

    // -- local
    address.host = ENET_HOST_ANY;
    address.port = atoi(local_port);
    local = enet_host_create(&address, 32, 2, 0, 0);
    if (local == NULL) {
        printf("An error occurred while trying to create an ENet local.\n");
        exit(EXIT_FAILURE);
    }
    printf("init local: done\n");

    // -- remote
    remote = enet_host_create(NULL, 1, 2, 0, 0);
    if (remote == NULL) {
        printf("An error occurred while trying to create an ENet remote.\n");
        exit(EXIT_FAILURE);
    }
    printf("init remote: done\n");

    // -- connect
    enet_address_set_host(&address, remote_ip);
    address.port = atoi(remote_port);

    std::atomic<ConnectionState> state = ConnectionState::Waiting;
    auto doConnect = [&]() {
        if (NULL == enet_host_connect(remote, &address, 2, 0)) {
            printf("connection: failed\n");
            state = ConnectionState::Error;
        }
        else {
            state = ConnectionState::Connected;
            printf("connection: ok\n");
            printf("press '1' to send a packet\n");
        }
    };

    std::thread connectThread([&]() {
        doConnect();
    });

    // loop
    bool loop = true;
    int connectionWaits = 0;
    while (loop) {
        while (enet_host_service(remote, &event, 0) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                printf("A new client connected from %x:%u.\n",
                    event.peer->address.host,
                    event.peer->address.port);
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                printf("A packet of length %u containing %s was received from %s on channel %u.\n",
                    event.packet->dataLength,
                    event.packet->data,
                    event.peer->data,
                    event.channelID);
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                printf("%s disconnected.\n", event.peer->data);
                event.peer->data = NULL;
                disconnected = true;
            }
        }
        
        switch (state) {
            case ConnectionState::Error: {
                printf("Error in Connection. App will exit\n");
                loop = false;
                break;
            }
            case ConnectionState::Waiting: {
                //doConnect(); dont in other thread
                printf("Waiting for connection (%d times now)\n", ++connectionWaits);
                Sleep(200);
                break;
            }
            case ConnectionState::Connected: {
                // simulating 30fps in on_draw loop of swos menu
                Sleep(33);

                // send packet
                if (_kbhit()) {
                    if (_getch() == '1')
                    {
                        ENetPacket* packet = enet_packet_create("packet", strlen("packet") + 1, ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(peer, 0, packet);
                        enet_host_flush(remote);
                    }
                    else if (_getch() == 'q')
                    {
                        loop = false;
                        printf("Q pressed, exit app\n");
                    }
                }
            }
        } 
    }

    // close
    // -- drop connection, since disconnection didn't successed
    if (!disconnected) {
        enet_peer_reset(peer);
    }

    // -- disconnect
    connectThread.join();
    enet_peer_disconnect(peer, 0);
    enet_host_destroy(remote);
    enet_deinitialize();
}