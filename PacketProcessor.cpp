namespace GOTHIC_ENGINE {
    void ProcessCoopPacket(json e, ENetEvent packet) {
        auto id = e["id"].get<std::string>();
        auto type = e["type"].get<int>();
        auto npcToSync = SyncNpcs.count(id.c_str()) ? SyncNpcs[id.c_str()] : NULL;

        if (type == SYNC_PLAYER_NAME) {
            auto connectId = e["connectId"].get<enet_uint32>();

            if (connectId == packet.peer->connectID) {
                std::string name = e["name"].get<std::string>();
                MyselfId = name.c_str();
            }

            return;
        }

        if (type == PLAYER_DISCONNECT) {
            string name = e["name"].get<std::string>().c_str();
            ChatLog(string::Combine("%s disconnected.", name));

            removeSyncedNpc(name);
            return;
        }

        if (IsCoopPaused) {
            return;
        }

        if (type == SYNC_SPELL_CAST) {
            //ChatLog(e.dump().c_str());
        }

        if (npcToSync == NULL) {
            if (IsCoopPlayer(id)) {
                npcToSync = addSyncedNpc(id.c_str());
                Myself->Reinit();
            }
            else if (UniqueNameToNpcList.count(id.c_str())) {
                npcToSync = addSyncedNpc(id.c_str());
            }
        }

        if (npcToSync) {
            npcToSync->localUpdates.push_back(e);
        }
    }

    void ProcessServerPacket(ENetEvent packet) {
        switch (packet.type) {
        case ENET_EVENT_TYPE_CONNECT:
        {
            auto playerName = string::Combine("FRIEND_%i", GetFreePlayerId());
            ChatLog(string::Combine("(Server) We got a new connection %s", string(playerName)));

            json j;
            j["id"] = "HOST";
            j["type"] = SYNC_PLAYER_NAME;
            j["name"] = string(playerName).ToChar();
            j["connectId"] = packet.peer->connectID;
            ReadyToSendJsons.enqueue(j);

            packet.peer->data = addSyncedNpc(playerName);

            if (Myself) {
                Myself->Reinit();
            }
            for each (auto n in BroadcastNpcs)
            {
                n.second->Reinit();
            }
            break;
        }
        case ENET_EVENT_TYPE_RECEIVE:
        {
            auto player = (RemoteNpc*)packet.peer->data;
            const char* data = (const char*)packet.packet->data;
            SaveNetworkPacket(data);

            auto j = json::parse(data);
            j["id"] = player->name.ToChar();
            ReadyToBeDistributedPackets.enqueue(j);

            ProcessCoopPacket(j, packet);
            break;
        }
        case ENET_EVENT_TYPE_DISCONNECT:
        {
            auto remoteNpc = (RemoteNpc*)(packet.peer->data);
            ChatLog(string::Combine("%s disconnected.", remoteNpc->name));

            json j;
            j["id"] = "HOST";
            j["type"] = PLAYER_DISCONNECT;
            j["name"] = string(remoteNpc->name).ToChar();
            ReadyToSendJsons.enqueue(j);

            removeSyncedNpc(remoteNpc->name);
            packet.peer->data = NULL;
            break;
        }
        }
    }

    void ProcessClientPacket(ENetEvent packet) {
        switch (packet.type) {
        case ENET_EVENT_TYPE_RECEIVE:
        {
            const char* data = (const char*)packet.packet->data;
            SaveNetworkPacket(data);

            auto j = json::parse(data);
            ProcessCoopPacket(j, packet);

            enet_packet_destroy(packet.packet);
            break;
        }
        case ENET_EVENT_TYPE_DISCONNECT:
        {
            ChatLog("Connection to the server lost.");
            break;
        }
        }
    }

    void PacketProcessorLoop() {
        PluginState = "PacketProcessorLoop";
        if (ReadyToBeReceivedPackets.isEmpty()) {
            return;
        }

        while (!ReadyToBeReceivedPackets.isEmpty()) {
            auto packet = ReadyToBeReceivedPackets.dequeue();

            if (ServerThread) {
                ProcessServerPacket(packet);
            }
            else if (ClientThread)
            {
                ProcessClientPacket(packet);
            }
        }
    }
}