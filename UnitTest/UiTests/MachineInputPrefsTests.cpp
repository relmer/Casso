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


        MachineInputPrefs::ReadFromUiPrefs (nullptr, true, InputMappingMode::Mouse,
                                            arrows, pointer);

        Assert::IsTrue (arrows);
        Assert::IsTrue (pointer == InputMappingMode::Mouse);
    }


    TEST_METHOD (ReadFromUiPrefs_EmptyBlock_UsesTheSeeds)
    {
        JsonValue         doc     = ParseOrFail ("{\"$cassoUiPrefs\":{\"colorMode\":\"green\"}}");
        bool              arrows  = false;
        InputMappingMode  pointer = InputMappingMode::Off;


        MachineInputPrefs::ReadFromUiPrefs (GetUiPrefsOrFail (doc), true,
                                            InputMappingMode::Mouse, arrows, pointer);

        Assert::IsTrue (arrows);
        Assert::IsTrue (pointer == InputMappingMode::Mouse);
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
        MachineInputPrefs::ReadFromUiPrefs (GetUiPrefsOrFail (doc), true,
                                            InputMappingMode::Mouse, arrows, pointer);

        Assert::IsFalse (arrows);
        Assert::IsTrue (pointer == InputMappingMode::Off);
    }


    TEST_METHOD (ReadFromUiPrefs_OneKeyStored_SeedsOnlyTheOther)
    {
        JsonValue         doc     = ParseOrFail ("{\"$cassoUiPrefs\":{\"pointerMapping\":\"mouse\"}}");
        bool              arrows  = false;
        InputMappingMode  pointer = InputMappingMode::Off;


        MachineInputPrefs::ReadFromUiPrefs (GetUiPrefsOrFail (doc), true,
                                            InputMappingMode::Off, arrows, pointer);

        Assert::IsTrue (arrows);                                  // seeded
        Assert::IsTrue (pointer == InputMappingMode::Mouse);      // stored
    }


    TEST_METHOD (ReadFromUiPrefs_StoredPaddle_ResolvesToOff)
    {
        JsonValue         doc     = ParseOrFail ("{\"$cassoUiPrefs\":{\"pointerMapping\":\"paddle\"}}");
        bool              arrows  = false;
        InputMappingMode  pointer = InputMappingMode::Mouse;


        MachineInputPrefs::ReadFromUiPrefs (GetUiPrefsOrFail (doc), false,
                                            InputMappingMode::Off, arrows, pointer);

        Assert::IsTrue (pointer == InputMappingMode::Off);
    }


    TEST_METHOD (ReadFromUiPrefs_SeededPaddle_ResolvesToOff)
    {
        bool              arrows  = false;
        InputMappingMode  pointer = InputMappingMode::Mouse;


        // The pre-1.23 global prefs could hold Paddle, so the seed needs the
        // same downgrade the stored value gets.
        MachineInputPrefs::ReadFromUiPrefs (nullptr, false, InputMappingMode::Paddle,
                                            arrows, pointer);

        Assert::IsTrue (pointer == InputMappingMode::Off);
    }


    TEST_METHOD (ReadFromUiPrefs_JoystickOnThePointerAxis_ResolvesToOff)
    {
        JsonValue         doc     = ParseOrFail ("{\"$cassoUiPrefs\":{\"pointerMapping\":\"joystick\"}}");
        bool              arrows  = false;
        InputMappingMode  pointer = InputMappingMode::Mouse;


        MachineInputPrefs::ReadFromUiPrefs (GetUiPrefsOrFail (doc), false,
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
        MachineInputPrefs::ReadFromUiPrefs (GetUiPrefsOrFail (doc), false,
                                            InputMappingMode::Mouse, arrows, pointer);

        Assert::IsTrue (pointer == InputMappingMode::Mouse);
    }


    TEST_METHOD (BuildUiPrefEntries_WritesBothKeys)
    {
        std::vector<std::pair<std::string, JsonValue>>  entries =
            MachineInputPrefs::BuildUiPrefEntries (true, InputMappingMode::Mouse);


        Assert::AreEqual (size_t (2), entries.size());

        Assert::AreEqual (std::string ("arrowsToJoystick"), entries[0].first);
        Assert::IsTrue (entries[0].second.GetType() == JsonType::Bool);
        Assert::IsTrue (entries[0].second.GetBool());

        Assert::AreEqual (std::string ("pointerMapping"), entries[1].first);
        Assert::IsTrue (entries[1].second.GetType() == JsonType::String);
        Assert::AreEqual (std::string ("mouse"), entries[1].second.GetString());
    }


    TEST_METHOD (BuildUiPrefEntries_RoundTripsThroughReadFromUiPrefs)
    {
        std::vector<std::pair<std::string, JsonValue>>  entries =
            MachineInputPrefs::BuildUiPrefEntries (true, InputMappingMode::Mouse);
        JsonValue                                       uiPrefs (std::move (entries));
        bool                                            arrows  = false;
        InputMappingMode                                pointer = InputMappingMode::Off;


        MachineInputPrefs::ReadFromUiPrefs (&uiPrefs, false, InputMappingMode::Off,
                                            arrows, pointer);

        Assert::IsTrue (arrows);
        Assert::IsTrue (pointer == InputMappingMode::Mouse);
    }


    // Every mode has a token, and every token maps back. A mode added without
    // a token would serialize as "off" and silently become Off on the way
    // back in, which is the failure a sweep over the enum catches and a sweep
    // over the table cannot.
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
};
