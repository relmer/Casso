#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyChordSequenceTests
//
//  Two-key chords, Visual Studio's Ctrl+R then F11: the first key is reported
//  as a prefix, only a chord that starts with it completes on the next key,
//  a modifier pressed alone between them leaves it waiting, and a one-key
//  chord on the same second key still works on its own.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiKeyChordSequenceTests
{
    static constexpr int            kStepInto     = 201;
    static constexpr int            kStepBackInto = 202;
    static constexpr int            kStepBackOut  = 203;
    static constexpr DxuiKeyStroke  kCtrlR        = { 'R', true, false, false };



    static const DxuiKeyChord  s_kChords[] =
    {
        { VK_F11, false, false, false, kStepInto                         },
        { VK_F11, false, false, false, kStepBackInto, kCtrlR             },
        { VK_F11, false, false, true,  kStepBackOut,  kCtrlR             },
    };





    TEST_CLASS (DxuiKeyChordSequenceTests)
    {
    public:

        TEST_METHOD (TheFirstKeyIsAPrefixAndTheSecondCompletesTheChord)
        {
            DxuiKeyMap                    map (L"Test", s_kChords);
            std::optional<DxuiKeyStroke>  pending;
            int                           id = 0;



            Assert::IsTrue (map.Match (kCtrlR, pending, id) == DxuiKeyMatch::Prefix, L"Ctrl+R starts a chord");

            pending = kCtrlR;
            Assert::IsTrue (map.Match ({ VK_F11, false, false, false }, pending, id) == DxuiKeyMatch::Command);
            Assert::AreEqual (kStepBackInto, id, L"Ctrl+R, F11");

            Assert::IsTrue (map.Match ({ VK_F11, false, false, true }, pending, id) == DxuiKeyMatch::Command);
            Assert::AreEqual (kStepBackOut, id, L"Ctrl+R, Shift+F11");
        }


        TEST_METHOD (TheSecondKeyAloneIsItsOwnChord)
        {
            DxuiKeyMap                    map (L"Test", s_kChords);
            std::optional<DxuiKeyStroke>  pending;
            int                           id = 0;



            Assert::IsTrue (map.Match ({ VK_F11, false, false, false }, pending, id) == DxuiKeyMatch::Command);
            Assert::AreEqual (kStepInto, id, L"F11 with nothing before it");

            Assert::IsTrue  (map.TryTranslate (VK_F11, false, false, false, id));
            Assert::AreEqual (kStepInto, id, L"TryTranslate reads one-key chords only");
            Assert::IsFalse (map.TryTranslate (VK_F11, false, false, true, id), L"Shift+F11 is bound only after Ctrl+R");
        }


        TEST_METHOD (AModifierWaitsAndAnUnboundKeyEndsTheChord)
        {
            DxuiKeyMap                    map (L"Test", s_kChords);
            std::optional<DxuiKeyStroke>  pending = kCtrlR;
            int                           id      = 0;



            Assert::IsTrue (map.Match ({ VK_SHIFT, false, false, true }, pending, id) == DxuiKeyMatch::Modifier, L"Shift pressed on its way to Shift+F11");
            Assert::IsTrue (map.Match ({ 'Q',      false, false, false }, pending, id) == DxuiKeyMatch::None, L"Ctrl+R, Q is nothing");
            Assert::IsTrue (map.Match ({ VK_SHIFT, false, false, true }, std::nullopt, id) == DxuiKeyMatch::None, L"Shift alone with nothing waiting");
        }


        TEST_METHOD (MenusShowBothKeysOfAChord)
        {
            static const DxuiKeyChord  kOnlyChord[] = { { VK_F10, false, false, false, kStepBackInto, kCtrlR } };
            DxuiKeyMap                 map (L"Test", kOnlyChord);



            Assert::AreEqual (std::wstring (L"Ctrl+R, F10"), map.GetChordText (kStepBackInto));
        }
    };
}