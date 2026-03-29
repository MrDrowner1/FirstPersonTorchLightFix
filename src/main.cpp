#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

void TraverseAndSetCulled(RE::NiAVObject* a_obj, bool a_culled)
{
    if (!a_obj)
        return;

    if (auto* light = netimmerse_cast<RE::NiPointLight*>(a_obj)) {
        logger::info("FirstPersonTorchLightFix: NiPointLight '{}' culled={} -> {}",
            light->name.c_str(),
            light->GetAppCulled(),
            a_culled);
        light->SetAppCulled(a_culled);
    }

    if (auto* node = a_obj->AsNode()) {
        for (auto& child : node->GetChildren()) {
            TraverseAndSetCulled(child.get(), a_culled);
        }
    }
}

void FixTorchLights()
{
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player)
        return;

    auto* fpRoot = player->Get3D(true);
    auto* tpRoot = player->Get3D(false);

    if (!fpRoot || !tpRoot)
        return;

    logger::info("FirstPersonTorchLightFix: updating 1st person skeleton (cull=0)");
    TraverseAndSetCulled(fpRoot, false);

    logger::info("FirstPersonTorchLightFix: updating 3rd person skeleton (cull=1)");
    TraverseAndSetCulled(tpRoot, true);
}


class EquipEventSink : public RE::BSTEventSink<RE::TESEquipEvent>
{
public:
    static EquipEventSink* GetSingleton()
    {
        static EquipEventSink singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(
        const RE::TESEquipEvent* a_event,
        RE::BSTEventSource<RE::TESEquipEvent>*) override
    {
        if (!a_event || !a_event->actor)
            return RE::BSEventNotifyControl::kContinue;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (a_event->actor.get() != player)
            return RE::BSEventNotifyControl::kContinue;

        auto* form = RE::TESForm::LookupByID(a_event->baseObject);
        if (!form || form->GetFormType() != RE::FormType::Light)
            return RE::BSEventNotifyControl::kContinue;

        bool equipped = a_event->equipped;
        SKSE::GetTaskInterface()->AddTask([equipped]() {
            auto* camera = RE::PlayerCamera::GetSingleton();
            if (!camera || !camera->currentState || camera->currentState->id != RE::CameraStates::kFirstPerson)
                return;

            FixTorchLights();
        });

        return RE::BSEventNotifyControl::kContinue;
    }
};


SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);

    SKSE::GetMessagingInterface()->RegisterListener(
    [](SKSE::MessagingInterface::Message* a_msg) {
        if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
            auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
            if (holder) {
                holder->AddEventSink(EquipEventSink::GetSingleton());
                logger::info("FirstPersonTorchLightFix: equip sink registered");
            } else {
                logger::info("FirstPersonTorchLightFix: failed to get event source holder");
            }
        }

        if (a_msg->type == SKSE::MessagingInterface::kPostLoadGame) {
            SKSE::GetTaskInterface()->AddTask([]() {
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!player)
                    return;

                if (auto* fpRoot = player->Get3D(true))
                    TraverseAndSetCulled(fpRoot, true);
                if (auto* tpRoot = player->Get3D(false))
                    TraverseAndSetCulled(tpRoot, true);

                auto* leftHand = player->GetEquippedObject(false);
                auto* rightHand = player->GetEquippedObject(true);

                bool hasTorch =
                    (leftHand && leftHand->GetFormType() == RE::FormType::Light) ||
                    (rightHand && rightHand->GetFormType() == RE::FormType::Light);

                if (!hasTorch)
                    return;

                auto* camera = RE::PlayerCamera::GetSingleton();
                if (!camera || !camera->currentState ||
                    camera->currentState->id != RE::CameraStates::kFirstPerson)
                    return;

                FixTorchLights();
            });
        }
    });

    return true;
}