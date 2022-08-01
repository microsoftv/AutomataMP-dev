#include <string>

#include <spdlog/spdlog.h>

#include <sdk/Entity.hpp>
#include <sdk/EntityList.hpp>

#include <utility/Scan.hpp>
#include <utility/Module.hpp>
#include <utility/Input.hpp>

#include "Mods.hpp"
#include "mods/AutomataMPMod.hpp"
#include "AutomataMP.hpp"

#include "VehHooks.hpp"

using namespace std;

uintptr_t get_on_update_function() {
    static uintptr_t on_update_function = []() -> uintptr_t {
        spdlog::info("[VehHooks] Finding on_update_function...");

        const auto middle = utility::scan(utility::get_executable(), "? 89 ? e8 00 00 00 ? 89 ? f0 00 00 00 ? ? ? e8 ? ? ? ? ? ? ? ? ? 00 00");

        if (!middle) {
            spdlog::error("[VehHooks] Failed to find on_update_function.");
            return 0;
        }

        const auto int3s = utility::scan_reverse(*middle, 0x100, "CC CC CC");

        if (!int3s) {
            spdlog::error("[VehHooks] Failed to find int3s.");
            return 0;
        }

        const auto result = *int3s + 3;

        spdlog::info("[VehHooks] on_update_function: {:x}", result);

        return result;
    }();

    return on_update_function;
}

// gets an instruction near the end of the function that has an easy-ish to scan for pattern
uintptr_t get_on_processed_buttons() {
    static uintptr_t on_processed_buttons = []() -> uintptr_t {
        spdlog::info("[VehHooks] Finding on_processed_buttons...");

        const auto middle = utility::scan(utility::get_executable(), "C7 ? 90 07 00 00 14 00 00 00 48");

        if (!middle) {
            spdlog::error("[VehHooks] Failed to find on_processed_buttons.");
            return 0;
        }

        const auto result = *middle + 10;

        spdlog::info("[VehHooks] on_processed_buttons: {:x}", result);

        return result;
    }();

    return on_processed_buttons;
}

// near the top of ProcessButtons
uintptr_t get_on_set_held_flags() {
    static uintptr_t pre_process_buttons = []() -> uintptr_t {
        spdlog::info("[VehHooks] Finding on_set_held_flags...");

        const auto middle = get_on_processed_buttons();
        const auto ref = utility::scan_reverse(middle, 0x1000, "CC CC CC");

        if (!ref) {
            spdlog::error("[VehHooks] Failed to find pre_process_buttons.");
            return 0;
        }

        const auto func = *ref + 3;

        spdlog::info("[VehHooks] pre_process_buttons: {:x}", func);

        const auto result = utility::scan_disasm(func, 0x100, "48 89 81 40 07 00 00");

        if (!result) {
            spdlog::error("[VehHooks] Failed to find on_set_held_Flags.");
            return 0;
        }

        spdlog::info("[VehHooks] on_set_held_flags: {:x}", *result);

        return *result;
    }();

    return pre_process_buttons;
}

uintptr_t get_entity_terminate_fn() {
    static uintptr_t entity_terminate_fn = []() -> uintptr_t {
        spdlog::info("[VehHooks] Finding entity_terminate_fn...");

        const auto terminate = ScriptFunctions::get().find("Behavior.terminate");

        if (!terminate) {
            spdlog::error("[VehHooks] Failed to find entity_terminate_fn.");
            return 0;
        }

        const auto terminate_script_fn = terminate->function;

        spdlog::info("[VehHooks] entity_terminate_script_fn: {:x}", terminate_script_fn);

        const auto ref = utility::scan_disasm(terminate_script_fn, 0x100, "0F 85");

        if (!ref) {
            spdlog::error("[VehHooks] Failed to find entity_terminate_fn.");
            return 0;
        }

        const auto result = utility::calculate_absolute(*ref + 2);

        spdlog::info("[VehHooks] entity_terminate_fn: {:x}", result);

        return result;
    }();

    return entity_terminate_fn;
}

