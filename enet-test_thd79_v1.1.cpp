#include <stdio.h>
#include <conio.h>
#include "enet\enet.h"
#include <stdint.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Winmm.lib")

enum class ConnectionState {
    Waiting,
    Connected,
    Error
};

enum PeerCommand : uint8_t
{
    pc_None,
    pc_Punch,
    pc_Ping,
    pc_Pong,
};

int main(int argc, char **argv) 
{
    // general setting
    if (argc != 4) {
        printf("invalid parameter\n");
        printf("usesage: udt-test localPort remoteIP remoteServer\n");
        return 0;
    }

    // set ip address and port
    char local_port[32];
    char remote_ip[32] = { "127.0.0.1" };
    char remote_port[32];
    //if (prog_mode == 0)
    //{
    //    strcpy_s(local_port, "27885");
    //    strcpy_s(remote_port, "27886");
    //}
    //else
    //{
    //    strcpy_s(local_port, "27886");
    //    strcpy_s(remote_port, "27885");
    //}
    strcpy_s(local_port, argv[1]);
    strcpy_s(remote_ip, argv[2]);
    strcpy_s(remote_port, argv[3]);
    // init
    // -- enet
    if (enet_initialize() != 0) {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        return EXIT_FAILURE;
    }

    // -- vars
    ENetAddress address;
    ENetHost* local;
    ENetEvent event;

    // -- loc
  
    address.host = ENET_HOST_ANY;
    address.port = atoi(local_port);
   
    local = enet_host_create(&address , 2, 1, 0, 0);
    if (local == NULL) {
        printf("An error occurred while trying to create an ENet local.\n");
        exit(EXIT_FAILURE);
    }
    printf("created Host on port %d\n", atoi(local_port));
  
   
    ENetPeer* otherPeer = nullptr;
    ENetPeer* secondPeer = nullptr;
   // if(prog_mode==1)
    {       
        ENetAddress serverAddress;
         enet_address_set_host_ip(&serverAddress, remote_ip);
        serverAddress.port = atoi(remote_port);
        otherPeer = enet_host_connect(local, &serverAddress, 2, 0);
        printf("try to connect to peer host on port %d\n", atoi(remote_port));
    }

   ConnectionState state = ConnectionState::Waiting;
 
    // loop
    bool loop = true;
    int pongs{ 0 };
    int Connections = 0;
    while (loop) {
        Sleep(1);
        while (enet_host_service(local, &event, 10) > 0)
        {           
            char ip[40];
            enet_address_get_host_ip(&event.peer->address, ip, 40);
            
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
            {
               
                Connections++;
                
                if (otherPeer == event.peer)
                {
                    state = ConnectionState::Connected;
                    printf("We connected to %x:%u.\n",
                        event.peer->address.host,
                        event.peer->address.port);
                }
                else
                { 
                    secondPeer = event.peer;
                    printf("We accepted a connetion from %x:%u.\n",
                        event.peer->address.host,
                        event.peer->address.port);
                }
                
                break; 
            }
            case ENET_EVENT_TYPE_RECEIVE:
            {
                /*  printf("A packet of length %u containing %s was received from %s on channel %u.\n",
                      event.packet->dataLength,
                      event.packet->data,
                      event.peer->data,
                      event.channelID);*/
              //  QuickReadBuffer b((const char*)event.packet->data, event.packet->dataLength);
               // PeerCommand pc = b.Read<PeerCommand>();
                auto pc = *((PeerCommand*)(event.packet->data));
                if (pc == pc_Ping) {
                    printf("Received ping from %s:%d\n", ip, (int)event.peer->address.port);

                    auto command = PeerCommand::pc_Pong;
                    ENetPacket* packetPong = enet_packet_create(&command, sizeof(PeerCommand), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, packetPong);
                    enet_packet_destroy(event.packet);

                }
                else if (pc == pc_Pong)
                {
                    printf("Received pong number %d from %s:%d\n", ++pongs, ip, (int)event.peer->address.port);
                    enet_packet_destroy(event.packet);
                   
                }
                else if (pc == pc_Punch)
                {
                    printf("Received punch from %s:%d\n", ip, (int)event.peer->address.port);
                    enet_packet_destroy(event.packet);

                }
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: 
            {
                Connections--;
                if (Connections == 0)
                {
                    loop = false;
                }
                /*printf("client %s:%u.disconncted\n",
                    ip,
                    event.peer->address.port);*/

                if (otherPeer == event.peer)
                {
                    printf("Diconnect from Peer we created\n");                   
                }
                else
                { 
                    printf("Diconnect from other Peer\n");
                  
                }
              
                event.peer->data = NULL;
                
                break; 
            }
            default:
            {
                printf("Received event type %d\n", event.type);
            }
            }
        }

        if (state == ConnectionState::Waiting)
        {
            Sleep(16);
            auto command = PeerCommand::pc_Punch;
            ENetPacket* packetPing = enet_packet_create(&command, sizeof(PeerCommand), 0);
            
            if(otherPeer)
                enet_peer_send(otherPeer, 0, packetPing);
        }

        if (state == ConnectionState::Connected)
        {
            // send packet
            if (_kbhit()) {
                if (_getch() == '1')
                {
                    auto command = PeerCommand::pc_Ping;
                    ENetPacket* packetPing = enet_packet_create(&command, sizeof(PeerCommand), ENET_PACKET_FLAG_RELIABLE);
                    if(otherPeer)
                        enet_peer_send(otherPeer, 0, packetPing);
                    printf("Sent ping\n");
                }
                else if (_getch() == 'q')
                {
                    //loop = false;
                    if (otherPeer)
                    {
                        enet_peer_disconnect(secondPeer, 0);
                        enet_peer_disconnect(otherPeer, 0);
                    }
                    printf("Q pressed, exit app\n");
                }
            }
        }
   
    }
  
    enet_host_destroy(local);
    enet_deinitialize();
}