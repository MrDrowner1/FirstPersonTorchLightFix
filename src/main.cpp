#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace {

    void TraverseAndSetCulled(RE::NiAVObject* a_obj, bool a_culled)
    {
        if (!a_obj)
            return;

        if (auto* light = netimmerse_cast<RE::NiPointLight*>(a_obj)) {
            logger::info("FPTorchFix: NiPointLight '{}' culled={} -> {}",
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

        auto* camera = RE::PlayerCamera::GetSingleton();
        if (!camera || !camera->currentState || camera->currentState->id != RE::CameraStates::kFirstPerson)
            return;

        auto* fpRoot = player->Get3D(true);
        auto* tpRoot = player->Get3D(false);

        if (!fpRoot || !tpRoot)
            return;

        logger::info("FPTorchFix: updating 1st person skeleton (cull=0)");
        TraverseAndSetCulled(fpRoot, false);

        logger::info("FPTorchFix: updating 3rd person skeleton (cull=1)");
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
                if (equipped) {
                    FixTorchLights();
                }
            });

            return RE::BSEventNotifyControl::kContinue;
        }
    };

}  // namespace

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);

    SKSE::GetMessagingInterface()->RegisterListener(
        [](SKSE::MessagingInterface::Message* a_msg) {
            if (a_msg->type == SKSE::MessagingInterface::kPostLoadGame ||
                a_msg->type == SKSE::MessagingInterface::kNewGame) {

                auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
                if (holder) {
                    holder->AddEventSink(EquipEventSink::GetSingleton());
                    logger::info("FPTorchFix: equip sink registered");
                } else {
                    logger::info("FPTorchFix: failed to get event source holder");
                }
            }
        });

    return true;
}