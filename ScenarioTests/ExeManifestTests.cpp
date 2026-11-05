#include "Pch.h"
#include "EmuTests/EmbeddedMachineJson.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  ExeManifestTests
//
//  The application manifest every shipped executable embeds.
//
//  Both executables hold strings narrow, in the process code page: CassoCli's
//  argv arrives that way, and Casso converts its wide command line down to it.
//  Without activeCodePage UTF-8 that code page is the system's ANSI one, and a
//  file name with a character it cannot hold, such as Greek or CJK, reaches the
//  program with `?` in its place and is never found. The setting lives only in
//  the manifest, so the manifest is what is checked. Each executable is opened
//  as a resource-only module, which runs none of it.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (ExeManifestTests)
{
public:

    static std::string LoadManifest (const wchar_t * exeName)
    {
        constexpr WORD  kManifestId = 1;

        fs::path      exePath = EmbeddedMachineJson::LocateCassoExe().parent_path() / exeName;
        HMODULE       hExe    = nullptr;
        HRSRC         hRes    = nullptr;
        HGLOBAL       hMem    = nullptr;
        DWORD         size    = 0;
        const char  * data    = nullptr;
        std::string   text;



        hExe = LoadLibraryExW (exePath.wstring().c_str(),
                               nullptr,
                               LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE);

        if (hExe != nullptr)
        {
            hRes = FindResourceW (hExe, MAKEINTRESOURCEW (kManifestId), RT_MANIFEST);
        }

        if (hRes != nullptr)
        {
            size = SizeofResource (hExe, hRes);
            hMem = LoadResource (hExe, hRes);
        }

        if (hMem != nullptr)
        {
            data = static_cast<const char *> (LockResource (hMem));
        }

        if (data != nullptr)
        {
            text.assign (data, size);
        }

        if (hExe != nullptr)
        {
            FreeLibrary (hExe);
        }

        return text;
    }

    //  The namespace is part of the match: Windows honors the element only in
    //  the 2019 WindowsSettings namespace, and ignores it anywhere else.
    static void AssertUtf8CodePage (const wchar_t * exeName)
    {
        static constexpr const char * kpszElement =
            "<activeCodePage xmlns=\"http://schemas.microsoft.com/SMI/2019/WindowsSettings\">UTF-8</activeCodePage>";

        std::string  manifest = LoadManifest (exeName);



        Assert::IsFalse (manifest.empty(), std::format (L"{} has an embedded manifest", exeName).c_str());
        Assert::IsTrue (manifest.find (kpszElement) != std::string::npos,
                        std::format (L"{} declares activeCodePage UTF-8", exeName).c_str());
    }

    TEST_METHOD (Casso_DeclaresTheUtf8CodePage)
    {
        AssertUtf8CodePage (L"Casso.exe");
    }

    TEST_METHOD (CassoCli_DeclaresTheUtf8CodePage)
    {
        AssertUtf8CodePage (L"CassoCli.exe");
    }
};
