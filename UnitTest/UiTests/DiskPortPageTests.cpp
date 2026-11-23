#include "Pch.h"

#include "Ui/Settings/DiskPortPage.h"

#include "Core/JsonParser.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPageTests
//
//  The Disk port tab on the //c and the //c Plus: a row for each drive on
//  the port, a dropdown on each row offering only the kinds that fit there,
//  and an add row that is left out when the port is full.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskPortPageTests)
{
public:

    TEST_METHOD (AnEmptyPortShowsOnlyTheAddRow)
    {
        SettingsPanelState  st;
        DiskPortPage        page;



        Load (st, "Apple2cPlus");
        page.SetState (&st);

        Assert::AreEqual ((size_t) 0, page.GetRowCount());
        Assert::IsTrue   (page.IsAddRowShown());
        Assert::IsTrue   (page.GetAddDropdown().GetItems() == std::vector<std::wstring> { L"Add a drive", L"Apple 3.5 Drive", L"UniDisk 3.5", L"5.25-inch drive" });
    }

    TEST_METHOD (TheIIcOffersNoApple35Drive)
    {
        SettingsPanelState  st;
        DiskPortPage        page;



        Load (st, "Apple2c");
        page.SetState (&st);

        Assert::IsTrue (page.GetAddDropdown().GetItems() == std::vector<std::wstring> { L"Add a drive", L"UniDisk 3.5", L"5.25-inch drive" });
    }

    TEST_METHOD (AddedDrivesAreSortedIntoChainOrder)
    {
        SettingsPanelState  st;
        DiskPortPage        page;



        Load (st, "Apple2cPlus");
        page.SetState (&st);

        page.AddDrive (3);   // 5.25-inch drive
        page.AddDrive (2);   // UniDisk 3.5
        page.AddDrive (1);   // Apple 3.5 Drive

        Assert::IsTrue  (st.GetDiskPortDrives() == std::vector<std::string> { "apple35-drive", "unidisk35", "disk-iic" });
        Assert::AreEqual ((size_t) 3, page.GetRowCount());
        Assert::IsFalse (page.IsAddRowShown(), L"no add row when the port is full");
    }

    TEST_METHOD (AUniDiskBecomesA525DriveOnlyWithoutOneOnTheChain)
    {
        SettingsPanelState  st;
        DiskPortPage        page;



        Load (st, "Apple2c");
        page.SetState (&st);

        page.AddDrive (1);   // UniDisk 3.5
        page.AddDrive (1);   // UniDisk 3.5

        Assert::IsTrue (page.GetRowDropdown (0).GetItems() == std::vector<std::wstring> { L"UniDisk 3.5", L"5.25-inch drive" });

        page.ChooseDrive (1, 1);

        Assert::IsTrue (st.GetDiskPortDrives() == std::vector<std::string> { "unidisk35", "disk-iic" });
        Assert::IsTrue (page.GetRowDropdown (0).GetItems() == std::vector<std::wstring> { L"UniDisk 3.5" }, L"the //c takes one 5.25-inch drive");
    }

    TEST_METHOD (ALone525DriveIsTheSecondDrive)
    {
        SettingsPanelState  st;
        DiskPortPage        page;



        Load (st, "Apple2c");
        page.SetState (&st);

        page.AddDrive (2);   // 5.25-inch drive

        Assert::IsTrue  (st.SecondDriveAttached(),        L"recorded as the port's device, as the second drive switch records it");
        Assert::IsTrue  (st.GetDiskPortChain().empty());
        Assert::IsFalse (st.RequiresReset(),              L"the second drive is a live setting");

        page.RemoveDrive (0);

        Assert::IsFalse (st.SecondDriveAttached());
        Assert::AreEqual ((size_t) 0, page.GetRowCount());
    }

    TEST_METHOD (AChainChangeTakesAReset)
    {
        SettingsPanelState  st;
        DiskPortPage        page;



        Load (st, "Apple2c");
        page.SetState (&st);

        page.AddDrive (1);   // UniDisk 3.5

        Assert::IsTrue (st.GetDiskPortChain() == std::vector<std::string> { "unidisk35" });
        Assert::IsTrue (st.RequiresReset());
    }

    TEST_METHOD (TheStagedDrivesSurviveTheRereadOnOk)
    {
        SettingsPanelState  st;
        DiskPortPage        page;



        Load (st, "Apple2c");
        page.SetState (&st);

        page.AddDrive (1);   // UniDisk 3.5
        st.RefreshMergedJson (st.GetDefaultJson());

        Assert::IsTrue (st.GetDiskPortChain() == std::vector<std::string> { "unidisk35" });
        Assert::IsTrue (st.RequiresReset(), L"OK rebuilds the machine");
    }


private:

    static void Load (SettingsPanelState & st, const std::string & machineId)
    {
        JsonValue       v;
        JsonParseError  e;
        std::string     text = std::format (R"JSON({{
            "$cassoMachineVersion": 1,
            "name": "{}",
            "cpu": "65C02",
            "timing": {{ "clockSpeed": 1023000 }},
            "ram": [ {{ "address": "0x0000", "size": "0xC000" }} ],
            "systemRom": {{ "address": "0xC000", "file": "x.rom", "romBankSize": "0x4000" }},
            "internalDevices": [ {{ "type": "keyboard" }} ],
            "ports": [
                {{ "name": "disk",     "device": "" }},
                {{ "name": "joystick", "device": "" }}
            ]
        }})JSON", machineId);



        AssertSucceeded (JsonParser::Parse (text, v, e), L"test fixture JSON must parse");
        AssertSucceeded (st.LoadFromMachine (machineId, v, v), L"the machine must load");
    }
};
