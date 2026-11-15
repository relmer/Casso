#include "Pch.h"
#include "../EhmTestHelper.h"

#include "Config/FileAssociations.h"
#include "Config/ShellExtensionRegistration.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeUserClasses
//
//  The user's file types in memory: each key's values by name, the default
//  under the empty name. A key exists while it is in the map.
//
////////////////////////////////////////////////////////////////////////////////

class FakeUserClasses : public IUserClasses
{
public:
    using Values = std::map<std::wstring, std::wstring>;

    bool GetValue (const std::wstring & key, const std::wstring & name, std::wstring & outValue) const override
    {
        auto  found = keys.find (key);
        bool  has   = found != keys.end() && found->second.contains (name);

        outValue = has ? found->second.at (name) : std::wstring();
        return has;
    }

    HRESULT SetValue (const std::wstring & key, const std::wstring & name, const std::wstring & value) override
    {
        keys[key][name] = value;
        return S_OK;
    }

    HRESULT DeleteValue (const std::wstring & key, const std::wstring & name) override
    {
        auto  found = keys.find (key);

        if (found != keys.end())
        {
            found->second.erase (name);
        }

        return S_OK;
    }

    HRESULT DeleteKey (const std::wstring & key) override
    {
        std::erase_if (keys, [&key] (const auto & entry) { return entry.first == key || entry.first.starts_with (key + L"\\"); });
        return S_OK;
    }

    bool IsEmpty (const std::wstring & key) const override
    {
        auto  found    = keys.find (key);
        bool  children = std::any_of (keys.begin(), keys.end(), [&key] (const auto & entry) { return entry.first.starts_with (key + L"\\"); });

        return (found == keys.end() || found->second.empty()) && !children;
    }

    void NotifyChanged() override { notified++; }

    std::map<std::wstring, Values>  keys;
    int                             notified = 0;
};