VehHooks::VehHooks() {
    spdlog::info("[VehHooks] Initializing...");

    // required to allow button to be held down without resetting
    /*m_hook.hook(0x140262B22, [=](const VehHook::RuntimeInfo& info) {
        auto entity = Address(info.context->Rcx).get(-0xCA0).as<Entity*>();

        auto& player = AutomataMPMod::get()->getPlayers()[1];

        if (entity == player.getEntity()) {
            entity->getCharacterController()->heldFlags = player.getPlayerData().heldButtonFlags;
        }
    });*/

    // Early version hooks
    /*addHook(0x140263006, &VehHooks::onProcessedButtons);
    addHook(0x1404F9AA0, &VehHooks::onPreEntitySpawn);
    addHook(0x1404F9BE9, &VehHooks::onPostEntitySpawn);
    addHook(0x1404F8DE0, &VehHooks::onEntityTerminate);
    addHook(0x140519460, &VehHooks::onUpdate);*/

    /*addHook(0x140263006, &VehHooks::onProcessedButtons);
    addHook(0x1404F9AA0, &VehHooks::onPreEntitySpawn);
    addHook(0x1404F9BE9, &VehHooks::onPostEntitySpawn);
    addHook(0x1404F8DE0, &VehHooks::onEntityTerminate);*/

    const auto [spawn_fn, spawn_this] = EntityList::getSpawnEntityFn();
    addHook((uintptr_t)spawn_fn, &VehHooks::onPreEntitySpawn);
    addHook((uintptr_t)EntityList::getPostSpawnEntityFn(), &VehHooks::onPostEntitySpawn);
    addHook(get_entity_terminate_fn(), &VehHooks::onEntityTerminate);
    // todo: hook the other version of the terminate function (the script function)
    addHook(get_on_update_function(), &VehHooks::onUpdate);
    addHook(get_on_processed_buttons(), &VehHooks::onProcessedButtons);

    m_hook.hook(get_on_set_held_flags(), [=](const VehHook::RuntimeInfo& info) {
        auto entity = Address(info.context->Rcx).get(-0xCA0).as<Entity*>();

        auto& player = AutomataMPMod::get()->getPlayers()[1];

        if (entity == player.getEntity()) {
            entity->getCharacterController()->heldFlags = player.getPlayerData().heldButtonFlags;
        }
    });

    spdlog::info("[VehHooks] Initialized.");
}

void VehHooks::onLightAttack(const VehHook::RuntimeInfo& info) {
    auto entity = Address(info.context->R8).get(-0xCA0).as<Entity*>();

    if (m_overridenEntities.count(entity) != 0) {
        nier_client_and_server::Buttons buttons;
        buttons.buttons[Entity::CharacterController::INDEX_ATTACK_LIGHT] = (uint32_t)info.context->Rax;
        buttons.buttons[Entity::CharacterController::INDEX_ATTACK_HEAVY] = (uint32_t)info.context->Rdx;
        AutomataMPMod::get()->sendPacket(buttons.data(), sizeof(buttons));
    }
}

void VehHooks::onHeavyAttack(const VehHook::RuntimeInfo & info) {
    auto entity = Address(info.context->R8).get(-0xCA0).as<Entity*>();

    if (m_overridenEntities.count(entity) != 0) {
        nier_client_and_server::Buttons buttons;
        buttons.buttons[Entity::CharacterController::INDEX_ATTACK_LIGHT] = (uint32_t)info.context->Rdx;
        buttons.buttons[Entity::CharacterController::INDEX_ATTACK_HEAVY] = (uint32_t)info.context->Rax;
        AutomataMPMod::get()->sendPacket(buttons.data(), sizeof(buttons));
    }
}

void VehHooks::onProcessedButtons(const VehHook::RuntimeInfo& info)
{   
    // OLD VERSION USES R8. THE REGISTER IS THE CHARACTER CONTROLLER OF THE ENTITY.
    //auto entity = Address(info.context->R8).get(-0xCA0).as<Entity*>();
    auto entity = Address(info.context->Rbx).get(-0xCA0).as<Entity*>();

    if (m_overridenEntities.count(entity) != 0) {
        for (auto button : entity->getCharacterController()->buttons) {
            if (button != 0) {
                nier_client_and_server::Buttons buttons;
                memcpy(&buttons.buttons, &entity->getCharacterController()->buttons, sizeof(buttons.buttons));
                AutomataMPMod::get()->sendPacket(buttons.data(), sizeof(buttons));
                break;
            }
        }
    }
}

void VehHooks::onPreEntitySpawn(const VehHook::RuntimeInfo& info) {
    if (!AutomataMPMod::get()->isServer()) {
        return;
    }

    auto spawnParams = (EntitySpawnParams*)info.context->Rdx;

    if (spawnParams->name) {
        if (spawnParams->name != string("FreeEnemy")) {
            return;
        }

        lock_guard<mutex> _(m_spawnMutex);
        m_threadIdToSpawnParams[GetCurrentThreadId()] = spawnParams;
    }
}

