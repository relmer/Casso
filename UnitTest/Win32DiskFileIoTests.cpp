#include "Pch.h"
#include "Cli/Win32DiskFileIo.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DiskFileIoTests
//
//  The one place the disk-image seams touch the real filesystem.
//
//  THE POINT IS THE ERROR CODE, NOT THE BYTES. Reads and writes went through
//  fstreams and a CBR, which turned every failure into E_FAIL; the save-failure
//  notice, built to print the code and the system's own words for it, said
//  "0x80004005 Unspecified error" for a folder that refused the file. Each
//  failing case here asserts the real Win32 code and, separately, that it is
//  not E_FAIL -- the second assertion is the one that would have caught this.
//
//  EVERY CASE CLEANS UP BEFORE IT ASSERTS. A read-only file left behind by a
//  failed assertion would break the next run of the suite, not this one.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Win32DiskFileIoTests)
{
public:

    //  A scratch folder of this suite's own.
    static std::filesystem::path  Scratch()
    {
        std::filesystem::path  folder = std::filesystem::temp_directory_path()
                                      / L"CassoWin32DiskFileIoTests";

        std::filesystem::create_directories (folder);

        return folder;
    }



    TEST_METHOD (WriteAllBytes_RoundTripsThroughReadAllBytes)
    {
        Win32DiskFileIo     io;
        std::vector<Byte>   out  { 0x01, 0x02, 0x03, 0xFF, 0x00, 0x7F };
        std::vector<Byte>   back;
        std::string         path = (Scratch() / L"roundtrip.bin").string();
        HRESULT             hrWrite = S_OK;
        HRESULT             hrRead  = S_OK;



        hrWrite = io.WriteAllBytes (path, out);
        hrRead  = io.ReadAllBytes (path, back);

        std::filesystem::remove (path);

        Assert::IsTrue (SUCCEEDED (hrWrite), L"a plain write into a writable folder succeeds");
        Assert::IsTrue (SUCCEEDED (hrRead),  L"and reads back");
        Assert::IsTrue (back == out,         L"byte for byte");
    }



    TEST_METHOD (WriteAllBytes_IntoAMissingFolder_ReportsPathNotFound_NotEFail)
    {
        Win32DiskFileIo         io;
        std::vector<Byte>       bytes { 0x42 };
        std::filesystem::path   folder = Scratch() / L"no-such-folder";
        std::string             path   = (folder / L"x.bin").string();
        HRESULT                 hr     = S_OK;



        std::filesystem::remove_all (folder);

        hr = io.WriteAllBytes (path, bytes);

        Assert::AreEqual ((int) HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND), (int) hr,
                          L"the system's own code for a folder that is not there");
        Assert::IsTrue (hr != E_FAIL, L"never the code that says nothing");
    }



    TEST_METHOD (WriteAllBytes_OntoAReadOnlyFile_ReportsAccessDenied_NotEFail)
    {
        Win32DiskFileIo     io;
        std::vector<Byte>   bytes { 0x42 };
        std::string         path  = (Scratch() / L"readonly.bin").string();
        std::wstring  wide   = std::filesystem::path (path).wstring();
        HRESULT       hrSeed = S_OK;
        HRESULT       hr     = S_OK;



        hrSeed = io.WriteAllBytes (path, bytes);
        SetFileAttributesW (wide.c_str(), FILE_ATTRIBUTE_READONLY);

        hr = io.WriteAllBytes (path, bytes);

        //  Cleanup first: a read-only file that outlives a failed assertion
        //  breaks the NEXT run, which is a worse failure than this one.
        SetFileAttributesW (wide.c_str(), FILE_ATTRIBUTE_NORMAL);
        std::filesystem::remove (path);

        Assert::IsTrue (SUCCEEDED (hrSeed), L"the file to protect was written first");
        Assert::AreEqual ((int) HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), (int) hr,
                          L"the system's own code for a file it will not let us write");
        Assert::IsTrue (hr != E_FAIL, L"never the code that says nothing");
    }



    TEST_METHOD (ReadAllBytes_OfAMissingFile_ReportsFileNotFound_NotEFail)
    {
        Win32DiskFileIo     io;
        std::vector<Byte>   back;
        std::string         path = (Scratch() / L"absent.bin").string();
        HRESULT             hr   = S_OK;



        std::filesystem::remove (path);

        hr = io.ReadAllBytes (path, back);

        Assert::AreEqual ((int) HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), (int) hr,
                          L"the system's own code for a file that is not there");
        Assert::IsTrue (hr != E_FAIL, L"never the code that says nothing");
        Assert::IsTrue (back.empty(), L"nothing was read");
    }
};
