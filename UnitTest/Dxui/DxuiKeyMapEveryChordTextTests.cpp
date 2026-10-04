#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiKeyMapEveryChordTextTests
//
//  A command bound to more than one chord shows every one of them in its menu
//  row and tooltip, in table order and joined by "or", so a second binding is
//  never hidden behind the first.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiKeyMapEveryChordText
{
    static constexpr int            kEveryChordStepBack = 301;
    static constexpr int            kEveryChordPause    = 302;
    static constexpr int            kEveryChordRun      = 303;
    static constexpr DxuiKeyStroke  kEveryChordCtrlR    = { 'R', true, false, false };



    static const DxuiKeyChord  s_kEveryChordTable[] =
    {
        { VK_F5,     false, false, false, kEveryChordRun                         },
        { VK_F5,     false, false, true,  kEveryChordPause                       },
        { VK_CANCEL, true,  false, false, kEveryChordPause                       },
        { VK_F11,    false, true,  false, kEveryChordStepBack                    },
        { VK_F11,    false, false, false, kEveryChordStepBack, kEveryChordCtrlR  },
    };





    TEST_CLASS (DxuiKeyMapEveryChordTextTests)
    {
    public:

        TEST_METHOD (TwoChordsShowBothJoinedByOr)
        {
            DxuiKeyMap  map (L"Test", s_kEveryChordTable);



            Assert::AreEqual (std::wstring (L"Alt+F11 or Ctrl+R, F11"), map.GetChordText (kEveryChordStepBack));
        }


        TEST_METHOD (CtrlBreakIsWrittenAsBreak)
        {
            DxuiKeyMap  map (L"Test", s_kEveryChordTable);



            Assert::AreEqual (std::wstring (L"Shift+F5 or Ctrl+Break"), map.GetChordText (kEveryChordPause));
        }


        TEST_METHOD (OneChordShowsItAlone)
        {
            DxuiKeyMap  map (L"Test", s_kEveryChordTable);



            Assert::AreEqual (std::wstring (L"F5"), map.GetChordText (kEveryChordRun));
        }
    };
}