TEST_CLASS (FileAssociationsTests)
{
public:

    TEST_METHOD (Register_TakesEveryTypeWithIconOpenAndOpenWith)
    {
        FakeUserClasses  classes;
        std::wstring     value;

        AssertSucceeded (FileAssociations::Register (classes, L"C:\\Casso\\Casso.exe", L"C:\\Casso\\CassoExplorer.exe"));

        Assert::IsTrue (FileAssociations::IsRegistered (classes, L"C:\\Casso\\Casso.exe", L"C:\\Casso\\CassoExplorer.exe"));
        Assert::IsTrue (classes.notified > 0, L"File Explorer is told to redraw its icons");

        Assert::IsTrue (classes.GetValue (L"Casso.DiskImage.woz\\shell\\open\\command", L"", value));
        Assert::AreEqual (std::wstring (L"\"C:\\Casso\\Casso.exe\" --disk1 \"%1\""), value);

        Assert::IsTrue (classes.GetValue (L"Applications\\CassoExplorer.exe\\shell\\open\\command", L"", value));
        Assert::AreEqual (std::wstring (L"\"C:\\Casso\\CassoExplorer.exe\" \"%1\""), value);
        Assert::IsTrue (classes.GetValue (L"Applications\\CassoExplorer.exe", L"FriendlyAppName", value));
        Assert::AreEqual (std::wstring (L"Casso Explorer"), value);

        Assert::IsTrue (classes.GetValue (L"Casso.DiskImage.woz\\DefaultIcon", L"", value));
        Assert::AreEqual (std::wstring (L"\"C:\\Casso\\Casso.exe\",-") + std::to_wstring (FileAssociations::kDiskIconId), value);

        //  Each type has the text Casso Explorer's Type column has for it.
        Assert::IsTrue (classes.GetValue (L"Casso.DiskImage.woz", L"", value));
        Assert::AreEqual (std::wstring (L"WOZ disk image"), value);
        Assert::IsTrue (classes.GetValue (L"Casso.DiskImage.po", L"", value));
        Assert::AreEqual (std::wstring (L"ProDOS-order disk image"), value);
        Assert::IsTrue (classes.GetValue (L".po", L"", value));
        Assert::AreEqual (std::wstring (L"Casso.DiskImage.po"), value);

        //  Casso's is the one type offered, so Windows opens the files with
        //  it and no prompt; Casso Explorer is under Open with as a program.
        for (const wchar_t * extension : FileAssociations::kExtensions)
        {
            Assert::IsTrue  (classes.GetValue (std::wstring (extension) + L"\\OpenWithList\\CassoExplorer.exe", L"", value),
                             L"Casso Explorer is under Open with for every type");
            Assert::IsFalse (classes.GetValue (std::wstring (extension) + L"\\OpenWithProgids", L"CassoExplorer.DiskImage", value),
                             L"and is not a second type");
        }
    }



    //  A copy of Casso in another folder registered the types, so they open
    //  in that copy rather than this one.
    TEST_METHOD (IsRegistered_IsFalseForAnotherCopysRegistration)
    {
        FakeUserClasses  classes;

        AssertSucceeded (FileAssociations::Register (classes, L"C:\\Old\\Casso.exe", L"C:\\Old\\CassoExplorer.exe"));

        Assert::IsTrue  (FileAssociations::IsRegistered (classes, L"C:\\Old\\Casso.exe",   L"C:\\Old\\CassoExplorer.exe"));
        Assert::IsFalse (FileAssociations::IsRegistered (classes, L"C:\\Casso\\Casso.exe", L"C:\\Casso\\CassoExplorer.exe"));
    }



    //  An earlier build's single type for every extension is replaced by one
    //  type each, and what the earlier build wrote doesn't count as the
    //  program the type had.
    TEST_METHOD (Register_ReplacesAnEarlierBuildsSingleType)
    {
        FakeUserClasses  classes;
        FakeUserClasses  fileExts;
        std::wstring     value;

        classes.SetValue (L"Casso.DiskImage", L"", L"Apple II disk image");
        classes.SetValue (L".dsk", L"", L"Casso.DiskImage");
        classes.SetValue (L".dsk\\OpenWithProgids", L"Casso.DiskImage", L"");

        AssertSucceeded (FileAssociations::Register (classes, L"C:\\Casso\\Casso.exe", L"C:\\Casso\\CassoExplorer.exe"));

        Assert::IsFalse (classes.keys.contains (L"Casso.DiskImage"));
        Assert::IsFalse (classes.GetValue (L".dsk", FileAssociations::kPreviousName, value));
        Assert::IsFalse (classes.GetValue (L".dsk\\OpenWithProgids", L"Casso.DiskImage", value));

        AssertSucceeded (FileAssociations::Unregister (classes, fileExts));
        Assert::IsTrue (classes.keys.empty(), L"Nothing Casso wrote is left");
    }



    //  A registration an earlier build made, with Casso Explorer as a type of
    //  its own, is removed in full.
    TEST_METHOD (Unregister_RemovesAnEarlierBuildsExplorerType)
    {
        FakeUserClasses  classes;
        FakeUserClasses  fileExts;

        AssertSucceeded (FileAssociations::Register (classes, L"C:\\Casso\\Casso.exe", L"C:\\Casso\\CassoExplorer.exe"));

        classes.SetValue (L"CassoExplorer.DiskImage\\shell\\open\\command", L"", L"\"C:\\Casso\\CassoExplorer.exe\" \"%1\"");
        classes.SetValue (L".do\\OpenWithProgids", L"CassoExplorer.DiskImage", L"");

        AssertSucceeded (FileAssociations::Unregister (classes, fileExts));

        Assert::IsTrue (classes.keys.empty(), L"Nothing Casso wrote is left");
    }



    TEST_METHOD (Unregister_LeavesTheUsersTypesAsTheyWere)
    {
        FakeUserClasses  classes;
        FakeUserClasses  fileExts;
        FakeUserClasses  before;
        FakeUserClasses  beforeExts;

        //  One type another program owns, with its own Open with entry; the
        //  rest absent. File Explorer's record lists the programs the user
        //  picked and another type.
        classes.SetValue  (L".dsk", L"", L"OtherEmulator.Disk");
        classes.SetValue  (L".dsk\\OpenWithProgids", L"OtherEmulator.Disk", L"");
        fileExts.SetValue (L".dsk\\OpenWithList", L"a", L"OtherEmulator.exe");
        fileExts.SetValue (L".dsk\\OpenWithProgids", L"OtherEmulator.Disk", L"");
        before.keys     = classes.keys;
        beforeExts.keys = fileExts.keys;

        AssertSucceeded (FileAssociations::Register (classes, L"C:\\Casso\\Casso.exe", L"C:\\Casso\\CassoExplorer.exe"));
        AssertSucceeded (FileAssociations::Register (classes, L"C:\\Casso\\Casso.exe", L"C:\\Casso\\CassoExplorer.exe"));

        //  File Explorer adds Casso's type to its record as the files are
        //  opened.
        fileExts.SetValue (L".dsk\\OpenWithProgids", L"Casso.DiskImage.dsk", L"");
        fileExts.SetValue (L".do\\OpenWithProgids", L"Casso.DiskImage.do", L"");

        AssertSucceeded (FileAssociations::Unregister (classes, fileExts));

        Assert::IsFalse (FileAssociations::IsRegistered (classes, L"C:\\Casso\\Casso.exe", L"C:\\Casso\\CassoExplorer.exe"));
        Assert::IsTrue  (classes.keys == before.keys, L"Registering twice gives the type back, and nothing Casso wrote is left");
        Assert::IsTrue  (fileExts.keys == beforeExts.keys, L"File Explorer's record lists Casso's type nowhere");
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  ShellExtensionRegistrationTests
//
//  CassoShellExtension.dll's per-user registration, in the fake registry
//  above: the class with its DLL and threading model, a handler entry for
//  folders and drives, and all of it removed again.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ShellExtensionRegistrationTests)
{
public:

    static constexpr const wchar_t *  kDll   = L"C:\\Casso\\CassoShellExtension.dll";
    static constexpr const wchar_t *  kClass = L"CLSID\\{8E3B6C41-2F7A-4D59-B1C8-6A0E9D24F713}";


    TEST_METHOD (Register_WritesTheClassAndAHandlerForFoldersAndDrives)
    {
        FakeUserClasses  classes;
        std::wstring     value;

        AssertSucceeded (ShellExtensionRegistration::Register (classes, kDll));

        Assert::IsTrue   (classes.GetValue (std::wstring (kClass) + L"\\InprocServer32", L"", value));
        Assert::AreEqual (std::wstring (kDll), value);
        Assert::IsTrue   (classes.GetValue (std::wstring (kClass) + L"\\InprocServer32", L"ThreadingModel", value));
        Assert::AreEqual (std::wstring (L"Apartment"), value);

        Assert::IsTrue   (classes.GetValue (L"Directory\\shellex\\DragDropHandlers\\CassoExplorer", L"", value));
        Assert::AreEqual (std::wstring (L"{8E3B6C41-2F7A-4D59-B1C8-6A0E9D24F713}"), value);
        Assert::IsTrue   (classes.GetValue (L"Drive\\shellex\\DragDropHandlers\\CassoExplorer", L"", value));

        Assert::IsTrue   (ShellExtensionRegistration::IsRegistered (classes, kDll));
        Assert::IsTrue   (classes.notified > 0, L"NotifyChanged is called, for File Explorer to reread the handlers");
    }


    TEST_METHOD (IsRegistered_OnlyForThisDll)
    {
        FakeUserClasses  classes;

        Assert::IsFalse (ShellExtensionRegistration::IsRegistered (classes, kDll));

        AssertSucceeded (ShellExtensionRegistration::Register (classes, L"D:\\Old\\CassoShellExtension.dll"));
        Assert::IsFalse (ShellExtensionRegistration::IsRegistered (classes, kDll), L"another copy's registration is not this one's");
        Assert::IsTrue  (ShellExtensionRegistration::IsRegistered (classes, L"d:\\old\\cassoshellextension.dll"), L"paths compare without case");

        classes.DeleteKey (L"Drive\\shellex\\DragDropHandlers\\CassoExplorer");
        Assert::IsFalse (ShellExtensionRegistration::IsRegistered (classes, L"D:\\Old\\CassoShellExtension.dll"), L"a missing handler entry is not registered");
    }


    TEST_METHOD (Unregister_RemovesEveryKeyRegisterWrote)
    {
        FakeUserClasses  classes;

        classes.SetValue (L"Directory\\shellex\\DragDropHandlers\\WinRAR", L"", L"{B41DB860-64E4-11D2-9906-E49FADC173CA}");

        AssertSucceeded (ShellExtensionRegistration::Register (classes, kDll));
        AssertSucceeded (ShellExtensionRegistration::Unregister (classes));

        Assert::IsTrue  (classes.IsEmpty (kClass));
        Assert::IsTrue  (classes.IsEmpty (L"Directory\\shellex\\DragDropHandlers\\CassoExplorer"));
        Assert::IsTrue  (classes.IsEmpty (L"Drive\\shellex\\DragDropHandlers\\CassoExplorer"));
        Assert::IsFalse (classes.IsEmpty (L"Directory\\shellex\\DragDropHandlers\\WinRAR"), L"another program's handler stays");
        Assert::IsFalse (ShellExtensionRegistration::IsRegistered (classes, kDll));
    }
};