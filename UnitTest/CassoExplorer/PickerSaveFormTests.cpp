#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/Location.h"
#include "CassoExplorer/Picker/PickerRequest.h"
#include "CassoExplorer/Window/Managers/ExplorerPickerMode.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PickerSaveFormTests
//
//  The path a save form returns for the name typed in its box.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PickerSaveFormTests)
{
public:

    static std::wstring Make (const PickerRequest & request, const Location & location, const wchar_t * name, size_t filter = 0)
    {
        std::wstring  path;



        return ExplorerPickerMode::TryMakeSavePath (request, filter, location, name, path) ? path : std::wstring (L"<none>");
    }



    TEST_METHOD (NameIsSavedInTheFolderListed)
    {
        PickerRequest  request;
        Location       folder = Location::MakeHostFolder (L"C:\\Disks");



        Assert::AreEqual (std::wstring (L"C:\\Disks\\game.dsk"), Make (request, folder, L"game.dsk"));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\game.dsk"), Make (request, folder, L"  game.dsk  "));
    }



    TEST_METHOD (EmptyOrInvalidName_HasNoPath)
    {
        PickerRequest  request;
        Location       folder = Location::MakeHostFolder (L"C:\\Disks");



        for (const wchar_t * name : { L"", L"   ", L"a\\b.dsk", L"a/b.dsk", L"a:b", L"what?.dsk", L"a*b", L"a|b", L"a<b", L"\"a\"" })
        {
            Assert::AreEqual (std::wstring (L"<none>"), Make (request, folder, name), name);
        }
    }



    //  The Casso node's list and the stock disks' list are not folders.
    TEST_METHOD (ListThatIsNoFolder_HasNoPath)
    {
        PickerRequest  request;



        Assert::AreEqual (std::wstring (L"<none>"), Make (request, Location::MakeRoot (L"casso:"), L"game.dsk"));
        Assert::AreEqual (std::wstring (L"<none>"), Make (request, Location::MakeDiskImage (L"C:\\Disks\\a.dsk"), L"game.dsk"));
        Assert::AreEqual (std::wstring (L"<none>"), Make (request, Location(), L"game.dsk"));
    }



    TEST_METHOD (NameWithoutAnExtension_GetsTheDefault_ElseTheActiveFiltersFirst)
    {
        PickerRequest  request;
        Location       folder = Location::MakeHostFolder (L"C:\\Disks");



        request.filters = { { L"WOZ disk images", L"*.woz" }, { L"DOS order", L"*.dsk;*.do" }, { L"All files", L"*.*" } };

        Assert::AreEqual (std::wstring (L"C:\\Disks\\game.woz"), Make (request, folder, L"game", 0));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\game.dsk"), Make (request, folder, L"game", 1));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\game"),     Make (request, folder, L"game", 2));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\game.po"),  Make (request, folder, L"game.po", 0), L"a typed extension stays");

        request.defaultExtension = L"wav";
        Assert::AreEqual (std::wstring (L"C:\\Disks\\tape.wav"), Make (request, folder, L"tape", 0));
    }
};
