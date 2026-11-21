#include "Pch.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Ui/Settings/SlotsPage.h"
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


    //  Every slot is listed with its card, and an empty slot as empty.
    TEST_METHOD (Page_ListsEverySlotWithItsCard)
    {
        SettingsPanelState  st;
        JsonValue           machine = Parse (kMachineJson);



        AssertSucceeded (st.LoadFromMachine ("Apple2e", machine, machine));

        Assert::AreEqual (std::wstring (L"Slot 1: Empty"),  SlotsPage::MakeSlotLabel (st.GetHardware(), 1));
        Assert::AreEqual (std::wstring (L"Slot 6: Disk ]["), SlotsPage::MakeSlotLabel (st.GetHardware(), 6));
        Assert::IsTrue   (SlotsPage::MakeSlotLabel (st.GetHardware(), 4).starts_with (L"Slot 4: Mockingboard"));
    }


    //  A card chosen for a filled slot replaces the card in it at once, and
    //  is reported, so its firmware download can be offered.
    TEST_METHOD (Page_ReplacesACardAtOnce)
    {
        SettingsPanelState  st;
        SlotsPage           page;
        JsonValue           machine = Parse (kMachineJson);
        int                 profile = -1;
        std::string         chosen;



        AssertSucceeded (st.LoadFromMachine ("Apple2e", machine, machine));
        page.SetState (&st);
        page.SetOnCardChosen ([&chosen] (const std::string & device) { chosen = device; });
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
        Assert::AreEqual (std::string (kpszProFileCardDevice), st.GetHardware()[(size_t) SlotCards::FindSlot (st.GetHardware(), 4)].type);
        Assert::AreEqual (std::string (kpszProFileCardDevice), chosen);
        Assert::IsTrue   (st.RequiresReset());
    }


    //  A chain card has a number of drives connected, from none to its
    //  limit.
    TEST_METHOD (Page_SetsHowManyDrivesAChainHas)
    {
        SettingsPanelState  st;
        SlotsPage           page;
        JsonValue           machine = Parse (kMachineJson);
        std::wstring        error;
        int                 liron   = -1;



        AssertSucceeded (st.LoadFromMachine ("Apple2e", machine, machine));
        AssertSucceeded (st.SetSlotCard (5, kpszLironCardDevice, error));
        liron = SlotCards::FindSlot (st.GetHardware(), 5);

        page.SetState (&st);
        page.SelectSlot (5);
        Assert::AreEqual ((size_t) 5, page.GetCountDropdown().GetItems().size(), L"none to four drives");
        Assert::AreEqual (1, page.GetCountDropdown().GetSelectedIndex());

        page.ChooseDriveCount (3);
        Assert::AreEqual ((size_t) 3, st.GetHardware()[(size_t) liron].ports.size());

        page.ChooseDriveCount (0);
        Assert::IsTrue (st.GetHardware()[(size_t) liron].ports.empty());
        Assert::IsTrue (st.RequiresReset());
    }


    //  Each SCSI ID takes a kind of drive, or none.
    TEST_METHOD (Page_ChoosesADriveForEachScsiId)
    {
        SettingsPanelState  st;
        SlotsPage           page;
        JsonValue           machine = Parse (kMachineJson);
        std::wstring        error;
        int                 scsi    = -1;



        AssertSucceeded (st.LoadFromMachine ("Apple2e", machine, machine));
        AssertSucceeded (st.SetSlotCard (7, kpszAppleScsiCardDevice, error));
        scsi = SlotCards::FindSlot (st.GetHardware(), 7);

        page.SetState (&st);
        page.SelectSlot (7);
        Assert::AreEqual ((size_t) 7, page.GetPortRowCount());
        Assert::AreEqual (1, page.GetFirmwareChoice().GetSelected(), L"Apple's ROM");

        page.SetAppleFirmware (false);
        Assert::IsTrue   (st.GetHardware()[(size_t) scsi].cassoFirmware);
        Assert::AreEqual (0, page.GetFirmwareChoice().GetSelected(), L"Casso's fast firmware");
        Assert::AreEqual (1, page.GetPortDropdown (6).GetSelectedIndex(), L"the HD20SC is at ID 6");
        Assert::AreEqual (0, page.GetPortDropdown (2).GetSelectedIndex());

        page.ChoosePortDrive (2, 1);
        Assert::AreEqual (std::string (kpszHd20scDevice), st.GetHardware()[(size_t) scsi].ports[2]);

        page.ChoosePortDrive (6, 0);
        Assert::IsTrue (st.GetHardware()[(size_t) scsi].ports[6].empty());
    }


    //  A card the machine can't host isn't put in a slot; the prompt lists
    //  the machines that can.
    TEST_METHOD (Page_BlockedCardShowsItsHosts)
    {
        SettingsPanelState  st;
        SlotsPage           page;
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
