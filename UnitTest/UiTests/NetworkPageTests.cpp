#include "Pch.h"

#include "Ui/Settings/NetworkPage.h"
#include "Ui/Settings/SettingsPanelState.h"

#include "Core/JsonParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkPageTests
//
//  The Network page against a real SettingsPanelState: layout inside the
//  sheet width with no overlapping controls, the staged card's settings, and
//  edits applied to the staged state.
//
////////////////////////////////////////////////////////////////////////////////

namespace NetworkPageTests
{
    static constexpr const char *  s_kpszMachine = R"JSON({
        "name": "Apple //e",
        "slots": [
            { "slot": 1, "device": "parallel-printer", "capabilityFlag": "optional" },
            { "slot": 3, "device": "uthernet2", "capabilityFlag": "optional", "enabled": true },
            { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom" }
        ]
    })JSON";

    static constexpr const char *  s_kpszCardOff = R"JSON({
        "name": "Apple //e",
        "slots": [
            { "slot": 3, "device": "uthernet2", "capabilityFlag": "optional", "enabled": false }
        ]
    })JSON";

    static constexpr RECT  s_kPageRect = { 0, 0, 760, 900 };


    static void Load (SettingsPanelState & state, const char * text)
    {
        JsonValue        json;
        JsonParseError   error;

        AssertSucceeded (JsonParser::Parse (text, json, error));
        AssertSucceeded (state.LoadFromMachine ("Custom", json, json));
    }


    static void LayOut (NetworkPage & page)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        page.Layout (s_kPageRect, scaler);
    }


    static bool Overlaps (const RECT & a, const RECT & b)
    {
        return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
    }






    TEST_CLASS (NetworkPageTests)
    {
    public:

        TEST_METHOD (ControlsFitTheSheetWithoutOverlapping)
        {
            SettingsPanelState  state;
            NetworkPage         page;

            Load (state, s_kpszMachine);
            page.SetState (&state);
            LayOut (page);

            std::vector<RECT>  rects = { page.GetMacInput().GetBounds(),      page.GetLanCheckbox().GetBounds(),
                                         page.GetForwardList().GetBounds(),   page.GetProtocolCombo().GetBounds(),
                                         page.GetHostPortInput().GetBounds(), page.GetGuestPortInput().GetBounds(),
                                         page.GetAddButton().GetBounds(),     page.GetRemoveButton().GetBounds() };
            size_t             i     = 0;
            size_t             j     = 0;

            for (i = 0; i < rects.size(); i++)
            {
                Assert::IsTrue (rects[i].right <= s_kPageRect.right && rects[i].left >= s_kPageRect.left, std::format (L"control {} inside the page", i).c_str());
                Assert::IsTrue (rects[i].right > rects[i].left && rects[i].bottom > rects[i].top, std::format (L"control {} has an area", i).c_str());

                for (j = i + 1; j < rects.size(); j++)
                {
                    Assert::IsFalse (Overlaps (rects[i], rects[j]), std::format (L"controls {} and {} overlap", i, j).c_str());
                }
            }

            Assert::IsTrue (page.GetContentHeightPx() > 0);
        }


        TEST_METHOD (LabelsAreSentenceCase)
        {
            NetworkPage  page;

            LayOut (page);

            Assert::AreEqual (L"Allow connections from other computers", page.GetLanCheckbox().GetLabel().c_str());
            Assert::AreEqual (L"Add",                                    page.GetAddButton().GetAccessibleName().c_str());
            Assert::AreEqual (L"Remove",                                 page.GetRemoveButton().GetAccessibleName().c_str());
        }


        TEST_METHOD (ACardTurnedOnInTheMachineTabShowsItsMac)
        {
            SettingsPanelState  state;
            NetworkPage         page;

            Load (state, s_kpszCardOff);
            page.SetState (&state);
            LayOut (page);

            Assert::IsTrue (page.GetMacInput().GetText().empty());

            AssertSucceeded (state.SetHardwareEnabled ((size_t) (state.GetNetworkCard() - state.GetHardware().data()), true));
            page.Sync();

            Assert::AreEqual ((size_t) 17, page.GetMacInput().GetText().size(), L"the generated MAC is shown");
        }


        TEST_METHOD (BadMacShowsTheErrorAndGoodMacClearsIt)
        {
            SettingsPanelState  state;
            NetworkPage         page;
            MacAddress          before;

            Load (state, s_kpszMachine);
            page.SetState (&state);
            LayOut (page);
            before = state.GetNetworkCard()->network.mac;

            page.GetMacInput().SetFocused (true);
            page.GetMacInput().SetText (L"01:00:5E:00:00:0");
            page.GetMacInput().OnKey ((WPARAM) VK_END);
            page.GetMacInput().OnChar (L'1');

            Assert::IsTrue (page.GetErrorText().starts_with (L"Error: invalid MAC address\n"));
            Assert::IsTrue (state.GetNetworkCard()->network.mac == before);

            page.GetMacInput().SetText (L"02:CA:55:00:00:4");
            page.GetMacInput().OnKey ((WPARAM) VK_END);
            page.GetMacInput().OnChar (L'2');

            Assert::IsTrue (page.GetErrorText().empty());
            Assert::AreEqual (std::string ("02:CA:55:00:00:42"), state.GetNetworkCard()->network.mac.Format());
        }


        TEST_METHOD (AddAndRemoveAForward)
        {
            SettingsPanelState  state;
            NetworkPage         page;
            DxuiKeyEvent        enter = {};

            Load (state, s_kpszMachine);
            page.SetState (&state);
            LayOut (page);

            page.GetHostPortInput().SetText  (L"8080");
            page.GetGuestPortInput().SetText (L"80");

            enter.vk = VK_RETURN;
            page.GetAddButton().SetFocused (true);
            page.GetAddButton().OnKey (enter);

            Assert::AreEqual ((size_t) 1, state.GetNetworkCard()->network.portForwards.size());
            Assert::AreEqual (1, page.GetForwardList().GetRowCount());
            Assert::IsTrue (page.GetRemoveButton().IsEnabled());

            page.GetForwardList().SetSelectedRow (0);
            page.GetRemoveButton().SetFocused (true);
            page.GetRemoveButton().OnKey (enter);

            Assert::AreEqual ((size_t) 0, state.GetNetworkCard()->network.portForwards.size());
            Assert::AreEqual (0, page.GetForwardList().GetRowCount());
        }


        TEST_METHOD (BadPortShowsTheError)
        {
            SettingsPanelState  state;
            NetworkPage         page;
            DxuiKeyEvent        enter = {};

            Load (state, s_kpszMachine);
            page.SetState (&state);
            LayOut (page);

            page.GetHostPortInput().SetText  (L"70000");
            page.GetGuestPortInput().SetText (L"80");

            enter.vk = VK_RETURN;
            page.GetAddButton().SetFocused (true);
            page.GetAddButton().OnKey (enter);

            Assert::IsTrue (page.GetErrorText().starts_with (L"Error: invalid port\n"));
            Assert::AreEqual ((size_t) 0, state.GetNetworkCard()->network.portForwards.size());
        }


        TEST_METHOD (SyncPicksUpEditsMadeElsewhere)
        {
            SettingsPanelState  state;
            NetworkPage         page;

            Load (state, s_kpszMachine);
            page.SetState (&state);

            state.SetNetworkLanAccess (true);
            page.Sync();

            Assert::IsTrue (page.GetLanCheckbox().IsChecked(), L"a staged change made elsewhere shows here");
        }
    };
}
