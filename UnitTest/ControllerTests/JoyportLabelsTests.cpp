#include "Pch.h"

#include "Controllers/JoyportLabels.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportLabelsTests
//
//  While the Joyport is in effect the players are the jacks they drive:
//  Joyport left and Joyport right, with Player 2's Disabled reading Same as
//  left. With it off, the players keep their own words.
//
////////////////////////////////////////////////////////////////////////////////

namespace ControllerTests
{
    TEST_CLASS (JoyportLabelsTests)
    {
    public:

        TEST_METHOD (WithTheJoyport_ThePlayersAreTheJacks)
        {
            Assert::AreEqual (std::wstring (L"Joyport left"),                JoyportLabels::GetPlayerLabel (0, true));
            Assert::AreEqual (std::wstring (L"Joyport right"),               JoyportLabels::GetPlayerLabel (1, true));
            Assert::AreEqual (std::wstring (L"Same as left"),                JoyportLabels::GetDisabledLabel (true));
            Assert::AreEqual (std::wstring (L"Joyport left: Automatic"),     JoyportLabels::GetAutomaticRowText (0, true));
            Assert::AreEqual (std::wstring (L"Joyport right: same as left"), JoyportLabels::GetAutomaticRowText (1, true));
        }


        TEST_METHOD (WithTheJoyport_ANoticeSaysWhichJacks)
        {
            Assert::AreEqual (std::wstring (L"Joyport left and right: Pad"), JoyportLabels::DescribeAssignment (0, L"Pad", true,  true), L"a controller playing alone");
            Assert::AreEqual (std::wstring (L"Joyport left and right: Pad"), JoyportLabels::DescribeAssignment (1, L"Pad", true,  true), L"either player, playing alone");
            Assert::AreEqual (std::wstring (L"Joyport left: Pad"),           JoyportLabels::DescribeAssignment (0, L"Pad", false, true));
            Assert::AreEqual (std::wstring (L"Joyport right: Pad"),          JoyportLabels::DescribeAssignment (1, L"Pad", false, true));
        }


        TEST_METHOD (WithoutTheJoyport_ThePlayersKeepTheirOwnWords)
        {
            Assert::AreEqual (std::wstring (L"Player 1"),           JoyportLabels::GetPlayerLabel (0, false));
            Assert::AreEqual (std::wstring (L"Player 2"),           JoyportLabels::GetPlayerLabel (1, false));
            Assert::AreEqual (std::wstring (L"Disabled"),           JoyportLabels::GetDisabledLabel (false));
            Assert::AreEqual (std::wstring (L"Player 1: Automatic"), JoyportLabels::GetAutomaticRowText (0, false));
            Assert::AreEqual (std::wstring (L"Player 2: Automatic"), JoyportLabels::GetAutomaticRowText (1, false));
            Assert::AreEqual (std::wstring (L"Player 1: Pad"),      JoyportLabels::DescribeAssignment (0, L"Pad", true,  false), L"alone or not");
            Assert::AreEqual (std::wstring (L"Player 2: Pad"),      JoyportLabels::DescribeAssignment (1, L"Pad", false, false));
        }


        //  The picker's words for each state, which it is built with.
        TEST_METHOD (PickerLabels_FollowTheJoyport)
        {
            InputModeRules::PlayerLabels  on  = JoyportLabels::GetPickerLabels (true);
            InputModeRules::PlayerLabels  off = JoyportLabels::GetPickerLabels (false);



            Assert::AreEqual (std::wstring (L"Joyport left"),  on.players[0]);
            Assert::AreEqual (std::wstring (L"Joyport right"), on.players[1]);
            Assert::AreEqual (std::wstring (L"Same as left"),  on.disabled);
            Assert::AreEqual (std::wstring (L"same as left"),  on.idle[1]);
            Assert::IsTrue   (on.idle[0].empty(), L"Joyport left on Automatic still reads Automatic");

            Assert::IsTrue   (off == InputModeRules::PlayerLabels(), L"off, the picker's own words");
        }
    };
}
