#include "Pch.h"

#include "InMemoryFileSystem.h"

#include "Config/MachineInputPrefs.h"
#include "Config/UserConfigStore.h"
#include "Controllers/ControllerTokens.h"
#include "Machines/MachineDefinitions.h"

#include "Core/JsonParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineInputPrefsTests
//
//  The per-machine input mapping: what a machine's $cassoUiPrefs block
//  resolves to, and what gets written back.
//
//  THE SEED FALLBACK IS THE UPGRADE PATH. The mapping was global through
//  1.22, so a machine with no stored value of its own has to answer with the
//  old global setting rather than with Off -- otherwise every machine loses
//  the setting on the launch after the upgrade, which looks exactly like the
//  bug this move was meant to fix.
//
//  Paddle resolving to Off is a rule, not a rounding: restoring a mouse-
//  capture mode would light the indicator while the pointer is not captured.
//  Joystick on the pointer axis is simply not an answer to that question.
//
//  The token round trip is covered because prefs store NAMES, not ordinals --
//  the whole reason the conversion exists rather than a cast.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineInputPrefsTests)
{
public:

    static JsonValue ParseOrFail (const char * text)
    {
        JsonValue        v;
        JsonParseError   err;
        HRESULT          hr = JsonParser::Parse (text, v, err);

        Assert::IsTrue (SUCCEEDED (hr), L"fixture JSON did not parse");
        return v;
    }


    // The $cassoUiPrefs sub-object of a fixture document. The returned
    // pointer aliases into `doc`, so `doc` must outlive every use.
    static const JsonValue * GetUiPrefsOrFail (const JsonValue & doc)
    {
        const JsonValue *  uiPrefs = nullptr;
        bool               found   = doc.HasObject ("$cassoUiPrefs", uiPrefs);

        Assert::IsTrue (found && uiPrefs != nullptr, L"fixture has no $cassoUiPrefs block");
        return uiPrefs;
    }


    TEST_METHOD (ReadFromUiPrefs_NullBlock_UsesTheSeeds)
    {
        bool              arrows  = false;
        InputMappingMode  pointer = InputMappingMode::Off;


        MachineInputPrefs::ReadFromUiPrefs (nullptr, InputMappingMode::Mouse,
                                            arrows, pointer);

        Assert::IsFalse (arrows, L"arrows-to-joystick is never resumed");
        Assert::IsTrue  (pointer == InputMappingMode::Mouse);
    }


    TEST_METHOD (ReadFromUiPrefs_EmptyBlock_UsesTheSeeds)
    {
        JsonValue         doc     = ParseOrFail ("{\"$cassoUiPrefs\":{\"colorMode\":\"green\"}}");
        bool              arrows  = false;
        InputMappingMode  pointer = InputMappingMode::Off;


        MachineInputPrefs::ReadFromUiPrefs (GetUiPrefsOrFail (doc),
                                            InputMappingMode::Mouse, arrows, pointer);

        Assert::IsFalse (arrows, L"arrows-to-joystick is never resumed");
        Assert::IsTrue  (pointer == InputMappingMode::Mouse);
    }


    TEST_METHOD (ReadFromUiPrefs_StoredValues_BeatTheSeeds)
    {
        JsonValue         doc     = ParseOrFail (
            "{\"$cassoUiPrefs\":{\"arrowsToJoystick\":false,\"pointerMapping\":\"off\"}}");
        bool              arrows  = true;
        InputMappingMode  pointer = InputMappingMode::Mouse;


        // Both stored values happen to be the falsy ones, which is the case
        // that matters: a machine deliberately turned OFF must not inherit
        // the global setting back.
        MachineInputPrefs::ReadFromUiPrefs (GetUiPrefsOrFail (doc),
                                            InputMappingMode::Mouse, arrows, pointer);

        Assert::IsFalse (arrows);
        Assert::IsTrue (pointer == InputMappingMode::Off);
    }


    TEST_METHOD (ReadFromUiPrefs_OneKeyStored_SeedsOnlyTheOther)
    {
        JsonValue         doc     = ParseOrFail ("{\"$cassoUiPrefs\":{\"pointerMapping\":\"mouse\"}}");
        bool              arrows  = false;
        InputMappingMode  pointer = InputMappingMode::Off;


        MachineInputPrefs::ReadFromUiPrefs (GetUiPrefsOrFail (doc),
                                            InputMappingMode::Off, arrows, pointer);

        Assert::IsFalse (arrows);                                 // never resumed
        Assert::IsTrue  (pointer == InputMappingMode::Mouse);     // stored
    }


    TEST_METHOD (ReadFromUiPrefs_StoredArrowsToJoystick_StillStartsOff)
    {
        JsonValue         doc     = ParseOrFail (
            "{\"$cassoUiPrefs\":{\"arrowsToJoystick\":true,\"pointerMapping\":\"off\"}}");
        bool              arrows  = false;
        InputMappingMode  pointer = InputMappingMode::Off;


        MachineInputPrefs::ReadFromUiPrefs (GetUiPrefsOrFail (doc),
                                            InputMappingMode::Off, arrows, pointer);

        //  The mode takes X and Z for the fire buttons, so resuming it leaves
        //  a machine where two letter keys quietly do not type. Saved, but
        //  turned on by hand in the session that plays -- the same rule the
        //  pointer axis already applies to Paddle.
        Assert::IsFalse (arrows, L"a stored arrows-to-joystick is not resumed at launch");
    }


    TEST_METHOD (ReadFromUiPrefs_StoredPaddle_ResolvesToOff)
    {
        JsonValue         doc     = ParseOrFail ("{\"$cassoUiPrefs\":{\"pointerMapping\":\"paddle\"}}");
        bool              arrows  = false;
        InputMappingMode  pointer = InputMappingMode::Mouse;


        MachineInputPrefs::ReadFromUiPrefs (GetUiPrefsOrFail (doc),
                                            InputMappingMode::Off, arrows, pointer);

        Assert::IsTrue (pointer == InputMappingMode::Off);
    }


    TEST_METHOD (ReadFromUiPrefs_SeededPaddle_ResolvesToOff)
    {
        bool              arrows  = false;
        InputMappingMode  pointer = InputMappingMode::Mouse;


        // The pre-1.23 global prefs could hold Paddle, so the seed needs the
        // same downgrade the stored value gets.
        MachineInputPrefs::ReadFromUiPrefs (nullptr, InputMappingMode::Paddle,
                                            arrows, pointer);

        Assert::IsTrue (pointer == InputMappingMode::Off);
    }


    TEST_METHOD (ReadFromUiPrefs_JoystickOnThePointerAxis_ResolvesToOff)
    {
        JsonValue         doc     = ParseOrFail ("{\"$cassoUiPrefs\":{\"pointerMapping\":\"joystick\"}}");
        bool              arrows  = false;
        InputMappingMode  pointer = InputMappingMode::Mouse;


        MachineInputPrefs::ReadFromUiPrefs (GetUiPrefsOrFail (doc),
                                            InputMappingMode::Off, arrows, pointer);

        Assert::IsTrue (pointer == InputMappingMode::Off);
    }


    TEST_METHOD (ReadFromUiPrefs_UnknownToken_KeepsTheSeed)
    {
        JsonValue         doc     = ParseOrFail ("{\"$cassoUiPrefs\":{\"pointerMapping\":\"trackball\"}}");
        bool              arrows  = false;
        InputMappingMode  pointer = InputMappingMode::Off;


        // A value written by a newer build must not silently disable the
        // mapping the current one was using.
        MachineInputPrefs::ReadFromUiPrefs (GetUiPrefsOrFail (doc),
                                            InputMappingMode::Mouse, arrows, pointer);

        Assert::IsTrue (pointer == InputMappingMode::Mouse);
    }


    TEST_METHOD (BuildUiPrefEntries_WritesOnlyWhatIsReadBack)
    {
        std::vector<std::pair<std::string, JsonValue>>  entries =
            MachineInputPrefs::BuildUiPrefEntries (InputMappingMode::Mouse);

        Assert::AreEqual (static_cast<size_t> (1), entries.size(),
            L"arrows-to-joystick is not resumed, so it is not stored");
        Assert::AreEqual (std::string (MachineInputPrefs::kpszPointerKey), entries[0].first);
    }


    TEST_METHOD (BuildUiPrefEntries_APointerModeThatHoldsThePointerIsStoredAsOff)
    {
        std::vector<std::pair<std::string, JsonValue>>  entries =
            MachineInputPrefs::BuildUiPrefEntries (InputMappingMode::Paddle);

        Assert::AreEqual (std::string ("off"), entries[0].second.GetString(),
            L"a file claiming paddle mode would describe a machine that never comes up that way");
    }


    TEST_METHOD (BuildUiPrefEntries_RoundTripsThroughReadFromUiPrefs)
    {
        JsonValue         uiPrefs (MachineInputPrefs::BuildUiPrefEntries (InputMappingMode::Mouse));
        bool              arrows  = true;
        InputMappingMode  pointer = InputMappingMode::Off;


        MachineInputPrefs::ReadFromUiPrefs (&uiPrefs, InputMappingMode::Off, arrows, pointer);

        //  What is written comes back unchanged, which is the whole point of
        //  writing only what is read.
        Assert::IsFalse (arrows);
        Assert::IsTrue  (pointer == InputMappingMode::Mouse);
    }


    TEST_METHOD (ModeTokens_RoundTripForEveryMode)
    {
        const InputMappingMode  modes[] = { InputMappingMode::Off,
                                            InputMappingMode::Joystick,
                                            InputMappingMode::Paddle,
                                            InputMappingMode::Mouse };
        size_t                  count   = sizeof (modes) / sizeof (modes[0]);
        size_t                  i       = 0;
        std::string             token;


        Assert::AreEqual (size_t (4), count);

        for (i = 0; i < count; ++i)
        {
            token = MachineInputPrefs::ModeToToken (modes[i]);

            Assert::IsFalse (token.empty());
            Assert::IsTrue (MachineInputPrefs::ModeFromToken (token, InputMappingMode::Paddle)
                                == modes[i]);
        }
    }


    TEST_METHOD (ModeFromToken_EmptyOrUnknown_ReturnsTheFallback)
    {
        Assert::IsTrue (MachineInputPrefs::ModeFromToken ("", InputMappingMode::Mouse)
                            == InputMappingMode::Mouse);
        Assert::IsTrue (MachineInputPrefs::ModeFromToken ("JOYSTICK", InputMappingMode::Mouse)
                            == InputMappingMode::Mouse);
    }


    TEST_METHOD (ReadControllerToken_AbsentKey_ReadsAsNoChoice)
    {
        JsonValue  doc = ParseOrFail (R"({"$cassoUiPrefs":{"arrowsToJoystick":true}})");

        Assert::IsTrue (MachineInputPrefs::ReadControllerToken (GetUiPrefsOrFail (doc)).empty(),
            L"a machine that has never chosen a controller reads as no choice");
        Assert::IsTrue (MachineInputPrefs::ReadControllerToken (nullptr).empty(),
            L"and so does a machine with no block at all");
    }


    TEST_METHOD (ReadControllerToken_StoredToken_ComesBackWhole)
    {
        JsonValue  doc = ParseOrFail (R"({"$cassoUiPrefs":{"controller":"dinput:231d:0121/guid:{01661270}"}})");

        Assert::AreEqual (std::string ("dinput:231d:0121/guid:{01661270}"),
                          MachineInputPrefs::ReadControllerToken (GetUiPrefsOrFail (doc)));
    }


    TEST_METHOD (ControllerEntries_RoundTripThroughRead)
    {
        std::vector<std::pair<std::string, JsonValue>>  entries =
            MachineInputPrefs::BuildControllerEntries ("xinput", "Lode Runner");
        JsonValue                                       uiPrefs (std::move (entries));

        Assert::AreEqual (std::string ("xinput"),      MachineInputPrefs::ReadControllerToken (&uiPrefs));
        Assert::AreEqual (std::string ("Lode Runner"), MachineInputPrefs::ReadProfileName (&uiPrefs));
    }


    TEST_METHOD (ControllerEntries_EmptyTokenIsStillWritten)
    {
        std::vector<std::pair<std::string, JsonValue>>  entries =
            MachineInputPrefs::BuildControllerEntries ("", "");

        // An absent key means the machine never chose, and the policy may
        // choose for it. An empty string means the user turned the controller
        // off in favor of the arrows, and choosing again would undo that.
        Assert::AreEqual (size_t (1), entries.size(), L"the controller key is written even when it is empty");
        Assert::AreEqual (std::string (MachineInputPrefs::kpszControllerKey), entries[0].first);
    }


    static ControllerUnitKey MakeStickUnit (const char * unitId)
    {
        ControllerUnitKey  unit;

        unit.model  = { ControllerKind::DirectInput, 0x231d, 0x0121 };
        unit.unitId = unitId;
        unit.source = ControllerUnitSource::InstanceGuid;
        return unit;
    }


    TEST_METHOD (Multiplayer_AbsentKeyReadsAsSingleSource)
    {
        JsonValue         doc   = ParseOrFail (R"({"$cassoUiPrefs":{"controller":"xinput"}})");
        MultiplayerSetup  setup = MachineInputPrefs::ReadMultiplayer (GetUiPrefsOrFail (doc));

        // A file written before two people could play keeps its meaning: the
        // saved controller drives PDL0 and PDL1 on its own.
        Assert::IsFalse (setup.isEnabled);
        Assert::IsFalse (setup.players[0].unit.has_value());
        Assert::IsFalse (MachineInputPrefs::ReadMultiplayer (nullptr).isEnabled);
    }


    TEST_METHOD (Multiplayer_RoundTripIncludingPaddlesATwoAxisMachineLacks)
    {
        MultiplayerSetup  setup;
        MultiplayerSetup  readBack;
        ControllerUnitKey xbox;

        xbox.model.kind = ControllerKind::XInput;

        setup.isEnabled         = true;
        setup.players[0].unit   = xbox;
        setup.players[0].target = PlayerAxisTarget::Paddle0;
        setup.players[1].unit   = MakeStickUnit ("{B}");
        setup.players[1].target = PlayerAxisTarget::Joystick1;

        std::vector<std::pair<std::string, JsonValue>>  entries;

        entries.push_back (MachineInputPrefs::BuildMultiplayerEntry (setup));

        JsonValue  uiPrefs (std::move (entries));

        readBack = MachineInputPrefs::ReadMultiplayer (&uiPrefs);

        Assert::IsTrue (readBack == setup, L"both players come back, PDL2 and PDL3 included");
    }


    TEST_METHOD (Multiplayer_OffIsStillWrittenWithBothSlots)
    {
        std::pair<std::string, JsonValue>    entry   = MachineInputPrefs::BuildMultiplayerEntry ({});
        const JsonValue                    * players = nullptr;

        // The block is spliced key by key, so leaving the key out would leave
        // a setup the user turned off in the file.
        Assert::AreEqual (std::string (MachineInputPrefs::kpszMultiplayerKey), entry.first);
        Assert::IsTrue   (entry.second.GetType() == JsonType::Object);
        Assert::IsTrue    (entry.second.HasArray ("players", players));
        Assert::IsNotNull (players);
        Assert::AreEqual (size_t (2), players->GetArraySize(), L"both slots are written, empty or not");
    }


    TEST_METHOD (Multiplayer_UnreadableSlotsAreLeftEmptyAndAnOverlapIsRefused)
    {
        JsonValue         doc   = ParseOrFail (R"({"$cassoUiPrefs":{"multiplayer":{"enabled":true,"players":[
            {"controller":"xinput","maps":"joystick0"},
            {"controller":"not a token","maps":"joystick1"}
        ]}}})");
        MultiplayerSetup  setup = MachineInputPrefs::ReadMultiplayer (GetUiPrefsOrFail (doc));

        Assert::IsTrue  (setup.isEnabled,                     L"the mode itself is still on");
        Assert::IsTrue  (setup.players[0].unit.has_value(),   L"player one is readable and plays");
        Assert::IsFalse (setup.players[1].unit.has_value(),   L"an unreadable token leaves that slot empty rather than dropping the block");

        JsonValue  clash = ParseOrFail (R"({"$cassoUiPrefs":{"multiplayer":{"enabled":true,"players":[
            {"controller":"xinput","maps":"joystick0"},
            {"controller":"dinput:231d:0121/guid:{01661270}","maps":"paddle1"}
        ]}}})");

        setup = MachineInputPrefs::ReadMultiplayer (GetUiPrefsOrFail (clash));

        Assert::IsFalse (setup.players[1].unit.has_value(),
            L"a hand-edited file claiming one paddle for both players is normalized, not played");
    }


    TEST_METHOD (Multiplayer_AnUnknownTargetTokenPlaysJoystick0)
    {
        JsonValue         doc   = ParseOrFail (R"({"$cassoUiPrefs":{"multiplayer":{"enabled":true,"players":[
            {"controller":"xinput","maps":"joystick9"}
        ]}}})");
        MultiplayerSetup  setup = MachineInputPrefs::ReadMultiplayer (GetUiPrefsOrFail (doc));

        // Every machine with a game port has PDL0 and PDL1, so a target a
        // newer build wrote degrades to one this machine can play.
        Assert::AreEqual ((int) PlayerAxisTarget::Joystick0, (int) setup.players[0].target);
        Assert::AreEqual (std::string ("paddle2"), std::string (MachineInputPrefs::TargetToToken (PlayerAxisTarget::Paddle2)));
        Assert::AreEqual ((int) PlayerAxisTarget::Paddle3,
                          (int) MachineInputPrefs::TargetFromToken ("paddle3", PlayerAxisTarget::Joystick0));
    }


    TEST_METHOD (ReadProfileName_AbsentMeansDefault)
    {
        JsonValue  doc = ParseOrFail (R"({"$cassoUiPrefs":{"controller":"xinput"}})");

        Assert::IsTrue (MachineInputPrefs::ReadProfileName (GetUiPrefsOrFail (doc)).empty(),
            L"no stored profile means Default, which is spelled as nothing stored");
    }


    TEST_METHOD (GamePortAdapter_ReadsTheJoyportWhereTheMachineCanTakeIt)
    {
        JsonValue  joyport = ParseOrFail (R"({"$cassoUiPrefs":{"gamePortAdapter":"siriusJoyport"}})");
        JsonValue  unknown = ParseOrFail (R"({"$cassoUiPrefs":{"gamePortAdapter":"atariAdapter"}})");
        JsonValue  absent  = ParseOrFail (R"({"$cassoUiPrefs":{}})");

        Assert::IsTrue (MachineInputPrefs::ReadGamePortAdapter (GetUiPrefsOrFail (joyport), true)  == GamePortAdapter::SiriusJoyport);
        Assert::IsTrue (MachineInputPrefs::ReadGamePortAdapter (GetUiPrefsOrFail (unknown), true)  == GamePortAdapter::None, L"an unknown token is None");
        Assert::IsTrue (MachineInputPrefs::ReadGamePortAdapter (GetUiPrefsOrFail (absent),  true)  == GamePortAdapter::None, L"no key is None");
        Assert::IsTrue (MachineInputPrefs::ReadGamePortAdapter (nullptr,                    true)  == GamePortAdapter::None, L"no block is None");
        Assert::IsTrue (MachineInputPrefs::ReadGamePortAdapter (GetUiPrefsOrFail (joyport), false) == GamePortAdapter::None,
            L"a machine with no annunciators reads None whatever its file says");
    }


    TEST_METHOD (GamePortAdapter_TheEntryReadsBackAsWritten)
    {
        for (GamePortAdapter adapter : { GamePortAdapter::None, GamePortAdapter::SiriusJoyport })
        {
            std::pair<std::string, JsonValue>  entry = MachineInputPrefs::BuildGamePortAdapterEntry (adapter);
            JsonValue                          block = JsonValue (std::vector<std::pair<std::string, JsonValue>> { entry });

            Assert::AreEqual (std::string ("gamePortAdapter"), entry.first);
            Assert::IsTrue (MachineInputPrefs::ReadGamePortAdapter (&block, true) == adapter);
        }
    }


    TEST_METHOD (GamePortAdapter_EachMachineAdoptsOnlyItsOwnSavedValue)
    {
        //  SC-007: what the cold-boot and machine-switch paths adopt. The //e
        //  saved a Joyport; the ][+ saved nothing; the //c's block claims one
        //  it cannot have, as a hand edit might.
        InMemoryFileSystem  fs;
        UserConfigStore     store (L"C:\\Casso\\User");
        JsonValue           defaultJson = ParseOrFail (R"({"$cassoMachineVersion":1})");

        SaveBlock (fs, store, defaultJson, "Apple2e", "siriusJoyport");
        SaveBlock (fs, store, defaultJson, "Apple2c", "siriusJoyport");

        Assert::IsTrue (AdoptedBy (fs, store, defaultJson, "Apple2e")    == GamePortAdapter::SiriusJoyport, L"the //e comes back with it");
        Assert::IsTrue (AdoptedBy (fs, store, defaultJson, "Apple2Plus") == GamePortAdapter::None,          L"the ][+ does not");
        Assert::IsTrue (AdoptedBy (fs, store, defaultJson, "Apple2c")    == GamePortAdapter::None,          L"nor can the //c");
    }


