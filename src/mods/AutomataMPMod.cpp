#include <mutex>

#include <windows.h>

#include <spdlog/spdlog.h>

#include <sdk/Entity.hpp>
#include <sdk/EntityList.hpp>

#include <sdk/Game.hpp>
#include <sdk/ScriptFunctions.hpp>
#include "AutomataMPMod.hpp"

using namespace std;

std::shared_ptr<AutomataMPMod> AutomataMPMod::get() {
    static std::shared_ptr<AutomataMPMod> instance = std::make_shared<AutomataMPMod>();

    return instance;
}

AutomataMPMod::~AutomataMPMod() {
    if (m_client) {
        m_client->disconnect();
    }

    m_vehHooks.getHook().remove();
}

std::optional<std::string> AutomataMPMod::on_initialize() try {
    spdlog::info("Entering AutomataMPMod.");

    // Do it later.
    /*enetpp::global_state::get().initialize();
    if (!clientConnect()) {
        spdlog::info("Connection failed");
        serverStart();
    }
    else {
        spdlog::info("Connection success");
    }

    spdlog::info("Leaving AutomataMPMod.");*/

    return Mod::on_initialize();
} catch(std::exception& e) {
    spdlog::error("{}", e.what());
    return e.what();
} catch (...) {
    spdlog::error("Unknown exception");
    return "Unknown exception";
}

bool AutomataMPMod::clientConnect() {
    m_client = make_unique<NierClient>("127.0.0.1");

    if (m_client->isConnected()) {
        return true;
    }
    else {
        m_client.reset();
        return false;
    }
}

void AutomataMPMod::serverStart() {
    m_server = make_unique<NierServer>();
}

void AutomataMPMod::sendPacket(const enet_uint8* data, size_t size) {
    if (m_client) {
        m_client->send_packet(0, data, size, ENET_PACKET_FLAG_RELIABLE);
    }

    if (m_server) {
        m_server->send_packet_to_all_if(0, data, size, ENET_PACKET_FLAG_RELIABLE, [](auto& a) { return true; });
    }
}

void AutomataMPMod::on_frame() {
    if (m_server) {
        // Draw "Server" at 0, 0 with red text.
        ImGui::GetBackgroundDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(0, 0), ImGui::GetColorU32(ImGuiCol_Text), "Server");
    }

    if (m_client) {
        // Draw "Client" at 0, 0 with green text.
        ImGui::GetBackgroundDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(0, 0), ImGui::GetColorU32(ImGuiCol_Text), "Client");
    }
}

