#include "Pch.h"
#include "Core/LongPath.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LongPathTests
//
//  The \\?\ form a path takes for Windows, and back.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LongPathTests)
{
public:

    TEST_METHOD (AnAbsolutePathTakesTheLocalPrefix)
    {
        Assert::AreEqual (std::wstring (L"\\\\?\\C:\\disks\\work.dsk"), LongPath::ToWin32 (std::wstring (L"C:\\disks\\work.dsk")));
    }


    //  Windows does not normalize a \\?\ path, so the conversion does.
    TEST_METHOD (ForwardSlashesAndDotSegmentsAreNormalized)
    {
        Assert::AreEqual (std::wstring (L"\\\\?\\C:\\disks\\work.dsk"), LongPath::ToWin32 (std::wstring (L"C:/disks/./old/../work.dsk")));
    }


    TEST_METHOD (ARelativePathIsMadeAbsolute)
    {
        std::wstring  current = std::filesystem::current_path().wstring();
        std::wstring  result  = LongPath::ToWin32 (std::wstring (L"work.dsk"));



        Assert::IsTrue (result.starts_with (L"\\\\?\\" + current), result.c_str());
        Assert::IsTrue (result.ends_with (L"\\work.dsk"), result.c_str());
    }


    TEST_METHOD (AShareTakesTheUncPrefix)
    {
        Assert::AreEqual (std::wstring (L"\\\\?\\UNC\\server\\share\\work.dsk"), LongPath::ToWin32 (std::wstring (L"\\\\server\\share\\work.dsk")));
    }


    TEST_METHOD (APathInADeviceFormOrEmptyIsReturnedAsGiven)
    {
        Assert::AreEqual (std::wstring (L"\\\\?\\C:\\disks\\work.dsk"), LongPath::ToWin32 (std::wstring (L"\\\\?\\C:\\disks\\work.dsk")));
        Assert::AreEqual (std::wstring (L"\\\\.\\pipe\\casso"),        LongPath::ToWin32 (std::wstring (L"\\\\.\\pipe\\casso")));
        Assert::AreEqual (std::wstring(),                              LongPath::ToWin32 (std::wstring()));
    }


    TEST_METHOD (FromWin32RemovesThePrefixToWin32Adds)
    {
        Assert::AreEqual (std::wstring (L"C:\\disks\\work.dsk"),         LongPath::FromWin32 (LongPath::ToWin32 (std::wstring (L"C:\\disks\\work.dsk"))));
        Assert::AreEqual (std::wstring (L"\\\\server\\share\\work.dsk"), LongPath::FromWin32 (LongPath::ToWin32 (std::wstring (L"\\\\server\\share\\work.dsk"))));
        Assert::AreEqual (std::wstring (L"C:\\disks\\work.dsk"),         LongPath::FromWin32 (std::wstring (L"C:\\disks\\work.dsk")));
    }
};
