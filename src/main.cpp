#include "main.hpp"
#include "ModConfig.hpp"

#include "config-utils/shared/config-utils.hpp"

#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/BurstSliderGameNoteController.hpp"
#include "GlobalNamespace/BombNoteController.hpp"
#include "GlobalNamespace/BoxCuttableBySaber.hpp"
#include "GlobalNamespace/CuttableBySaber.hpp"

#include "HMUI/Touchable.hpp"

#include "questui/shared/CustomTypes/Components/MainThreadScheduler.hpp"
#include "questui/shared/QuestUI.hpp"

#include "beatsaber-hook/shared/config/rapidjson-utils.hpp"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/filereadstream.h"

#include "UnityEngine/GameObject.hpp"

using namespace GlobalNamespace;
using namespace QuestUI;
using namespace UnityEngine;

ModInfo modInfo; // Stores the ID and version of our mod, and is sent to the modloader upon startup

DEFINE_CONFIG(ModConfig);

// Returns a logger, useful for printing debug messages
Logger& getLogger() {
    static Logger* logger = new Logger(modInfo, LoggerOptions(false, true));
    return *logger;
}

// Called at the early stages of game loading
extern "C" void setup(ModInfo& info) {
    info.id = ID;
    info.version = VERSION;
    modInfo = info;
	
    getModConfig().Init(modInfo); // Load the config file
    getLogger().info("Completed setup!");
}

UnityEngine::Transform* GetSubcontainer(UnityEngine::UI::VerticalLayoutGroup* vertical) {
    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(vertical);
    horizontal->GetComponent<UnityEngine::UI::LayoutElement*>()->set_minHeight(8);
    horizontal->set_childForceExpandHeight(true);
    horizontal->set_childAlignment(UnityEngine::TextAnchor::MiddleCenter);
    return horizontal->get_transform();
}

void GameplaySettings(GameObject* gameObject, bool firstActivation) {
    if (firstActivation) {
        auto vertical = BeatSaberUI::CreateVerticalLayoutGroup(gameObject->get_transform());

        BeatSaberUI::CreateSliderSetting(GetSubcontainer(vertical), "Qube size", 0.01, getModConfig().QubeSize.GetValue(), 0.01, 2, [](float newValue) {
            getModConfig().QubeSize.SetValue(newValue);
        });
    }
}

void scaleCollider(CuttableBySaber *cuttable, float value) {
    auto transform = cuttable->get_transform();

    auto localScale = transform->get_localScale();
    localScale.x *= value;
    localScale.y *= value;
    localScale.z *= value;

    transform->set_localScale(localScale);
}

MAKE_HOOK_MATCH(NoteControllerInit, &NoteController::Init, void, NoteController* self, NoteData* noteData, float worldRotation, Vector3 moveStartPos, Vector3 moveEndPos, Vector3 jumpEndPos, float moveDuration, float jumpDuration, float jumpGravity, float endRotation, float uniformScale, bool rotatesTowardsPlayer, bool useRandomRotation) {
    float qubeSize = getModConfig().QubeSize.GetValue();
    NoteControllerInit(self, noteData, worldRotation, moveStartPos, moveEndPos, jumpEndPos, moveDuration, jumpDuration, jumpGravity, endRotation, uniformScale * qubeSize, rotatesTowardsPlayer, useRandomRotation);

    float colliderScaleBack = (1.0 / qubeSize);

    auto gameNote = il2cpp_utils::try_cast<GameNoteController>(self);
    if (gameNote != std::nullopt) {
        auto bigCuttable = gameNote.value()->bigCuttableBySaberList;
        for (size_t i = 0; i < bigCuttable.Length(); i++)
        {
            scaleCollider(bigCuttable[i], colliderScaleBack);
        }

        auto smallCuttable = gameNote.value()->smallCuttableBySaberList;
        for (size_t i = 0; i < smallCuttable.Length(); i++)
        {
            scaleCollider(smallCuttable[i], colliderScaleBack);
        }
    }

    auto bombNote = il2cpp_utils::try_cast<BombNoteController>(self);
    if (bombNote != std::nullopt) {
        scaleCollider(bombNote.value()->cuttableBySaber, colliderScaleBack);
    }
}

// Called later on in the game loading - a good time to install function hooks
extern "C" void load() {
    il2cpp_functions::Init();

    bool shouldStart = false;
    FILE *fp = fopen("/sdcard/BMBFData/config.json", "r");
    rapidjson::Document config;
    if (fp != NULL) {
        char buf[0XFFFF];
        rapidjson::FileReadStream input(fp, buf, sizeof(buf));
        config.ParseStream(input);
        fclose(fp);
    }

    if (!config.HasParseError() && config.IsObject()) {
        auto mods = config["Mods"].GetArray();
        shouldStart = true;
        for (int index = 0; index < (int)mods.Size(); ++index)
        {
            auto const& mod = mods[index];
            
            if (strcmp(mod["Id"].GetString(), "qosmetics-notes") == 0 && mod["Installed"].GetBool()) {
                shouldStart = false;
                break;
            }
        }
    }

    if (shouldStart) {
        LoggerContextObject logger = getLogger().WithContext("load");

        QuestUI::Init();
        QuestUI::Register::RegisterGameplaySetupMenu(modInfo, "Small qubes", QuestUI::Register::MenuType::All, GameplaySettings);

        getLogger().info("Installing main hooks...");
        
        INSTALL_HOOK(logger, NoteControllerInit);

        getLogger().info("Installed main hooks!");
    }
}