namespace GOTHIC_ENGINE {
    int CoopServerThread()
    {
        if (enet_initialize() != 0)
        {
            ChatLog("An error occurred while initializing ENet.");
            return EXIT_FAILURE;
        }
        atexit(enet_deinitialize);

        ENetAddress address;
        ENetHost* server;
        enet_address_set_host(&address, "0.0.0.0");
        address.port = 1234;
        server = enet_host_create(&address, 32, 2, 0, 0);
        if (server == NULL)
        {
            ChatLog("An error occurred while trying to create an ENet server host.");
            return EXIT_FAILURE;
        }

        ChatLog(string::Combine("(Server) Ready (v. %i).", COOP_VERSION));
        while (true) {
            ENetEvent event;
            auto eventStatus = enet_host_service(server, &event, 1);
            if (eventStatus > 0) {
                ReadyToBeReceivedPackets.enqueue(event);
            }

            if (!ReadyToBeDistributedPackets.isEmpty()) {
                auto jsonPacket = ReadyToBeDistributedPackets.dequeue();
                auto playerId = jsonPacket["id"].get<std::string>();

                for (int i = 0; i < server->peerCount; i++) {
                    auto peer = &server->peers[i];
                    auto player = (PeerData*)peer->data;

                    if (!peer || !player) {
                        continue;
                    }

                    if (!player->name.Compare(playerId.c_str())) {
                        auto stringPacket = jsonPacket.dump();
                        ENetPacket* packet = enet_packet_create(stringPacket.c_str(), strlen(stringPacket.c_str()) + 1, ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(peer, 0, packet);
                    }
                }
            }

            if (!ReadyToSendJsons.isEmpty()) {
                auto updateJSON = ReadyToSendJsons.dequeue().dump();
                ENetPacket* packet = enet_packet_create(updateJSON.c_str(), strlen(updateJSON.c_str()) + 1, ENET_PACKET_FLAG_RELIABLE);
                enet_host_broadcast(server, 0, packet);
            }
        }
    }
}