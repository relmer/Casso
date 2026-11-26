#include "Pch.h"

#include "CassoExplorer/Model/DiskMessageText.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMessageTextTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskMessageTextTests)
{
public:

    TEST_METHOD (ImageError_NamesTheFileNotItsPath)
    {
        Assert::AreEqual (std::wstring (L"a.po is empty."),
                          DiskMessageText::FormatImageError (L"C:\\Disks\\a.po", "C:\\Disks\\a.po: is empty\n"));
        Assert::AreEqual (std::wstring (L"Something else."),
                          DiskMessageText::FormatImageError (L"C:\\Disks\\a.po", "Something else"), L"A message of another form is kept");
    }



    TEST_METHOD (LinkError_DiffersForAnUnrecognizedDiskAndAnUnreadableOne)
    {
        Assert::AreEqual (std::wstring (L"The disk in this drive can't be previewed because it doesn't have a recognized file system."),
                          DiskMessageText::FormatLinkError (HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME), std::string()));
        Assert::AreEqual (std::wstring (L"This drive could not be read."),
                          DiskMessageText::FormatLinkError (HRESULT_FROM_WIN32 (ERROR_CRC), std::string()));
        Assert::AreEqual (std::wstring (L"The connection is disconnected."),
                          DiskMessageText::FormatLinkError (HRESULT_FROM_WIN32 (ERROR_NOT_CONNECTED), "The connection is disconnected."), L"A message is shown as it is");
    }



    TEST_METHOD (ShortenPaths_CutsEachPathToItsName)
    {
        Assert::AreEqual (std::wstring (L"broken.dsk: is 26 bytes but should be 143,360 bytes"),
                          DiskMessageText::ShortenPaths (L"C:\\Users\\me\\Temp\\t077\\ui\\broken.dsk: is 26 bytes but should be 143,360 bytes"));
        Assert::AreEqual (std::wstring (L"a.po: HELLO: wrote out.bin."),
                          DiskMessageText::ShortenPaths (L"C:\\My Disks\\a.po: HELLO: wrote D:\\Out Dir\\out.bin."),
                          L"Paths with spaces, mid-sentence, and before a period");
        Assert::AreEqual (std::wstring (L"one\nDisks\ntwo.dsk"),
                          DiskMessageText::ShortenPaths (L"one\nC:\\Disks\\\nE:\\x\\two.dsk"), L"A folder keeps its own name");
        Assert::AreEqual (std::wstring (L"No path at all, just a colon: see."),
                          DiskMessageText::ShortenPaths (L"No path at all, just a colon: see."));
    }
};
