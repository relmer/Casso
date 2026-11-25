#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/Model/PreviewDecoder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserFormattingTests
//
//  The text of a list row and of the status bar: Content view lines, sizes,
//  dates, selection counts and location labels.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BrowserFormattingTests)
{
public:

    //  Until the shell's own lines arrive, a host file's Content row has its
    //  date and size at the right, each with its name, and no type under its
    //  name; a file in a disk image has its type there; a folder with no date
    //  has a blank first line in the right column.
    TEST_METHOD (ContentLines_DateAndSizeForAHostFileTypeForAnAppleFile)
    {
        CatalogRow                       file;
        CatalogRow                       apple;
        CatalogRow                       folder;
        std::vector<DxuiListView::Cell>  hostCells;
        std::vector<DxuiListView::Cell>  appleCells;
        std::vector<DxuiListView::Cell>  folderCells;

        file.name         = L"readme.txt";
        file.typeText     = L"Text Document";
        file.sizeBytes    = 302;
        file.hasModified  = true;
        file.modifiedUnix = 1700000000;

        apple.name     = L"HELLO";
        apple.typeText = L"Applesoft BASIC";

        folder.name        = L"Games";
        folder.isDirectory = true;

        hostCells   = BrowserCells::ToCells (file,   Location::MakeHostFolder (L"C:\\Disks"));
        appleCells  = BrowserCells::ToCells (apple,  Location::MakeDiskImage (L"C:\\Disks\\a.dsk"));
        folderCells = BrowserCells::ToCells (folder, Location::MakeHostFolder (L"C:\\Disks"));

        Assert::IsTrue   (hostCells[0].contentLeft.empty(), L"No type under a host file's name");
        Assert::AreEqual ((size_t) 2, hostCells[0].contentRight.size());
        Assert::AreEqual (std::wstring (L"Date modified:"), hostCells[0].contentRight[0].label);
        Assert::AreEqual (std::wstring (L"Size:"),          hostCells[0].contentRight[1].label);
        Assert::AreEqual (std::wstring (L"302 bytes"),      hostCells[0].contentRight[1].text, L"The value apart from its name");

        Assert::AreEqual ((size_t) 1, appleCells[0].contentLeft.size());
        Assert::AreEqual (std::wstring (L"Type:"),           appleCells[0].contentLeft[0].label);
        Assert::AreEqual (std::wstring (L"Applesoft BASIC"), appleCells[0].contentLeft[0].text);

        Assert::AreEqual ((size_t) 1, folderCells[0].contentRight.size());
        Assert::IsTrue   (folderCells[0].contentRight[0].text.empty(), L"The date's line is blank, not removed");
    }


    TEST_METHOD (LocationLabels)
    {
        Assert::AreEqual (std::wstring (L"C:\\"),    BrowserCells::GetLocationLabel (Location::MakeHostFolder (L"C:\\")));
        Assert::AreEqual (std::wstring (L"Disks"),   BrowserCells::GetLocationLabel (Location::MakeHostFolder (L"C:\\Disks\\")));
        Assert::AreEqual (std::wstring (L"SUB"),     BrowserCells::GetLocationLabel (Location::MakeDiskDirectory (L"C:\\a.po", "/CASSO/SUB")));
    }


    TEST_METHOD (Formatting)
    {
        Assert::AreEqual (std::wstring (L"777 bytes"), BrowserCells::FormatSize (777));
        Assert::AreEqual (std::wstring (L"8.00 KB"),   BrowserCells::FormatSize (8192));

        //  The Size column, as the property system formats System.Size, which
        //  varies by Windows build.
        auto shellSize = [] (uint64_t bytes)
        {
            HRESULT      hr    = S_OK;
            PROPERTYKEY  key   = {};
            PROPVARIANT  value = {};
            PWSTR        shown = nullptr;
            std::wstring text;

            value.vt             = VT_UI8;
            value.uhVal.QuadPart = bytes;

            hr = PSGetPropertyKeyFromName (L"System.Size", &key);
            Assert::IsTrue (SUCCEEDED (hr));

            hr = PSFormatForDisplayAlloc (key, value, PDFF_DEFAULT, &shown);
            Assert::IsTrue (SUCCEEDED (hr));

            text = shown;
            CoTaskMemFree (shown);
            return text;
        };

        Assert::AreEqual (shellSize (187),    BrowserCells::FormatSizeColumn (187));
        Assert::AreEqual (shellSize (5539),   BrowserCells::FormatSizeColumn (5539));
        Assert::AreEqual (shellSize (933616), BrowserCells::FormatSizeColumn (933616));
        Assert::AreEqual (std::wstring (L"1 item"),    BrowserCells::FormatSelection (0, 1));
        Assert::AreEqual (std::wstring (L"7 items, 3 selected"), BrowserCells::FormatSelection (3, 7));

        //  Explorer's selection field, the size after two spaces.
        Assert::AreEqual (std::wstring (L"1 item selected"),           BrowserCells::FormatSelected (1, 0, false));
        Assert::AreEqual (std::wstring (L"4 items selected  777 bytes"), BrowserCells::FormatSelected (4, 777, true));

        //  1984-08-17 12:34, as the scratch ProDOS volume records it.
        //  In the user's own short date and time, whatever they are set to.
        {
            SYSTEMTIME  st       = { 1984, 8, 0, 17, 12, 34, 0, 0 };
            wchar_t     date[80] = {};
            wchar_t     time[80] = {};
            int         dateLen  = GetDateFormatEx (LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, date, 80, nullptr);
            int         timeLen  = GetTimeFormatEx (LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &st, nullptr, time, 80);

            Assert::IsTrue   (dateLen > 0 && timeLen > 0);
            Assert::AreEqual (std::wstring (date) + L" " + time, BrowserCells::FormatModified (461594040, true));
        }
    }
};