void VehHooks::onPostEntitySpawn(const VehHook::RuntimeInfo& info) {
    auto threadId = GetCurrentThreadId();

    if (m_threadIdToSpawnParams.find(threadId) == m_threadIdToSpawnParams.end()) {
        return;
    }

    auto entity = (EntityContainer*)info.context->Rax;

    if (entity) {
        AutomataMPMod::get()->getNetworkEntities().onEntityCreated(entity, m_threadIdToSpawnParams[threadId]);
    }

    lock_guard<mutex> _(m_spawnMutex);
    m_threadIdToSpawnParams.erase(threadId);
}

void VehHooks::onEntityTerminate(const VehHook::RuntimeInfo& info) {
    auto ent = (EntityContainer*)info.context->Rcx;

    AutomataMPMod::get()->getNetworkEntities().onEntityDeleted(ent);
}

// Potential OnUpdate candidates
/*
Function calls:
0x0000000140519460: 1
0x000000014051DCF0: 1
0x00000001401A35C0: 5
0x00000001405B6AC0: 5
0x00000001401A34F0: 5
0x00000001401A3440: 5
0x00000001401A3640: 5
0x00000001401A36C0: 5
0x00000001408E6730: 5
0x0000000140519470: 1
0x0000000140001060: 1
0x00000001408E6A10: 5
0x00000001408134B0: 5
0x00000001408082C0: 5
0x0000000140813520: 5
0x00000001408710A0: 5
0x00000001408710F0: 5
0x00000001408711A0: 5
0x00000001408E6900: 5
0x00000001408135F0: 5
0x0000000140938030: 5
*/

void VehHooks::onUpdate(const VehHook::RuntimeInfo& info) {
    auto entityList = EntityList::get();
    auto player = entityList->getByName("Player");

    if (!player) {
        return;
    }

    //spdlog::info("In here");

    //static std::unordered_map<uintptr_t, std::unordered_set<uintptr_t>> functionCalls;
    //static std::mutex mut;

    /*mut.lock();
    functionCalls[Address(info.context->Rbx).get(0x20).to<uintptr_t>()].insert(info.context->Rbx);
    //auto rcx = (Entity*)info.context->Rcx;

    //spdlog::info("{:x} ({})", (uintptr_t)rcx, rcx->getContainer()->name);

    spdlog::info("Function calls: ");

    for (auto& i : functionCalls) {
        spdlog::info("{:x}: {}", (uintptr_t)i.first, i.second.size());
    }
    mut.unlock();*/

    auto& mods = g_framework->get_mods()->get_mods();

    for (auto& mod : mods) {
        mod->on_think();
    }

    if (utility::was_key_down(VK_F1) && AutomataMPMod::get()->isServer()) {
        /*static std::unordered_map<uint32_t, uint32_t> models{ { EModel::MODEL_2B, EModel::MODEL_9S }, 
                                                              { EModel::MODEL_9S, EModel::MODEL_A2 }, 
                                                              { EModel::MODEL_A2, EModel::MODEL_2B }};

        auto ent = entityList->spawnEntity("Player", models[*player->entity->getModel()], *player->entity->getPosition());

        if (ent) {
            player->entity->setBuddyHandle(ent->handle);
            ent->entity->setSuspend(false);
            player->entity->changePlayer();
        }*/

        EntitySpawnParams params;
        EntitySpawnParams::PositionalData matrix;
        matrix.position = Vector4f{*player->entity->getPosition(), 1.0f};
        matrix.unknown = *Address(player->entity).get(0x90).as<Vector4f*>();
        params.name = "FreeEnemy";
        params.matrix = &matrix;
        params.model = 0x21020;
        params.model2 = 0x21020;

        auto test = entityList->spawnEntity(params);

        if (test) {
            test->entity->setSuspend(false);
            AutomataMPMod::get()->getNetworkEntities().onEntityCreated(test, &params);
        }

        return;
    }

    if (utility::was_key_down(VK_F2)) {
        auto enemies = entityList->getAllByName("FreeEnemy");

        for (auto i : enemies) {
            if (!i->entity) {
                continue;
            }

            i->entity->terminate();
        }
    }
}

void VehHooks::addOverridenEntity(Entity* ent) {
    m_overridenEntities.insert(ent);
}

void VehHooks::addHook(uintptr_t address, MemberCallbackFn cb) {
    m_hook.hook(address, [=](const VehHook::RuntimeInfo& info) { invoke(cb, this, info); });
}