private:

    static void SaveBlock (InMemoryFileSystem & fs, UserConfigStore & store, const JsonValue & defaultJson,
                           const std::string & machine, const char * token)
    {
        JsonValue  merged;
        JsonValue  updated;
        HRESULT    hr      = S_OK;

        //  For the //c this stands in for a hand edit: the shell never writes
        //  the key for a machine without a Joyport.
        hr = store.Load (machine, defaultJson, fs, merged);
        Assert::IsTrue (SUCCEEDED (hr), L"Load");

        updated = UserConfigStore::SpliceUiPrefs (merged,
            { MachineInputPrefs::BuildGamePortAdapterEntry (ControllerTokens::GamePortAdapterFromToken (token)) });

        hr = store.SaveDelta (machine, updated, defaultJson, fs);
        Assert::IsTrue (SUCCEEDED (hr), L"SaveDelta");
    }


    static GamePortAdapter AdoptedBy (InMemoryFileSystem & fs, UserConfigStore & store, const JsonValue & defaultJson,
                                      const std::string & machine)
    {
        JsonValue                  merged;
        const JsonValue          * uiPrefs    = nullptr;
        const MachineDefinition  * definition = MachineDefinitions::Find (machine);
        HRESULT                    hr         = S_OK;

        hr = store.Load (machine, defaultJson, fs, merged);
        Assert::IsTrue (SUCCEEDED (hr), L"Load");
        merged.HasObject ("$cassoUiPrefs", uiPrefs);

        return MachineInputPrefs::ReadGamePortAdapter (uiPrefs, definition != nullptr && definition->hasAnnunciators);
    }
};
