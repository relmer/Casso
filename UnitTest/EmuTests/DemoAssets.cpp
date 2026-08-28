#include "Pch.h"

#include "DemoAssets.h"

#include "CppUnitTest.h"




//  The module this code was linked into, whatever it ends up called.
//
//  GetModuleHandle(nullptr) IS THE WRONG ANSWER HERE. It hands back the
//  EXECUTABLE, which for a test run is the test host, and the resources are in
//  the test assembly beside this translation unit. __ImageBase is the linker's
//  own symbol for the image containing it, which is exactly the one wanted and
//  needs no name to look up.
extern "C" IMAGE_DOS_HEADER  __ImageBase;





////////////////////////////////////////////////////////////////////////////////
//
//  DemoAssets::Bytes
//
//  One embedded payload, as a view into the loaded module image.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const Byte> DemoAssets::Bytes (int resourceId)
{
    HMODULE   module = reinterpret_cast<HMODULE> (&__ImageBase);
    HRSRC     found  = FindResourceW (module, MAKEINTRESOURCEW (resourceId), RT_RCDATA);
    HGLOBAL   loaded = nullptr;
    DWORD     size   = 0;
    void    * data   = nullptr;



    Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsNotNull (found,
        L"a demo asset is missing from the test assembly; DemoAssets.rc did not compile it in");

    size   = SizeofResource (module, found);
    loaded = LoadResource (module, found);
    data   = (loaded != nullptr) ? LockResource (loaded) : nullptr;

    Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue (size > 0 && data != nullptr,
        L"a demo asset is embedded but empty");

    return std::span<const Byte> (static_cast<const Byte *> (data), static_cast<size_t> (size));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DemoAssets::Text
//
//  The same payload as a string, for the sources the test assembles.
//
////////////////////////////////////////////////////////////////////////////////

std::string DemoAssets::Text (int resourceId)
{
    std::span<const Byte>  bytes = Bytes (resourceId);



    return std::string (reinterpret_cast<const char *> (bytes.data()), bytes.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DemoAssets::Copy
//
//  The same payload in a vector, for a caller that wants to own it.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> DemoAssets::Copy (int resourceId)
{
    std::span<const Byte>  bytes = Bytes (resourceId);



    return std::vector<Byte> (bytes.begin(), bytes.end());
}
