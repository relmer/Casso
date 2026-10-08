#include "Pch.h"
#include "../EhmTestHelper.h"

#include "Config/FileAssociations.h"

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

        Assert::IsTrue (FileAssociations::IsRegistered (classes));
        Assert::IsTrue (classes.notified > 0, L"File Explorer is told to redraw its icons");

        Assert::IsTrue (classes.GetValue (L"Casso.DiskImage\\shell\\open\\command", L"", value));
        Assert::AreEqual (std::wstring (L"\"C:\\Casso\\Casso.exe\" --disk1 \"%1\""), value);

        Assert::IsTrue (classes.GetValue (L"CassoExplorer.DiskImage\\shell\\open\\command", L"", value));
        Assert::AreEqual (std::wstring (L"\"C:\\Casso\\CassoExplorer.exe\" \"%1\""), value);

        Assert::IsTrue (classes.GetValue (L"Casso.DiskImage\\DefaultIcon", L"", value));
        Assert::AreEqual (std::wstring (L"\"C:\\Casso\\Casso.exe\",-") + std::to_wstring (FileAssociations::kDiskIconId), value);

        for (const wchar_t * extension : FileAssociations::kExtensions)
        {
            Assert::IsTrue (classes.GetValue (std::wstring (extension) + L"\\OpenWithProgids", L"CassoExplorer.DiskImage", value),
                            L"Casso Explorer is under Open with for every type");
        }
    }


    TEST_METHOD (Unregister_LeavesTheUsersTypesAsTheyWere)
    {
        FakeUserClasses  classes;
        FakeUserClasses  before;

        //  One type another program owns, with its own Open with entry; the
        //  rest absent.
        classes.SetValue (L".dsk", L"", L"OtherEmulator.Disk");
        classes.SetValue (L".dsk\\OpenWithProgids", L"OtherEmulator.Disk", L"");
        before.keys = classes.keys;

        AssertSucceeded (FileAssociations::Register   (classes, L"C:\\Casso\\Casso.exe", L"C:\\Casso\\CassoExplorer.exe"));
        AssertSucceeded (FileAssociations::Register   (classes, L"C:\\Casso\\Casso.exe", L"C:\\Casso\\CassoExplorer.exe"));
        AssertSucceeded (FileAssociations::Unregister (classes));

        Assert::IsFalse (FileAssociations::IsRegistered (classes));
        Assert::IsTrue  (classes.keys == before.keys, L"Registering twice still gives the type back, and nothing Casso wrote is left");
    }
};
