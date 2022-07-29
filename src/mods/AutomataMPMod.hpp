#pragma once

#include <chrono>
#include <array>

#include "../Mod.hpp"

#include "../VehHooks.hpp"
#include "../PlayerHook.hpp"

#include "../NierServer.hpp"
#include "../NierClient.hpp"
#include "../Packets.hpp"
#include "../Player.hpp"
#include "../EntitySync.hpp"

class AutomataMPMod : public Mod {
public:
    static std::shared_ptr<AutomataMPMod> get();

public:
    ~AutomataMPMod();

    std::string_view get_name() const override { return "AutomataMPMod"; }
    std::optional<std::string> on_initialize() override;

public:
    bool clientConnect();
    void serverStart();
    void sendPacket(const enet_uint8* data, size_t size);

    bool isServer() {
        return m_server != nullptr;
    }

    void on_frame() override;
    void on_think() override;
    void sharedThink();

    auto& getPlayers() {
        return m_players;
    }

    auto& getNetworkEntities() {
        return m_networkEntities;
    }

public:
    void synchronize();
    void serverPacketProcess(const Packet* data, size_t size);
    void sharedPacketProcess(const Packet* data, size_t size);

private:
    void processPlayerData(const nier_client_and_server::PlayerData* movement);
    void processAnimationStart(const nier_client_and_server::AnimationStart* animation);
    void processButtons(const nier_client_and_server::Buttons* buttons);
    void processEntitySpawn(nier_server::EntitySpawn* spawn);
    void processEntityData(nier_server::EntityData* data);

private:
    std::chrono::high_resolution_clock::time_point m_nextThink;

    bool m_isServer{ false };
    
    std::mutex m_hookGuard;

    VehHooks m_vehHooks;
    PlayerHook m_playerHook;

    std::unique_ptr<NierClient> m_client;
    std::unique_ptr<NierServer> m_server;

    std::array<Player, 2> m_players;
    EntitySync m_networkEntities;
};