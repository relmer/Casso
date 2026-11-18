#include "Pch.h"

#include "EmbeddedMachineJson.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace fs = std::filesystem;

//  The module this code is linked into, the test assembly. See DemoAssets.cpp.
extern "C" IMAGE_DOS_HEADER  __ImageBase;





////////////////////////////////////////////////////////////////////////////////
//
//  EmbeddedMachineJson::Load
//
////////////////////////////////////////////////////////////////////////////////

std::string EmbeddedMachineJson::Load (int resourceId)
{
    HMODULE       module = reinterpret_cast<HMODULE> (&__ImageBase);
    HRSRC         found  = FindResourceW (module, MAKEINTRESOURCEW (resourceId), RT_RCDATA);
    HGLOBAL       loaded = nullptr;
    DWORD         size   = 0;
    const void *  data   = nullptr;



    Assert::IsNotNull (found, L"a machine configuration is missing from the test assembly; MachineConfigs.rc did not compile it in");

    size   = SizeofResource (module, found);
    loaded = LoadResource (module, found);
    data   = (loaded != nullptr) ? LockResource (loaded) : nullptr;

    Assert::IsTrue (size > 0 && data != nullptr, L"a machine configuration is embedded but empty");

    return (data != nullptr) ? std::string (static_cast<const char *> (data), size) : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmbeddedMachineJson::LocateCassoExe
//
////////////////////////////////////////////////////////////////////////////////

fs::path EmbeddedMachineJson::LocateCassoExe()
{
    wchar_t   buf[MAX_PATH] = {};
    HMODULE   hSelf         = nullptr;
    BOOL      ok            = FALSE;
    fs::path  candidate;
    fs::path  found;



    ok = GetModuleHandleExW (
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR> (&EmbeddedMachineJson::LocateCassoExe),
        &hSelf);

    // Locate this DLL by an address inside it, then look for Casso.exe as a
    // sibling -- vstest drops both binaries in the same output folder. Any
    // step failing means "not found", which the caller reports.
    if (ok && hSelf != nullptr && GetModuleFileNameW (hSelf, buf, MAX_PATH) != 0)
    {
        candidate = fs::path (buf).parent_path() / L"Casso.exe";

        if (fs::exists (candidate))
        {
            found = candidate;
        }
    }

    return (found);
}
