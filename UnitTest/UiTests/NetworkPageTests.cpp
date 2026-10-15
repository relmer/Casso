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
//  sheet width with no overlapping controls, what is live with the card out
//  and in, and edits applied to the staged state.
//
////////////////////////////////////////////////////////////////////////////////

namespace NetworkPageTests
{
    static constexpr const char *  s_kpszMachine = R"JSON({
        "name": "Apple //e",
        "slots": [
            { "slot": 1, "device": "parallel-printer", "capabilityFlag": "optional" },
            { "slot": 3, "device": "uthernet2", "capabilityFlag": "optional", "enabled": false },
            { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom" }
        ]
    })JSON";

    static constexpr const char *  s_kpszNoSlots = R"JSON({ "name": "Apple //c" })JSON";

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


    static void PressSpace (IDxuiControl & control)
    {
        DxuiKeyEvent  key = {};

        key.vk = VK_SPACE;
        control.OnFocusChanged (true);
        control.OnKey (key);
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

            std::vector<RECT>  rects = { page.GetEnableToggle().GetBounds(),  page.GetSlotCombo().GetBounds(),
                                         page.GetMacInput().GetBounds(),      page.GetLanCheckbox().GetBounds(),
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


        TEST_METHOD (OnlyTheToggleIsLiveWhileTheCardIsOut)
        {
            SettingsPanelState  state;
            NetworkPage         page;

            Load (state, s_kpszMachine);
            page.SetState (&state);

            Assert::IsTrue  (page.GetEnableToggle().IsEnabled());
            Assert::IsFalse (page.GetEnableToggle().IsChecked());
            Assert::IsFalse (page.GetSlotCombo().IsEnabled());
            Assert::IsFalse (page.GetMacInput().IsEnabled());
            Assert::IsFalse (page.GetAddButton().IsEnabled());
        }


        TEST_METHOD (TurningTheCardOnStagesItWithAMac)
        {
            SettingsPanelState  state;
            NetworkPage         page;

            Load (state, s_kpszMachine);
            page.SetState (&state);
            LayOut (page);

            PressSpace (page.GetEnableToggle());

            Assert::IsTrue (state.GetNetworkCard()->enabled);
            Assert::IsTrue (page.GetMacInput().IsEnabled());
            Assert::AreEqual ((size_t) 17, page.GetMacInput().GetText().size(), L"the generated MAC is shown");
            Assert::AreEqual (2, page.GetSlotCombo().GetSelectedIndex(), L"slot 3");
        }


        TEST_METHOD (MachineWithoutSlotsOffersNothing)
        {
            SettingsPanelState  state;
            NetworkPage         page;

            Load (state, s_kpszNoSlots);
            page.SetState (&state);

            Assert::IsFalse (page.GetEnableToggle().IsEnabled());
        }


        TEST_METHOD (BadMacShowsTheErrorAndGoodMacClearsIt)
        {
            SettingsPanelState  state;
            NetworkPage         page;
            MacAddress          before;

            Load (state, s_kpszMachine);
            page.SetState (&state);
            LayOut (page);
            PressSpace (page.GetEnableToggle());
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
            PressSpace (page.GetEnableToggle());

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
            PressSpace (page.GetEnableToggle());

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

            AssertSucceeded (state.SetNetworkCardEnabled (true));
            page.Sync();

            Assert::IsTrue (page.GetEnableToggle().IsChecked(), L"the Machine tab's toggle shows here");
        }
    };
}
