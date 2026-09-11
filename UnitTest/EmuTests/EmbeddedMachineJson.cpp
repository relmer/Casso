#include "Pch.h"

#include "EmbeddedMachineJson.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  EmbeddedMachineJson::Load
//
////////////////////////////////////////////////////////////////////////////////

std::string EmbeddedMachineJson::Load (int resourceId)
{
    HMODULE       hExe    = nullptr;
    HRSRC         hRes    = nullptr;
    HGLOBAL       hMem    = nullptr;
    DWORD         size    = 0;
    const void *  data    = nullptr;
    std::string   jsonText;
    fs::path      exePath = LocateCassoExe();



    // Every guard below ends in Assert::Fail, which THROWS -- so nothing after
    // it runs and no early return is needed. The FreeLibrary calls must still
    // come BEFORE the Fail: the throw unwinds straight past this frame, so
    // anything after it would leak the module.
    if (exePath.empty())
    {
        Assert::Fail (L"Casso.exe not found next to the test DLL");
    }

    hExe = LoadLibraryExW (exePath.wstring().c_str(),
                           nullptr,
                           LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE);

    if (hExe == nullptr)
    {
        Assert::Fail (std::format (L"LoadLibraryExW failed for {}",
                                   exePath.wstring()).c_str());
    }

    hRes = FindResourceW (hExe, MAKEINTRESOURCEW (resourceId), RT_RCDATA);

    if (hRes == nullptr)
    {
        FreeLibrary (hExe);
        Assert::Fail (L"Embedded RCDATA resource not found in Casso.exe");
    }

    size = SizeofResource (hExe, hRes);

    if (size == 0)
    {
        FreeLibrary (hExe);
        Assert::Fail (L"Embedded resource is empty");
    }

    hMem = LoadResource (hExe, hRes);

    if (hMem == nullptr)
    {
        FreeLibrary (hExe);
        Assert::Fail (L"LoadResource failed");
    }

    data = LockResource (hMem);

    if (data == nullptr)
    {
        FreeLibrary (hExe);
        Assert::Fail (L"LockResource failed");
    }

    jsonText.assign (static_cast<const char *> (data), size);

    FreeLibrary (hExe);

    return (jsonText);
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
