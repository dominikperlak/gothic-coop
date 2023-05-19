#include "resource.h"

namespace GOTHIC_ENGINE {
    void Game_Entry() {
        GetCurrentDirectory(MAX_PATH, GothicExeFolderPath);

        SymInitialize(GetCurrentProcess(), NULL, true);
        SymSetSearchPath(GetCurrentProcess(), GothicExeFolderPath);

        std::string logFilePath = GothicExeFolderPath;
        logFilePath.append("\\GothicCoopLog.log");
        GothicCoopLogPath = logFilePath;

        std::string configFilePath = GothicExeFolderPath;
        configFilePath.append("\\GothicCoopConfig.json");
        std::ifstream configFile(configFilePath);

        if (configFile.good()) {
            try {
                CoopConfig = json::parse(configFile);
            }
            catch (...) {
                Message::Error("(Gothic Coop) Invalid config file, please check your GothicCoopConfig.json file!");
                exit(1);
            }
        }
        else {
            Message::Error("(Gothic Coop) No config file found!");
            Message::Error(configFilePath.c_str());
            exit(1);
        }
    }

    void Game_Init() {
        MainThreadId = GetCurrentThreadId();
        ToggleGameLogKey = ReadConfigKey("toggleGameLogKey", "KEY_P");
        ToggleGameStatsKey = ReadConfigKey("toggleGameStatsKey", "KEY_O");
        StartServerKey = ReadConfigKey("startServerKey", "KEY_F1");
        StartConnectionKey = ReadConfigKey("startConnectionKey", "KEY_F2");
        ReinitPlayersKey = ReadConfigKey("reinitPlayersKey", "KEY_F3");
        RevivePlayerKey = ReadConfigKey("revivePlayerKey", "KEY_F4");

        PlayersDamageMultipler = CoopConfig["playersDamageMultipler"].get<int>();
        NpcsDamageMultipler = CoopConfig["npcsDamageMultipler"].get<int>();
        if (CoopConfig.contains("friendInstanceId")) {
            FriendInstanceId = string(CoopConfig["friendInstanceId"].get<std::string>().c_str()).ToChar();
        }
        if (CoopConfig.contains("nickname")) {
            MyNickname = string(CoopConfig["nickname"].get<std::string>().c_str()).ToChar();
        }
    }

    void Game_Loop() {
        PluginState = "GameLoop";
        if (IsLoadingLevel) {
            return;
        }

        CurrentMs = GetCurrentMs();
        GameChat->Render();
        GameStatsLoop();
        PacketProcessorLoop();
        DamageProcessorLoop();
        SpellCastProcessorLoop();
        ReviveFriendLoop();

        if (CurrentMs > LastNpcListRefreshTime + 1000) {
            BuildGlobalNpcList();
            LastNpcListRefreshTime = CurrentMs;
            PluginState = "GameLoop";
        }

        if (CurrentMs > LastUpdateListOfVisibleNpcs + 500) {
            UpdateVisibleNpc();
            LastUpdateListOfVisibleNpcs = CurrentMs;
            PluginState = "GameLoop";
        }

        if ((ClientThread || ServerThread) && !Myself) {
            Myself = new LocalNpc(player, MyselfId);
        }

        if (!IsCoopPaused) {
            PluginState = "PulseMyself";
            if (Myself) {
                Myself->Pulse();
                Myself->PackUpdate();
            }

            PluginState = "PulseBroadcastNpcs";
            for (auto p : BroadcastNpcs) {
                p.second->Pulse();
                p.second->PackUpdate();
            }

            PluginState = "UpdateSyncNpcs";
            for (auto const& pair : SyncNpcs) {
                auto npc = pair.second;
                npc->Update();

                if (npc->destroyed) {
                    SyncNpcs.erase(pair.first);
                }
            }
        }

        if (!IsPlayerTalkingWithAnybody()) {
            if (zinput->KeyToggled(StartServerKey) && !ServerThread && !ClientThread) {
                wchar_t mappedPort[1234];
                std::wcsncpy(mappedPort, L"UDP", 1234);
                new MappedPort(1234, mappedPort, mappedPort);

                Thread t;
                t.Init(&CoopServerThread);
                t.Detach();
                ServerThread = &t;
                MyselfId = "HOST";
            }

            if (zinput->KeyToggled(StartConnectionKey) && !ServerThread) {
                if (!ClientThread) {
                    addSyncedNpc("HOST");

                    Thread  t;
                    t.Init(&CoopClientThread);
                    t.Detach();
                    ClientThread = &t;

                    ogame->SetTime(0, 12, 00);
                    rtnMan->RestartRoutines();
                }
                else {
                    if (IsCoopPaused) {
                        ChatLog("Restoring world synchronization.");
                    }
                    else {
                        ChatLog("Stop world synchronization.");
                    }

                    IsCoopPaused = !IsCoopPaused;
                    rtnMan->RestartRoutines();
                }
            }

            if (zinput->KeyToggled(ReinitPlayersKey)) {
                for (auto playerNpcToName : PlayerNpcs) {
                    auto name = playerNpcToName.second;
                    if (SyncNpcs.count(name)) {
                        SyncNpcs[name]->InitCoopFriendNpc();
                    }
                }
            }
        }
    }

    void Game_SaveBegin() {
        IsSavingGame = true;
    }

    void Game_SaveEnd() {
        for (auto playerNpcToName : PlayerNpcs) {
            auto name = playerNpcToName.second;
            if (SyncNpcs.count(name)) {
                SyncNpcs[name]->InitCoopFriendNpc();
            }
        }

        IsSavingGame = false;
    }

    void LoadBegin() {
        IsLoadingLevel = true;
        Myself = NULL;
    }

    void LoadEnd() {
        if (ServerThread || ClientThread) {
            Myself = new LocalNpc(player, MyselfId);
        }

        std::map<string, RemoteNpc*> syncPlayerNpcs;
        for (auto playerNpcToName : PlayerNpcs) {
            auto name = playerNpcToName.second;
            if (SyncNpcs.count(name)) {
                syncPlayerNpcs[name] = SyncNpcs[name];
            }
        }

        SyncNpcs.clear();
        for (auto syncPlayerNpc : syncPlayerNpcs) {
            SyncNpcs[syncPlayerNpc.first] = syncPlayerNpc.second;
        }

        BroadcastNpcs.clear();
        UniqueNameToNpcList.clear();
        NpcToUniqueNameList.clear();
        NamesCounter.clear();
        NpcToFirstRoutineWp.clear();
        GameChat->Clear();
        LastNpcListRefreshTime = 0;
        LastUpdateListOfVisibleNpcs = 0;

        auto totWp = ogame->GetWorld()->wayNet->GetWaypoint("TOT");
        if (totWp) {
            CurrentWorldTOTPosition = &totWp->GetPositionWorld();
        }
        else {
            CurrentWorldTOTPosition = NULL;
        }

        IsLoadingLevel = false;
    }

    void Game_LoadEnd_SaveGame() {
        LoadEnd();

        auto coopFriendInstanceId = GetFriendDefaultInstanceId();
        auto* list = ogame->GetGameWorld()->voblist_npcs->next;

        while (list) {
            auto npc = list->data;
            if (npc->GetInstance() == coopFriendInstanceId && npc->GetAttribute(NPC_ATR_STRENGTH) == COOP_MAGIC_NUMBER) {
                ogame->spawnman->DeleteNpc(npc);
            }
            list = list->next;
        }
    }

    void Game_LoadBegin_Trigger() {
        return;
    }
}