void AutomataMPMod::on_think() {
    if (nier::isLoading()) {
        m_players[1].setHandle(0);
        return;
    }

    auto entityList = EntityList::get();

    if (!entityList) {
        return;
    }


    auto player = entityList->getByName("Player");

    if (!player) {
        spdlog::info("Player not found");
        return;
    }
    
    auto partners = entityList->getAllByName("partner");
    auto partner = entityList->getByName("partner");

    if (partner) {
        if (GetAsyncKeyState(VK_F4) & 1) {
            Vector3f* myPos = Address(player->entity).get(0x50).as<Vector3f*>();

            for (auto i : partners) {
                if (!i->entity) {
                    continue;
                }

                Vector3f* vec = Address(i->entity).get(0x50).as<Vector3f*>();
                *vec = *myPos;
            }
        }
    }
    else {
        spdlog::info("Spawning partner");

        auto ent = entityList->spawnEntity("partner", EModel::MODEL_2B, *player->entity->getPosition());

        if (ent) {
            ent->entity->setBuddyHandle(player->handle);
            player->entity->setBuddyHandle(ent->handle);

            // alternate way of assigning AI/control to the entity easily.
            player->entity->changePlayer();
            player->entity->changePlayer();

            ent->assignAIRoutine("player");

            ent->entity->setBuddyFlags(8);
            ent->entity->setBuddyFromNpc();
            ent->entity->setBuddyFlags(0);
            ent->entity->setSuspend(false);

            m_players[1].setStartTick(*ent->entity->getTickCount());
        }
    }

    //spdlog::info("Player: 0x%p, handle: 0x%X", player, player->handle);
    //spdlog::info("Partner: 0x%p, handle: 0x%X", partner, partner->handle);
    //spdlog::info(" partner real ent: 0x%p", partner->entity);

    static uint32_t(*possessEntity)(Entity* player, uint32_t* handle, bool a3) = (decltype(possessEntity))0x1402118D0;
    static uint32_t(*unpossessEntity)(Entity* player, bool a2) = (decltype(unpossessEntity))0x140211AE0;

    /*if (GetAsyncKeyState(VK_F5) & 1) {
        auto curHandle = Address(0x1416053E0).as<uint32_t*>();
        auto curEnt = entityList->getByHandle(*curHandle);

        if (!curEnt)
            return;

        auto pl = entityList->getAllByName("Player");
        auto players = entityList->getAllByName("partner");
        players.insert(players.end(), pl.begin(), pl.end());

        auto curPlayer = players.begin();

        for (auto& i : *entityList) {
            if (!i.ent || !i.handle || i.handle == player->handle || i.handle == *curHandle || std::find(players.begin(), players.end(), i.ent) != players.end())
                continue;

            if (!i.ent->entity || i.ent->handle == *curHandle)
                continue;

            if (i.ent->entity->getHealth() == 0)
                continue;

            if ((*curPlayer)->entity->getBuddyThing() == 0x10200) {
                if ((*curPlayer)->entity->getPossessedHandle() != 0) {
                    unpossessEntity((*curPlayer)->entity, true);
                }

                possessEntity((*curPlayer)->entity, &i.handle, true);

                if ((*curPlayer)->entity->getPossessedHandle() != 0) {
                    curPlayer++;
                }
            }
            else
                curPlayer++;

            if (curPlayer == players.end())
                break;
        }
    }*/

    if (GetAsyncKeyState(VK_F6) & 1) {
        player->entity->changePlayer();
        //nier_client_and_server::ChangePlayer change;
        //sendPacket(change.data(), sizeof(change));
    }

    if (GetAsyncKeyState(VK_F7) & 1) {
        for (auto& i : *entityList) {
            if (!i.ent || !i.handle)
                continue;

            if (!i.ent->entity)
                continue;

            if (i.ent->entity->getHealth() == 0)
                continue;

            i.ent->entity->setBuddyFlags(-1);
            i.ent->entity->setBuddyFromNpc();
            i.ent->entity->setBuddyFlags(8);
            i.ent->entity->setBuddyFromNpc();
            i.ent->entity->setBuddyFlags(1);
        }
    }

    auto prevPlayer = player;

    // generates a linked list of players pretty much
    // so we can swap between all of them instead of just two.
    for (uint32_t index = 0; index < entityList->size(); ++index) {
        auto ent = entityList->get(index);

        if (!ent || !ent->entity) {
            continue;
        }
        
        if (ent->name != string("Player") && ent->name != string("partner"))
            continue;

        if (prevPlayer == ent)
            continue;

        prevPlayer->entity->setBuddyHandle(ent->handle);
        prevPlayer = ent;
    }

    if (prevPlayer != player) {
        prevPlayer->entity->setBuddyHandle(player->handle);
    }

    static uint32_t(*spawnBuddy)(Entity* player) = (decltype(spawnBuddy))0x140245C30;

    sharedThink();

    if (GetAsyncKeyState(VK_F9) & 1) {
        /*auto old = player->entity->getBuddyHandle();
        player->entity->setBuddyHandle(0);
        spawnBuddy(player->entity);
        player->entity->setBuddyHandle(old);*/

        auto ent = entityList->spawnEntity("partner", EModel::MODEL_2B, *player->entity->getPosition());

        if (ent) {
            ent->entity->setBuddyHandle(player->handle);
            player->entity->setBuddyHandle(ent->handle);

            // alternate way of assigning AI to the entity easily.
            //changePlayer(player->entity);
            //changePlayer(player->entity);

            ent->entity->setSuspend(false);

            ent->entity->setBuddyFlags(-1);
            ent->entity->setBuddyFromNpc();
            ent->entity->setBuddyFlags(1);
        }

        spdlog::info("{:x}", (uintptr_t)ent);
    }

    if ((GetAsyncKeyState(VK_F10) & 1) && partner) {
        for (auto p : partners) {
            p->entity->terminate();
        }
    }

    /*if (GetAsyncKeyState(VK_F2) & 1) {
        Entity::Signal signal;
        signal.signal = 0xEB1B2287;
        player->entity->signal(signal);
    }*/

    if (GetAsyncKeyState(VK_F3) & 1) {
        player->entity->setSuspend(!player->entity->isSuspend());
    }
}

void AutomataMPMod::sharedThink()
{
    spdlog::info("Shared think");

    static uint32_t(*changePlayer)(Entity* player) = (decltype(changePlayer))0x1401ED500;

    auto entityList = EntityList::get();

    if (!entityList) {
        return;
    }

    // main player entity that game is originally controlling
    auto player = entityList->getByName("Player");

    if (!player) {
        spdlog::info("Player not found");
        return;
    }

    auto controlledEntity = entityList->getPossessedEntity();

    if (!controlledEntity || !controlledEntity->entity) {
        spdlog::info("Controlled entity invalid");
        return;
    }

    if (m_client && controlledEntity->name != string("partner")) {
        auto realBuddy = entityList->getByHandle(controlledEntity->entity->getBuddyHandle());

        if (realBuddy && realBuddy->entity) {
            //realBuddy->entity->setBuddyFlags(0);
            realBuddy->entity->setSuspend(false);
            changePlayer(player->entity);
        }

        return;
    }

    m_vehHooks.addOverridenEntity(controlledEntity->entity);
    m_playerHook.reHook(controlledEntity->entity);
    controlledEntity->entity->setBuddyFlags(0);

    auto realBuddy = entityList->getByHandle(controlledEntity->entity->getBuddyHandle());
    
    if (realBuddy && realBuddy->entity) {
        realBuddy->entity->setBuddyFlags(0);
        
        m_players[1].setHandle(realBuddy->handle);
        synchronize();
    }
    else {
        spdlog::info("Buddy not found");
        m_players[1].setHandle(0);
    }

    auto& playerData = m_players[0].getPlayerData();
    playerData.facing = *controlledEntity->entity->getFacing();
    playerData.facing2 = *controlledEntity->entity->getFacing2();
    playerData.speed = *controlledEntity->entity->getSpeed();
    playerData.position = *controlledEntity->entity->getPosition();
    playerData.weaponIndex = *controlledEntity->entity->getWeaponIndex();
    playerData.podIndex = *controlledEntity->entity->getPodIndex();
    playerData.flashlight = *controlledEntity->entity->getFlashlightEnabled();
    playerData.heldButtonFlags = controlledEntity->entity->getCharacterController()->heldFlags;

    sendPacket(playerData.data(), sizeof(playerData));

    m_networkEntities.think();

    if (m_server) {
        m_server->think();
    }

    if (m_client) {
        m_client->think();
    }
}

