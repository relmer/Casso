#include "Pch.h"

#include "InMemoryFileSystem.h"

#include "Config/MachineInputPrefs.h"
#include "Config/UserConfigStore.h"
#include "Controllers/ControllerTokens.h"
#include "Machines/MachineDefinitions.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"

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


    TEST_METHOD (Multiplayer_ReadsBothPlayersIncludingPaddlesATwoAxisMachineLacks)
    {
        JsonValue         doc      = ParseOrFail (R"({"$cassoUiPrefs":{"multiplayer":{"enabled":true,"players":[
            {"controller":"xinput","maps":"paddle0"},
            {"controller":"dinput:231d:0121/guid:{B}","maps":"joystick1"}
        ]}}})");
        MultiplayerSetup  expected;
        MultiplayerSetup  readBack = MachineInputPrefs::ReadMultiplayer (GetUiPrefsOrFail (doc));
        ControllerUnitKey xbox;

        xbox.model.kind = ControllerKind::XInput;

        expected.isEnabled         = true;
        expected.players[0].unit   = xbox;
        expected.players[0].target = PlayerAxisTarget::Paddle0;
        expected.players[1].unit   = MakeStickUnit ("{B}");
        expected.players[1].target = PlayerAxisTarget::Joystick1;

        Assert::IsTrue (readBack == expected, L"both players come back, PDL2 and PDL3 included");
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


    //  The setting is global now, so nothing writes the per-machine key: the
    //  input entries a machine's block is saved with leave it out whatever
    //  the pointer mapping.
    TEST_METHOD (GamePortAdapter_TheMachinesInputEntriesNeverWriteIt)
    {
        for (InputMappingMode pointer : { InputMappingMode::Off, InputMappingMode::Joystick, InputMappingMode::Paddle, InputMappingMode::Mouse })
        {
            for (const std::pair<std::string, JsonValue> & entry : MachineInputPrefs::BuildUiPrefEntries (pointer))
            {
                Assert::AreNotEqual (std::string (MachineInputPrefs::kpszGamePortAdapterKey), entry.first);
            }
        }
    }


    TEST_METHOD (GamePortAdapter_EachMachineAdoptsOnlyItsOwnSavedValue)
    {
        //  What the one-time adoption at launch reads, from blocks an older
        //  build wrote. The //e saved a Joyport; the ][+ saved nothing; the
        //  //c's block claims one it cannot have, as a hand edit might.
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

        //  What an older build wrote for the machine. For the //c this stands
        //  in for a hand edit: no build wrote the key for a machine without a
        //  Joyport.
        hr = store.Load (machine, defaultJson, fs, merged);
        Assert::IsTrue (SUCCEEDED (hr), L"Load");

        updated = UserConfigStore::SpliceUiPrefs (merged,
            { { MachineInputPrefs::kpszGamePortAdapterKey, JsonValue (std::string (token)) } });

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

    //
    //  The one-time adoption of the launched machine's selection
    //

    // The players the adoption makes of a $cassoUiPrefs block, and its last
    // holders.
    static PlayerEntries AdoptFrom (const std::string & uiPrefsText, PlayerLastHolders & lastHolders)
    {
        std::string  text = "{\"$cassoUiPrefs\":" + uiPrefsText + "}";
        JsonValue    doc  = ParseOrFail (text.c_str());

        return MachineInputPrefs::ReadAdoptedPlayers (GetUiPrefsOrFail (doc), lastHolders);
    }


    static std::string MakeSlotJson (const char * unitId, const char * maps)
    {
        return std::string ("{\"controller\":\"dinput:231d:0121/guid:") + unitId + "\",\"maps\":\"" + maps + "\"}";
    }


    TEST_METHOD (Adoption_ArrowKeysBecomePlayerOnesKeys)
    {
        PlayerLastHolders  lastHolders;
        PlayerEntries      entries = AdoptFrom (R"({"arrowsToJoystick":true})", lastHolders);

        Assert::AreEqual ((int) PlayerEntryKind::ArrowKeys, (int) entries[0].kind);
        Assert::AreEqual ((int) PlayerEntryKind::Automatic, (int) entries[1].kind);
        Assert::IsFalse  (lastHolders[0].has_value());
        Assert::IsFalse  (lastHolders[1].has_value());
    }


    TEST_METHOD (Adoption_MouseAsPaddleBecomesPlayerOnesMouseUnlessTheKeysTookIt)
    {
        PlayerLastHolders  lastHolders;
        PlayerEntries      mouse = AdoptFrom (R"({"pointerMapping":"paddle"})", lastHolders);
        PlayerEntries      both  = AdoptFrom (R"({"arrowsToJoystick":true,"pointerMapping":"paddle"})", lastHolders);

        Assert::AreEqual ((int) PlayerEntryKind::MousePaddle, (int) mouse[0].kind);
        Assert::AreEqual ((int) PlayerEntryKind::ArrowKeys,   (int) both[0].kind, L"the keys come first");
    }


    TEST_METHOD (Adoption_TheIouMouseStaysWithTheMachine)
    {
        PlayerLastHolders  lastHolders;
        PlayerEntries      entries = AdoptFrom (R"({"pointerMapping":"mouse"})", lastHolders);

        Assert::AreEqual ((int) PlayerEntryKind::Automatic, (int) entries[0].kind, L"the //c's own mouse is not a player's entry");
    }


    TEST_METHOD (Adoption_ASavedControllerIsOnlyPlayerOnesLastHolder)
    {
        PlayerLastHolders  lastHolders;
        PlayerEntries      entries = AdoptFrom (R"({"controller":"dinput:231d:0121/guid:{A}"})", lastHolders);

        Assert::AreEqual ((int) PlayerEntryKind::Automatic, (int) entries[0].kind, L"Player 1 stays on Automatic");
        Assert::IsFalse  (entries[0].unit.has_value(), L"the saved controller is no pick");
        Assert::IsTrue   (lastHolders[0] == MakeStickUnit ("{A}"), L"it is Player 1's last holder");
        Assert::IsFalse  (lastHolders[1].has_value(), L"and Player 2 has none");

        entries = AdoptFrom (R"({"controller":"dinput:231d:0121/guid:{A}","arrowsToJoystick":true})", lastHolders);

        Assert::AreEqual ((int) PlayerEntryKind::ArrowKeys, (int) entries[0].kind, L"the keys still become Player 1's entry");
        Assert::IsTrue   (lastHolders[0] == MakeStickUnit ("{A}"));

        entries = AdoptFrom (R"({"controller":"nonsense"})", lastHolders);

        Assert::IsFalse (lastHolders[0].has_value(), L"an unreadable controller is no last holder");
    }


    TEST_METHOD (Adoption_AnEnabledTwoPlayerBlockGivesBothPlayersPicksAndOutranksTheKeys)
    {
        PlayerLastHolders  lastHolders;
        PlayerEntries      entries = AdoptFrom (
            R"({"arrowsToJoystick":true,"controller":"dinput:231d:0121/guid:{C}","multiplayer":{"enabled":true,"players":[)" +
            MakeSlotJson ("{A}", "paddle0") + "," + MakeSlotJson ("{B}", "paddle1") + "]}}", lastHolders);

        Assert::AreEqual ((int) PlayerEntryKind::Controller, (int) entries[0].kind, L"Player 1's slot outranks the keys");
        Assert::IsTrue   (entries[0].unit == MakeStickUnit ("{A}"));
        Assert::IsTrue   (entries[0].mode == PlayerMode::Paddle, L"a single paddle there is Paddle mode");
        Assert::AreEqual ((int) PlayerEntryKind::Controller, (int) entries[1].kind);
        Assert::IsTrue   (entries[1].unit == MakeStickUnit ("{B}"));
        Assert::IsTrue   (entries[1].mode == PlayerMode::Paddle);
        Assert::IsTrue   (lastHolders[0] == MakeStickUnit ("{C}"), L"the saved controller is still only the last holder");
        Assert::IsFalse  (lastHolders[1].has_value());
    }


    TEST_METHOD (Adoption_AnEnabledBlockWithAnEmptyFirstSlotLeavesPlayerOneTheKeys)
    {
        PlayerLastHolders  lastHolders;
        PlayerEntries      entries = AdoptFrom (
            R"({"arrowsToJoystick":true,"multiplayer":{"enabled":true,"players":[{"controller":"","maps":"joystick0"},)" +
            MakeSlotJson ("{B}", "joystick1") + "]}}", lastHolders);

        Assert::AreEqual ((int) PlayerEntryKind::ArrowKeys,  (int) entries[0].kind);
        Assert::AreEqual ((int) PlayerEntryKind::Controller, (int) entries[1].kind);
        Assert::IsTrue   (entries[1].unit == MakeStickUnit ("{B}"));
    }


    TEST_METHOD (Adoption_ADisabledOrAbsentBlockLeavesPlayerTwoOnAutomatic)
    {
        PlayerLastHolders  lastHolders;
        PlayerEntries      disabled = AdoptFrom (
            R"({"multiplayer":{"enabled":false,"players":[)" +
            MakeSlotJson ("{A}", "joystick0") + "," + MakeSlotJson ("{B}", "joystick1") + "]}}", lastHolders);
        PlayerEntries      absent   = AdoptFrom (R"({"controller":"dinput:231d:0121/guid:{A}"})", lastHolders);

        Assert::IsTrue   (disabled == PlayerEntries(), L"a block turned off gives no picks");
        Assert::AreEqual ((int) PlayerEntryKind::Automatic, (int) absent[1].kind);
    }


    TEST_METHOD (Adoption_NothingSavedIsAutomaticForBoth)
    {
        PlayerLastHolders  lastHolders;
        PlayerEntries      empty = AdoptFrom ("{}", lastHolders);

        Assert::IsTrue  (empty == PlayerEntries());
        Assert::IsFalse (lastHolders[0].has_value());

        lastHolders[1] = MakeStickUnit ("{STALE}");

        Assert::IsTrue  (MachineInputPrefs::ReadAdoptedPlayers (nullptr, lastHolders) == PlayerEntries(), L"no block at all");
        Assert::IsFalse (lastHolders[1].has_value(), L"the last holders start empty");
    }


    TEST_METHOD (Adoption_ReadsTheMachinesKeysAndWritesNone)
    {
        std::string        text   = std::string (R"({"$cassoUiPrefs":{"arrowsToJoystick":true,"pointerMapping":"paddle",)") +
                                    R"("controller":"dinput:231d:0121/guid:{C}","multiplayer":{"enabled":true,"players":[)" +
                                    MakeSlotJson ("{A}", "joystick0") + "," + MakeSlotJson ("{B}", "joystick1") + "]}}}";
        JsonValue          doc    = ParseOrFail (text.c_str());
        std::string        before = JsonWriter::Write (doc);
        PlayerLastHolders  lastHolders;

        MachineInputPrefs::ReadAdoptedPlayers (GetUiPrefsOrFail (doc), lastHolders);

        Assert::AreEqual (before, JsonWriter::Write (doc), L"the machine's controller, two-player block and mappings stay as they were");
    }
};
