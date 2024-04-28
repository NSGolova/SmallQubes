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

#include "beatsaber-hook/shared/config/rapidjson-utils.hpp"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/filereadstream.h"

#include "UnityEngine/GameObject.hpp"

#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML.hpp"

using namespace GlobalNamespace;
using namespace UnityEngine;

// Called at the early stages of game loading
MOD_EXPORT void setup(CModInfo *info) noexcept
{
    *info = modInfo.to_c();

    getModConfig().Init(modInfo);

    Paper::Logger::RegisterFileContextId(SQLogger.tag);

    SQLogger.info("Completed setup!");
}

UnityEngine::Transform *GetSubcontainer(UnityEngine::UI::VerticalLayoutGroup *vertical)
{
    auto horizontal = BSML::Lite::CreateHorizontalLayoutGroup(vertical);
    horizontal->GetComponent<UnityEngine::UI::LayoutElement *>()->set_minHeight(8);
    horizontal->set_childForceExpandHeight(true);
    horizontal->set_childAlignment(UnityEngine::TextAnchor::MiddleCenter);
    return horizontal->get_transform();
}

void GameplaySettings(GameObject *gameObject, bool firstActivation)
{
    if (firstActivation)
    {
        auto vertical = BSML::Lite::CreateVerticalLayoutGroup(gameObject->get_transform());

        BSML::Lite::CreateSliderSetting(GetSubcontainer(vertical), "Qube size", 0.01, getModConfig().QubeSize.GetValue(), 0.01, 2, [](float newValue)
                                        { getModConfig().QubeSize.SetValue(newValue); });
    }
}

void scaleCollider(CuttableBySaber *cuttable, float value)
{
    auto transform = cuttable->get_transform();

    auto localScale = transform->get_localScale();
    localScale.x *= value;
    localScale.y *= value;
    localScale.z *= value;

    transform->set_localScale(localScale);
}

MAKE_HOOK_MATCH(NoteControllerInit, &NoteController::Init, void, NoteController *self, NoteData *noteData, float worldRotation, Vector3 moveStartPos, Vector3 moveEndPos, Vector3 jumpEndPos, float moveDuration, float jumpDuration, float jumpGravity, float endRotation, float uniformScale, bool rotatesTowardsPlayer, bool useRandomRotation)
{
    float qubeSize = getModConfig().QubeSize.GetValue();
    NoteControllerInit(self, noteData, worldRotation, moveStartPos, moveEndPos, jumpEndPos, moveDuration, jumpDuration, jumpGravity, endRotation, uniformScale * qubeSize, rotatesTowardsPlayer, useRandomRotation);

    float colliderScaleBack = (1.0 / qubeSize);

    auto gameNote = il2cpp_utils::try_cast<GameNoteController>(self);
    if (gameNote != std::nullopt)
    {
        auto bigCuttable = gameNote.value()->_bigCuttableBySaberList;
        for (size_t i = 0; i < bigCuttable.size(); i++)
        {
            scaleCollider(bigCuttable[i], colliderScaleBack);
        }

        auto smallCuttable = gameNote.value()->_smallCuttableBySaberList;
        for (size_t i = 0; i < smallCuttable.size(); i++)
        {
            scaleCollider(smallCuttable[i], colliderScaleBack);
        }
    }

    auto bombNote = il2cpp_utils::try_cast<BombNoteController>(self);
    if (bombNote != std::nullopt)
    {
        scaleCollider(bombNote.value()->_cuttableBySaber, colliderScaleBack);
    }
}

// Called later on in the game loading - a good time to install function hooks
MOD_EXPORT "C" void late_load()
{
    il2cpp_functions::Init();

    bool shouldStart = false;
    FILE *fp = fopen("/sdcard/BMBFData/config.json", "r");
    rapidjson::Document config;
    if (fp != NULL)
    {
        char buf[0XFFFF];
        rapidjson::FileReadStream input(fp, buf, sizeof(buf));
        config.ParseStream(input);
        fclose(fp);
    }

    if (!config.HasParseError() && config.IsObject())
    {
        auto mods = config["Mods"].GetArray();
        shouldStart = true;
        for (int index = 0; index < (int)mods.Size(); ++index)
        {
            auto const &mod = mods[index];

            if (strcmp(mod["Id"].GetString(), "qosmetics-notes") == 0 && mod["Installed"].GetBool())
            {
                shouldStart = false;
                break;
            }
        }
    }

    if (shouldStart)
    {
        BSML::Init();
        BSML::Register::RegisterGameplaySetupTab("Small qubes", GameplaySettings);

        INSTALL_HOOK(SQLogger, NoteControllerInit);
    }
}