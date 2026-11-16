#include "Pch.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Ui/Settings/HardwarePage.h"
#include "Ui/Settings/SettingsJson.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SlotCards.h"
#include "SettingsRecordingSink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SlotCardsTests
//
//  The Machine tab's storage card edits: the cards it offers a machine, a
//  card put in a slot or replaced or taken out, images attached, a chain's
//  limit, and each edit written to the machine's file with a reset to
//  apply it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SlotCardsTests)
{
public:

    static constexpr const char *  kMachineJson = R"JSON({
        "$cassoMachineVersion": 1,
        "name": "Apple //e",
        "internalDevices": [ { "type": "keyboard" } ],
        "slots": [
            { "slot": 4, "device": "mockingboard-c" },
            { "slot": 6, "device": "disk-ii", "rom": "Disk2.rom", "ports": [ "disk-ii-drive", "disk-ii-drive" ] }
        ]
    })JSON";


    static JsonValue Parse (const char * text)
    {
        JsonValue       value;
        JsonParseError  error;



        AssertSucceeded (JsonParser::Parse (text, value, error));

        return value;
    }


    static std::string Write (const JsonValue & value)
    {
        std::string          text;
        JsonWriter::Options  opts;



        opts.fPretty = false;
        AssertSucceeded (JsonWriter::Write (value, opts, text));

        return text;
    }


    //  Every storage card is offered; one the machine can't host has a tip with the
    //  machines that can host it.
    TEST_METHOD (Choices_MarkTheCardsTheMachineHosts)
    {
        std::vector<SlotCardChoice>  onIIe  = SlotCards::GetChoices ("Apple2e");
        std::vector<SlotCardChoice>  onPlus = SlotCards::GetChoices ("Apple2Plus");



        Assert::AreEqual (BlockDeviceCatalog::GetCards().size() + 1, onIIe.size(), L"no card, then every card");
        Assert::IsTrue (onIIe[0].device.empty());

        for (const SlotCardChoice & choice : onIIe)
        {
            Assert::IsTrue (choice.allowed, choice.label.c_str());
        }

        for (const SlotCardChoice & choice : onPlus)
        {
            if (choice.device == kpszApple35CardDevice)
            {
                Assert::IsFalse (choice.allowed);
                Assert::IsTrue (choice.tip.find (L"Apple //e") != std::wstring::npos, choice.tip.c_str());
            }
        }
    }


    //  A card in an empty slot has its default drives, and a card the
    //  machine can't host fails with the machines that can.
    TEST_METHOD (SetCard_DefaultDrivesAndHostCheck)
    {
        std::vector<HardwareEntry>  entries;
        std::wstring                error;
        int                         index = -1;



        AssertSucceeded (SlotCards::SetCard (entries, "Apple2e", 7, kpszAppleScsiCardDevice, error));
        index = SlotCards::FindSlot (entries, 7);
        Assert::AreEqual (0, index);
        Assert::AreEqual ((size_t) 7, entries[0].ports.size());
        Assert::AreEqual (std::string (kpszHd20scDevice), entries[0].ports[6]);
        Assert::AreEqual (std::string ("Slot 7: Apple II SCSI Card"), entries[0].displayName);

        AssertSucceeded (SlotCards::SetCard (entries, "Apple2e", 5, kpszLironCardDevice, error));
        Assert::AreEqual (std::string (kpszUniDisk35Device), entries[1].ports[0]);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), SlotCards::SetCard (entries, "Apple2Plus", 4, kpszApple35CardDevice, error));
        Assert::IsTrue (error.find (L"works in") != std::wstring::npos);
    }


    TEST_METHOD (Chain_LimitAndRemove)
    {
        std::vector<HardwareEntry>  entries;
        std::wstring                error;



        AssertSucceeded (SlotCards::SetCard (entries, "Apple2e", 5, kpszLironCardDevice, error));

        for (int i = 0; i < 3; i++)
        {
            AssertSucceeded (SlotCards::AddChainDrive (entries, 0, error));
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_TOO_MANY_NAMES), SlotCards::AddChainDrive (entries, 0, error));
        Assert::IsTrue (error.find (L"at most 4") != std::wstring::npos, error.c_str());

        AssertSucceeded (SlotCards::RemoveChainDrive (entries, 0, 1));
        Assert::AreEqual ((size_t) 3, entries[0].ports.size());

        AssertSucceeded (SlotCards::AddChainDrive (entries, 0, error));
        Assert::AreEqual ((size_t) 4, entries[0].ports.size(), L"a removed drive's place can be filled again");
    }


    //  Each edit is written to the machine's file and takes a reset: a card
    //  put in an empty slot, a card replaced, an image attached, and a card
    //  taken out.
    TEST_METHOD (Edits_AreSavedAndTakeAReset)
    {
        SettingsPanelState      st;
        SettingsRecordingSink   sink;
        JsonValue               machine = Parse (kMachineJson);
        JsonValue               out;
        std::wstring            error;
        std::string             text;
        int                     liron   = -1;



        AssertSucceeded (st.LoadFromMachine ("Apple2e", machine, machine));
        Assert::IsTrue (st.IsSlotFilled (4));
        Assert::IsFalse (st.IsSlotFilled (5));

        AssertSucceeded (st.SetSlotCard (5, kpszLironCardDevice, error));
        liron = SlotCards::FindSlot (st.GetHardware(), 5);
        AssertSucceeded (st.SetPortImage ((size_t) liron, 0, "C:/disks/800k.po"));
        AssertSucceeded (st.SetSlotCard (4, kpszProFileCardDevice, error));
        AssertSucceeded (st.SetSlotCard (6, "", error));
        Assert::IsTrue (st.RequiresReset());

        st.Apply (sink, out);
        text = Write (out);

        Assert::AreEqual (1, sink.queuedResetCount);
        Assert::IsTrue (text.find (R"("slot":5,"device":"liron-card")") != std::string::npos, L"the new card");
        Assert::IsTrue (text.find (R"("image":"C:/disks/800k.po")") != std::string::npos, L"its image");
        Assert::IsTrue (text.find (R"("device":"profile-card")") != std::string::npos, L"the replacing card");
        Assert::IsTrue (text.find ("mockingboard-c") == std::string::npos, L"the replaced card is gone");
        Assert::IsTrue (text.find ("disk-ii") == std::string::npos, L"the card taken out is gone");
        Assert::IsTrue (text.find ("Disk2.rom") == std::string::npos, L"with its firmware");
    }


    //  Replacing a card in a filled slot takes a confirmation, and after
    //  Cancel the slot's card is in place.
    TEST_METHOD (Page_ConfirmsBeforeReplacingACard)
    {
        SettingsPanelState  st;
        HardwarePage        page;
        JsonValue           machine = Parse (kMachineJson);
        int                 profile = -1;



        AssertSucceeded (st.LoadFromMachine ("Apple2e", machine, machine));
        page.SetState (&st);
        page.SelectSlot (4);

        for (size_t i = 0; i < page.GetCardDropdown().GetItems().size(); i++)
        {
            if (page.GetCardDropdown().GetItems()[i] == L"ProFile interface card")
            {
                profile = (int) i;
            }
        }

        Assert::IsTrue (profile >= 0);

        page.ChooseCard (profile);
        Assert::IsTrue (page.IsReplacePending());
        Assert::IsTrue (page.GetStoragePrompt().starts_with (L"Replace the Mockingboard"), page.GetStoragePrompt().c_str());
        Assert::IsFalse (st.RequiresReset(), L"nothing changes before the confirmation");

        page.CancelReplace();
        Assert::IsFalse (page.IsReplacePending());
        Assert::AreEqual (std::string ("mockingboard-c"), st.GetHardware()[(size_t) SlotCards::FindSlot (st.GetHardware(), 4)].type);

        page.ChooseCard (profile);
        page.ConfirmReplace();
        Assert::AreEqual (std::string (kpszProFileCardDevice), st.GetHardware()[(size_t) SlotCards::FindSlot (st.GetHardware(), 4)].type);
        Assert::IsTrue (st.RequiresReset());
    }


    //  A drive selected in the tree takes an image from the file dialog, and
    //  a chain's Add drive stops at its limit with the limit in the prompt.
    TEST_METHOD (Page_AttachesAnImageAndLimitsAChain)
    {
        SettingsPanelState  st;
        HardwarePage        page;
        JsonValue           machine = Parse (kMachineJson);
        std::wstring        error;
        int                 liron   = -1;



        AssertSucceeded (st.LoadFromMachine ("Apple2e", machine, machine));
        AssertSucceeded (st.SetSlotCard (5, kpszLironCardDevice, error));
        liron = SlotCards::FindSlot (st.GetHardware(), 5);

        page.SetState (&st);
        page.SetOnBrowseImage ([] (std::string & path) { path = "C:/disks/800k.po"; return true; });
        page.SelectTreeRow (std::format (L"drive:{}:0", liron));
        Assert::AreEqual (5, page.GetSelectedSlot());
        Assert::IsTrue (page.GetAttachButton().IsEnabled());
        Assert::IsFalse (page.GetEjectButton().IsEnabled(), L"the drive is empty");

        page.AttachImage();
        Assert::AreEqual (std::string ("C:/disks/800k.po"), st.GetHardware()[(size_t) liron].portImages[0]);

        for (int i = 0; i < 4; i++)
        {
            page.AddDrive();
        }

        Assert::AreEqual ((size_t) 4, st.GetHardware()[(size_t) liron].ports.size());
        Assert::IsTrue (page.GetStoragePrompt().find (L"at most 4") != std::wstring::npos, page.GetStoragePrompt().c_str());
    }


    //  A card the machine can't host isn't put in a slot; the prompt lists
    //  the machines that can.
    TEST_METHOD (Page_BlockedCardShowsItsHosts)
    {
        SettingsPanelState  st;
        HardwarePage        page;
        JsonValue           machine = Parse (kMachineJson);



        AssertSucceeded (st.LoadFromMachine ("Apple2Plus", machine, machine));
        page.SetState (&st);
        page.SelectSlot (5);

        for (size_t i = 0; i < page.GetCardDropdown().GetItems().size(); i++)
        {
            if (!page.GetCardDropdown().IsItemEnabled ((int) i))
            {
                page.ChooseCard ((int) i);
                Assert::IsFalse (st.IsSlotFilled (5));
                Assert::IsTrue (page.GetStoragePrompt().find (L"works in") != std::wstring::npos, page.GetStoragePrompt().c_str());
                return;
            }
        }

        Assert::Fail (L"a card the ][+ can't host");
    }
};
