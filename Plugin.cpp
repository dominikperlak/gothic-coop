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
        ChatKey = ReadConfigKey("chatKey", "KEY_T");

        if (CoopConfig.contains("port")) {
            ConnectionPort = CoopConfig["port"].get<int>();
        }

        if (CoopConfig.contains("bodyTextVarNr")) {
            MyBodyTextVarNr = CoopConfig["bodyTextVarNr"].get<int>();
        }

        if (CoopConfig.contains("headVarNr")) {
            MyHeadVarNr = CoopConfig["headVarNr"].get<int>();
        }

        if (CoopConfig.contains("headModel")) {
            MyHeadModel = CoopConfig["headModel"].get<std::string>();
        }

        // Set Appearance with default value "Custom" if not specified
        if (!CoopConfig.contains("Appearance")) {
            CoopConfig["Appearance"] = "Custom";
        }

        PlayersDamageMultipler = CoopConfig["playersDamageMultipler"].get<int>();
        NpcsDamageMultipler = CoopConfig["npcsDamageMultipler"].get<int>();
        if (CoopConfig.contains("friendInstanceId")) {
            auto stdStringFriendInstanceId = CoopConfig["friendInstanceId"].get<std::string>();
            std::transform(stdStringFriendInstanceId.begin(), stdStringFriendInstanceId.end(), stdStringFriendInstanceId.begin(), ::tolower);
            FriendInstanceId = string(stdStringFriendInstanceId.c_str()).ToChar();
        }
        if (CoopConfig.contains("nickname")) {
            MyNickname = string(CoopConfig["nickname"].get<std::string>().c_str()).ToChar();
        }
        if (CoopConfig.contains("editmode")) {
            auto editModeValue = string(CoopConfig["editmode"].get<std::string>().c_str());
            if (editModeValue.Compare("MARVIN")) {
                WorldEditMode = true;
                Thread  t;
                ClientThread = &t;
                ChatLog("World edit mode is activated!");
            }
        }
    }

    void Game_Loop() {
        PluginState = "GameLoop";
        if (IsLoadingLevel) {
            return;
        }

        CurrentMs = GetCurrentMs();
        GameChat->Render();

        // Render chat input line when typing
        if (IsTyping) {
            auto prevFont = screen->GetFontName();
            auto prevColor = screen->fontColor;
            screen->SetFont(zSTRING("Font_Old_10_White_Hi.TGA"));
            screen->SetFontColor(GFX_WHITE);
            screen->Print(50, 30, zSTRING(string::Combine("> %s", string(ChatInputBufferStd.c_str()))));
            screen->SetFont(prevFont);
            screen->SetColor(prevColor);
        }
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

            PluginState = "DisplayPing";
            if (ClientThread && CurrentPing > 0) {
                auto font = screen->GetFontName();
                auto color = screen->fontColor;

                screen->SetFont(zSTRING("Font_Old_10_White_Hi.TGA"));
                screen->SetFontColor(CurrentPing > 100 ? GFX_RED : GFX_WHITE);
                screen->Print(50, 0, "Ping: " + Z CurrentPing);

                screen->SetFont(font);
                screen->SetColor(color);
            }
        }

        PluginState = "KeysPressedChecks";
        if (!IsPlayerTalkingWithAnybody() && !WorldEditMode) {
            if (zinput->KeyToggled(StartServerKey) && !ServerThread && !ClientThread) {
                wchar_t mappedPort[1234];
                std::wcsncpy(mappedPort, L"UDP", 1234);
                new MappedPort(ConnectionPort, mappedPort, mappedPort);

                Thread t;
                t.Init(&CoopServerThread);
                t.Detach();
                ServerThread = &t;
                MyselfId = "HOST";
                // Apply appearance based on Appearance setting
                if (CoopConfig.contains("Appearance") && CoopConfig["Appearance"].get<std::string>() == "Custom") {
                    player->SetAdditionalVisuals(zSTRING("hum_body_Naked0"), MyBodyTextVarNr, DefaultBodyTexColorNr, zSTRING(MyHeadModel.c_str()), MyHeadVarNr, 0, -1);
                } else {
                    // Use default appearance values
#if ENGINE >= Engine_G2
                    player->SetAdditionalVisuals(zSTRING("hum_body_Naked0"), 9, DefaultBodyTexColorNr, zSTRING("HUM_HEAD_PONY"), 18, 0, -1);
#else
                    player->SetAdditionalVisuals(zSTRING("hum_body_Naked0"), 4, DefaultBodyTexColorNr, zSTRING("HUM_HEAD_PONY"), 9, 0, -1);
#endif
                }
            }

            if (zinput->KeyToggled(StartConnectionKey) && !ServerThread) {
                if (!ClientThread) {
                    addSyncedNpc("HOST");

                    Thread  t;
                    t.Init(&CoopClientThread);
                    t.Detach();
                    ClientThread = &t;

                    ogame->SetTime(ogame->GetWorldTimer()->GetDay(), 12, 00);
                    rtnMan->RestartRoutines();
                    // Apply appearance based on Appearance setting
                    if (CoopConfig.contains("Appearance") && CoopConfig["Appearance"].get<std::string>() == "Custom") {
                        player->SetAdditionalVisuals(zSTRING("hum_body_Naked0"), MyBodyTextVarNr, DefaultBodyTexColorNr, zSTRING(MyHeadModel.c_str()), MyHeadVarNr, 0, -1);
                    } else {
                        // Use default appearance values
#if ENGINE >= Engine_G2
                        player->SetAdditionalVisuals(zSTRING("hum_body_Naked0"), 9, DefaultBodyTexColorNr, zSTRING("HUM_HEAD_PONY"), 18, 0, -1);
#else
                        player->SetAdditionalVisuals(zSTRING("hum_body_Naked0"), 4, DefaultBodyTexColorNr, zSTRING("HUM_HEAD_PONY"), 9, 0, -1);
#endif
                    }
                }
                else if (IsClientConnected) {
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
                ChatLog("Respawning all coop friend NPCs.");

                std::map<oCNpc*, string> PlayerNpcsCopy = PlayerNpcs;
                for (auto playerNpcToName : PlayerNpcsCopy) {
                    auto name = playerNpcToName.second;
                    if (SyncNpcs.count(name)) {
                        SyncNpcs[name]->ReinitCoopFriendNpc();
                    }
                }
            }
        }

        // Chat typing toggle and capture (T to start typing)
        if (!WorldEditMode) {
            if (!IsTyping && !IsPlayerTalkingWithAnybody() && zinput->KeyToggled(ChatKey)) {
                // Enter typing mode: save camera mode and disable mouse to block clicks
                PrevCameraMode = ogame->GetCameraAI()->GetMode();
                PrevMouseDeviceEnabled = zinput->GetDeviceEnabled(zINPUT_MOUSE);
                ogame->GetCameraAI()->SetMode(zSTRING("CAMMODDIALOG"), ogame->GetCameraAI()->targetVobList);
                zinput->SetDeviceEnabled(zINPUT_MOUSE, 0);
                IsTyping = true;
                ChatOpenedMs = CurrentMs;
                ChatInputBufferStd.clear();
                LastTypedChar = 0;
                LastTypedCharMs = 0;
                zinput->ClearKeyBuffer(); // flush chat-open key press from char buffer
            }

            if (IsTyping) {
                // Prevent character movement while chat is open – reset every frame before the engine sees them
                zinput->ResetRepeatKey(KEY_UP);
                zinput->ResetRepeatKey(KEY_DOWN);
                zinput->ResetRepeatKey(KEY_LEFT);
                zinput->ResetRepeatKey(KEY_RIGHT);
                zinput->ResetRepeatKey(KEY_UPARROW);
                zinput->ResetRepeatKey(KEY_DOWNARROW);
                zinput->ResetRepeatKey(KEY_LEFTARROW);
                zinput->ResetRepeatKey(KEY_RIGHTARROW);
                zinput->ResetRepeatKey(KEY_LCONTROL);
                zinput->ResetRepeatKey(KEY_RCONTROL);
                // KEY_LSHIFT / KEY_RSHIFT intentionally NOT reset – needed for uppercase detection below

                // During the startup delay: drain the buffer every frame so nothing accumulates
                if (CurrentMs - ChatOpenedMs < ChatInputStartDelayMs) {
                    zinput->ClearKeyBuffer();
                }
                else
                {

                // Read all characters from input buffer (handles localization/shift) and build message
                unsigned char ch = zinput->GetChar();
                bool submitted = false;
                while (ch) {
                    long long now = GetCurrentMs();

                    // Debounce identical characters happening too quickly
                    if (ch == LastTypedChar && (now - LastTypedCharMs) < ChatCharRepeatDelayMs) {
                        ch = zinput->GetChar();
                        continue;
                    }

                    // Accept and record this character
                    LastTypedChar = ch;
                    LastTypedCharMs = now;

                    if (ch == 13) { // Enter
                        submitted = true;
                        break;
                    }
                    else if (ch == 27) { // Escape
                        ChatInputBufferStd.clear();
                        IsTyping = false;
                        ogame->GetCameraAI()->SetMode(PrevCameraMode, ogame->GetCameraAI()->targetVobList);
                        zinput->SetDeviceEnabled(zINPUT_MOUSE, PrevMouseDeviceEnabled);
                        break;
                    }
                    else if (ch == 8) { // Backspace
                        if (!ChatInputBufferStd.empty()) ChatInputBufferStd.pop_back();
                    }
                    else if (ch >= 32 && ch < 127) { // printable ASCII (skip 127+ ghost values from modifier keys)
                        if (ChatInputBufferStd.size() < ChatInputMaxLength) {
                            // Apply Shift and CapsLock for letter keys (GetChar returns lowercase only)
                            if (ch >= 'a' && ch <= 'z') {
                                bool shifted  = zinput->KeyPressed(KEY_LSHIFT) || zinput->KeyPressed(KEY_RSHIFT);
                                bool capsLock = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
                                if (shifted ^ capsLock) ch = ch - 'a' + 'A';
                            }
                            ChatInputBufferStd.push_back((char)ch);
                        }
                    }

                    ch = zinput->GetChar();
                }

                // Clear key states to avoid other systems reacting to these presses
                zinput->ClearKeyBuffer();
                for (int k = KEY_A; k <= KEY_Z; ++k) zinput->ResetRepeatKey(k);
                for (int k = KEY_1; k <= KEY_0; ++k) zinput->ResetRepeatKey(k);
                zinput->ResetRepeatKey(KEY_SPACE);

                if (submitted) {
                    std::string sender = "";
                    if (MyNickname.Length() > 0) sender = MyNickname.ToChar(); else sender = MyselfId.ToChar();
                    std::string message = ChatInputBufferStd;
                    if (!message.empty()) {
                        std::string full = sender + ": " + message;
                        ChatLog(string(full.c_str()));
                        if (ServerThread || ClientThread) {
                            json j;
                            j["id"] = MyselfId.ToChar();
                            j["type"] = SYNC_CHAT;
                            j["text"] = full;
                            ReadyToSendJsons.enqueue(j);
                        }
                    }

                    // Exit typing mode: restore camera and mouse
                    IsTyping = false;
                    ChatInputBufferStd.clear();
                    ogame->GetCameraAI()->SetMode(PrevCameraMode, ogame->GetCameraAI()->targetVobList);
                    zinput->SetDeviceEnabled(zINPUT_MOUSE, PrevMouseDeviceEnabled);
                }

                } // end delay else
            }
        }

        if (WorldEditMode) {
            if (zinput->KeyToggled(StartServerKey)) {
                if (player->GetObjectName() == "PC_HERO") {
                    return;
                }

                auto currentNpc = player;
                Myself->npc->SetAsPlayer();

                ogame->spawnman->SpawnNpc(currentNpc, currentNpc->GetPositionWorld(), 0.f);
                ChatLog(string::Combine("%s was added to the world!", string(currentNpc->GetObjectName())));
            }

            if (zinput->KeyToggled(RevivePlayerKey)) {
                if (player->GetObjectName() == "PC_HERO") {
                    return;
                }

                auto currentNpc = player;
                Myself->npc->SetAsPlayer();

                ogame->spawnman->DeleteNpc(currentNpc);
                ogame->GetGameWorld()->RemoveVob(currentNpc);

                ChatLog(string::Combine("%s was removed from the world!", string(currentNpc->GetObjectName())));
            }

            if (zinput->KeyToggled(ToggleGameLogKey)) {
                if (player->GetObjectName() == "PC_HERO") {
                    return;
                }

                Myself->npc->SetAsPlayer();
            }
        }

        if ((ClientThread || ServerThread) && !Myself) {
            PluginState = "Myself_Init";
            Myself = new LocalNpc(player, MyselfId);
        }

        PluginState = "GameLoop_End";
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
        KilledByPlayerNpcNames.clear();

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
