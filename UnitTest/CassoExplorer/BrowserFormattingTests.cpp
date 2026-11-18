#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/Browser/BrowserPreviewer.h"
#include "CassoExplorer/Model/PreviewDecoder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserFormattingTests
//
//  The text of a list row and of the status bar: Content view lines, sizes,
//  dates, selection counts and location labels. Also the character set a PC
//  text file is detected as.
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


    TEST_METHOD (HostTextCharsets)
    {
        const Byte  ascii[]   = { 'R', 'E', 'A', 'D', '\r', '\n', '\t', 'x', 0x0C, 0x1A };
        const Byte  dos[]     = { ' ', 0xC4, 0xC4, 0xCD, 0xB3, 'Z', 0x82, '\r', '\n' };
        const Byte  windows[] = { 'c', 'a', 'f', 0xE9, ' ', 0xFC, 'r' };
        const Byte  utf8[]    = { 'c', 'a', 'f', 0xC3, 0xA9 };
        const Byte  cutOff[]  = { 'a', 0xE2, 0x80 };
        const Byte  binary[]  = { 'M', 'Z', 0x90, 0x00, 0x03 };

        Assert::IsTrue (BrowserPreviewer::GetHostCharset (ascii)   == HostCharset::Ascii,   L"Tabs, line breaks, form feeds and DOS's end mark are text");
        Assert::IsTrue (BrowserPreviewer::GetHostCharset (dos)     == HostCharset::Dos,     L"Mostly box drawing is DOS text");
        Assert::IsTrue (BrowserPreviewer::GetHostCharset (windows) == HostCharset::Windows, L"Accented letters are Windows text");
        Assert::IsTrue (BrowserPreviewer::GetHostCharset (utf8)    == HostCharset::Utf8,    L"Well-formed UTF-8 is UTF-8");
        Assert::IsTrue (BrowserPreviewer::GetHostCharset (cutOff)  == HostCharset::Utf8,    L"A sequence cut off at the end of the sample is UTF-8");
        Assert::IsTrue (BrowserPreviewer::GetHostCharset (binary)  == HostCharset::None,    L"A control code other than those is binary");
        Assert::IsTrue (BrowserPreviewer::GetHostCharset ({})      == HostCharset::None,    L"An empty file is no text");

        //  DOS text draws a control code now and then as a symbol, such as a
        //  menu's pointer; more than one byte in a hundred is no text.
        std::vector<Byte>  pointer (200, 'a');

        pointer[50] = 0x10;
        Assert::IsTrue (BrowserPreviewer::GetHostCharset (pointer) == HostCharset::Dos,     L"A DOS symbol among text is DOS text");

        pointer[51] = 0x10;
        pointer[52] = 0x11;
        Assert::IsTrue (BrowserPreviewer::GetHostCharset (pointer) == HostCharset::None,    L"but too many are binary");
    }


    TEST_METHOD (LocationLabels)
    {
        Assert::AreEqual (std::wstring (L"C:\\"),    BrowserCells::GetLocationLabel (Location::MakeHostFolder (L"C:\\")));
        Assert::AreEqual (std::wstring (L"Disks"),   BrowserCells::GetLocationLabel (Location::MakeHostFolder (L"C:\\Disks\\")));
        Assert::AreEqual (std::wstring (L"SUB"),     BrowserCells::GetLocationLabel (Location::MakeDiskDirectory (L"C:\\a.po", "/CASSQUE/SUB")));
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
