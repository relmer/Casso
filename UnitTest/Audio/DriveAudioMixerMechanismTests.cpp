#include "Pch.h"
#include "Machines/Apple2/Common/Disk2AudioSource.h"
#include "Audio/DriveAudioMixer.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveAudioMixerMechanismTests
//
//  Spec 005-disk-ii-audio Phase 14 (T143). Validates the runtime
//  mechanism dropdown's underlying state machine: bad input must not
//  mutate state (SC-010 "no state-change on validation failure"), and a
//  mechanism set before the asset context is set is stored for the first
//  load. The cases that reload real WAVs are in the scenario suite's
//  DriveAudioMixerMechanismTests.
//
////////////////////////////////////////////////////////////////////////////////






TEST_CLASS (DriveAudioMixerMechanismTests)
{
public:

    TEST_METHOD (SetMechanism_invalidName_returnsFailureNoStateChange)
    {
        DriveAudioMixer  mixer;

        Assert::AreEqual (std::wstring (L"Shugart"), std::wstring (mixer.GetMechanism()),
            L"Default mechanism must be Shugart (SA400)");

        HRESULT  hr = mixer.SetMechanism (L"Sony");

        Assert::AreEqual (HRESULT (E_INVALIDARG), hr,
            L"Bogus mechanism name must return E_INVALIDARG");
        Assert::AreEqual (std::wstring (L"Shugart"), std::wstring (mixer.GetMechanism()),
            L"Mechanism must not change on invalid input (SC-010)");
    }


    TEST_METHOD (SetMechanism_beforeSetSampleLoadContext_recordsButDoesNotLoad)
    {
        // The shell calls SetMechanism on the CPU thread before
        // SetSampleLoadContext when WASAPI is uninitialized. That
        // path must not crash and must record the chosen mechanism
        // so the eventual first load uses it.
        DriveAudioMixer  mixer;

        HRESULT  hr = mixer.SetMechanism (L"Alps");

        Assert::AreEqual (HRESULT (S_OK), hr,
            L"Pre-context SetMechanism must return S_OK without loading");
        Assert::AreEqual (std::wstring (L"Alps"), std::wstring (mixer.GetMechanism()),
            L"Mechanism state must persist for the eventual first load");
    }


    TEST_METHOD (SetMechanism_caseInsensitive_canonicalizesToMixedCase)
    {
        // Persisted prefs use the lower-case token ("alps"/"shugart");
        // the boot path hands that straight to SetMechanism. Matching
        // must be case-insensitive and the stored mechanism must be the
        // canonical mixed-case name so LoadSamples finds the right subdir
        // (regression: a lower-case token used to fail IsValidMechanism,
        // leaving the mixer stuck on the Shugart default).
        DriveAudioMixer  mixer;

        Assert::IsTrue (mixer.IsValidMechanism (L"alps"),  L"'alps' must validate");
        Assert::IsTrue (mixer.IsValidMechanism (L"SHUGART"), L"'SHUGART' must validate");
        Assert::IsFalse (mixer.IsValidMechanism (L"sony"), L"unknown names stay invalid");

        Assert::AreEqual (HRESULT (S_OK), mixer.SetMechanism (L"alps"),
            L"Lower-case 'alps' must be accepted");
        Assert::AreEqual (std::wstring (L"Alps"), std::wstring (mixer.GetMechanism()),
            L"Mechanism must be stored canonical mixed-case");

        Assert::AreEqual (HRESULT (S_OK), mixer.SetMechanism (L"shugart"),
            L"Lower-case 'shugart' must be accepted");
        Assert::AreEqual (std::wstring (L"Shugart"), std::wstring (mixer.GetMechanism()),
            L"Mechanism must be stored canonical mixed-case");
    }
};
