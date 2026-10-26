#include "Pch.h"
#include "Seams/Win32ShellIcons.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellIconsTests
//
//  Which items look like every other file with their extension, so the
//  larger icons are found by type instead of by opening the file.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ShellIconsTests)
{
public:

    TEST_METHOD (AnOrdinaryFileIsFoundByItsType)
    {
        Assert::IsTrue  (Win32ShellIcons::IsFoundByType (L"C:\\Disks\\Game.dsk", false), L"an ordinary file");
        Assert::IsFalse (Win32ShellIcons::IsFoundByType (L"C:\\Disks", true),             L"a folder can have its own icon");
        Assert::IsFalse (Win32ShellIcons::IsFoundByType (L"C:\\Tools\\Casso.exe", false), L"so can a program");
        Assert::IsFalse (Win32ShellIcons::IsFoundByType (L"C:\\Disks\\README", false),    L"a file with no extension has no type");
    }
};