void AutomataMPMod::synchronize() {
    auto npc = EntityList::get()->getByHandle(m_players[1].getHandle())->entity;

    auto& data = m_players[1].getPlayerData();
    *npc->getRunSpeedType() = SPEED_PLAYER;
    *npc->getFlashlightEnabled() = data.flashlight;
    *npc->getSpeed() = data.speed;
    *npc->getFacing() = data.facing;
    *npc->getFacing2() = data.facing2;
    *npc->getWeaponIndex() = data.weaponIndex;
    *npc->getPodIndex() = data.podIndex; 
    npc->getCharacterController()->heldFlags = data.heldButtonFlags;
    //*npc->getPosition() = movement.position;
}

void AutomataMPMod::serverPacketProcess(const Packet* data, size_t size) {
    spdlog::info("Server packet %i received", data->id);

    switch (data->id) {
    case ID_SPAWN_ENTITY:
        processEntitySpawn((nier_server::EntitySpawn*)data);
        break;
    case ID_ENTITY_DATA:
        processEntityData((nier_server::EntityData*)data);
        break;
    default:
        break;
    }
}

void AutomataMPMod::sharedPacketProcess(const Packet* data, size_t size) {
    spdlog::info("Shared packet %i received", data->id);

    switch (data->id) {
        // Shared
    case ID_PLAYER_DATA:
        processPlayerData((nier_client_and_server::PlayerData*)data);
        break;
    case ID_ANIMATION_START:
        processAnimationStart((nier_client_and_server::AnimationStart*)data);
        break;
    case ID_BUTTONS:
        processButtons((nier_client_and_server::Buttons*)data);
        break;
    case ID_CHANGE_PLAYER:
    default:
        break;
    }
}

void AutomataMPMod::processPlayerData(const nier_client_and_server::PlayerData* movement) {
    auto npc = m_players[1].getEntity();
    
    if (npc) {
        *npc->getPosition() = movement->position;
    }

    m_players[1].setPlayerData(*movement);
}

void AutomataMPMod::processAnimationStart(const nier_client_and_server::AnimationStart* animation) {
    auto npc = m_players[1].getEntity();

    switch (animation->anim) {
    case INVALID_CRASHES_GAME:
    case INVALID_CRASHES_GAME2:
    case INVALID_CRASHES_GAME3:
    case INVALID_CRASHES_GAME4:
    case Light_Attack:
        return;
    default:
        if (npc) {
            npc->startAnimation(animation->anim, animation->variant, animation->a3, animation->a4);
        }
    }
}

void AutomataMPMod::processButtons(const nier_client_and_server::Buttons* buttons) {
    auto npc = m_players[1].getEntity();

    if (npc) {
        memcpy(&npc->getCharacterController()->buttons, buttons->buttons, sizeof(buttons->buttons));

        for (uint32_t i = 0; i < Entity::CharacterController::INDEX_MAX; ++i) {
            auto controller = npc->getCharacterController();

            if (buttons->buttons[i] > 0) {
                controller->heldFlags |= (1 << i);
            }
        }
    }
}

void AutomataMPMod::processEntitySpawn(nier_server::EntitySpawn* spawn) {
    spdlog::info("Enemy spawn received");
    auto entityList = EntityList::get();

    if (entityList) {
        EntitySpawnParams params;
        EntitySpawnParams::PositionalData matrix = spawn->matrix;
        params.matrix = &matrix;
        params.model = spawn->model;
        params.model2 = spawn->model2;
        params.name = spawn->name;

        spdlog::info("Spawning %s", params.name);

        auto ent = entityList->spawnEntity(params);

        if (ent) {
            m_networkEntities.addEntity(ent, spawn->guid);
        }
    }
}

void AutomataMPMod::processEntityData(nier_server::EntityData* data) {
    m_networkEntities.processEntityData(data);
}
