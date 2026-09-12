#include "Pch.h"

#include "Config/MachineInputPrefs.h"

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


    TEST_METHOD (ReadProfileName_AbsentMeansDefault)
    {
        JsonValue  doc = ParseOrFail (R"({"$cassoUiPrefs":{"controller":"xinput"}})");

        Assert::IsTrue (MachineInputPrefs::ReadProfileName (GetUiPrefsOrFail (doc)).empty(),
            L"no stored profile means Default, which is spelled as nothing stored");
    }
};
