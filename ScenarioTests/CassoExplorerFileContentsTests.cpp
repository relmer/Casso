#include "Pch.h"
#include "FixtureProvider.h"
#include "Seams/ShellVerbs/Win32FileContents.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerFileContentsTests
//
//  The text of real files through the reader their extension's registered
//  IFilter selects, including the separate process the filters run in. Text,
//  Markdown and UTF-16 log files are written to a scratch folder; the Word,
//  PDF and JPEG files are checked-in fixtures; the .dll is a system file.
//
//  A test for a format with no IFilter registered on the machine logs "NOT
//  RUN" with the extension and returns. The test log lists the formats that
//  were checked.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerFileContentsTests)
{
public:

    static constexpr const wchar_t *  s_kpszWord = L"cassowarysentinel";

    TEST_METHOD_INITIALIZE (Initialize)
    {
        std::error_code  ec;
        std::string      utf16Log;

        m_dir = std::filesystem::temp_directory_path (ec) / std::format (L"casso-contents-{}-{}", GetCurrentProcessId(), GetTickCount64());
        std::filesystem::create_directories (m_dir, ec);

        WriteBytes (m_dir / L"note.txt",     "The CassoWarySentinel walked by.\r\n");
        WriteBytes (m_dir / L"readme.md",    "# Title\n\nA cassowarysentinel in markdown.\n");
        WriteBytes (m_dir / L"blob.dat",     std::string (256, '\0'));
        WriteBytes (m_dir / L"packed.zip",   "cassowarysentinel stored in a file with a .zip extension");
        WriteBytes (m_dir / L"nothing.txt",  "");

        utf16Log = "\xFF\xFE";

        for (wchar_t ch : std::wstring (L"2026-11-05 cassowarysentinel started\r\n"))
        {
            utf16Log.push_back ((char) (ch & 0xFF));
            utf16Log.push_back ((char) (ch >> 8));
        }

        WriteBytes (m_dir / L"service.log", utf16Log);
    }

    TEST_METHOD_CLEANUP (Cleanup)
    {
        std::error_code  ec;

        std::filesystem::remove_all (m_dir, ec);
    }

    TEST_METHOD (GetReader_PlainTextFilter_IsDirect)
    {
        Assert::IsTrue (Win32FileContents::GetReader ((m_dir / L"note.txt").wstring()) == Win32FileContents::Reader::Direct);
    }

    TEST_METHOD (GetReader_NullFilter_IsNone)
    {
        Assert::IsTrue (Win32FileContents::GetReader (GetSystemDll()) == Win32FileContents::Reader::None);
    }

    TEST_METHOD (GetReader_Zip_IsNone)
    {
        Assert::IsTrue (Win32FileContents::GetReader ((m_dir / L"packed.zip").wstring()) == Win32FileContents::Reader::None);
    }

    TEST_METHOD (GetReader_NoFilterAndTextStart_IsDirect)
    {
        Assert::IsTrue (Win32FileContents::GetReader ((m_dir / L"readme.md").wstring()) != Win32FileContents::Reader::None);
        Assert::IsTrue (Win32FileContents::GetReader ((m_dir / L"service.log").wstring()) != Win32FileContents::Reader::None);
    }

    TEST_METHOD (GetReader_NoFilterAndBinaryStart_IsNone)
    {
        Assert::IsTrue (Win32FileContents::GetReader ((m_dir / L"blob.dat").wstring()) == Win32FileContents::Reader::None);
    }

    TEST_METHOD (GetReader_EmptyFileWithNoFilter_IsNone)
    {
        WriteBytes (m_dir / L"empty.dat", "");

        Assert::IsTrue (Win32FileContents::GetReader ((m_dir / L"empty.dat").wstring()) == Win32FileContents::Reader::None);
    }

    TEST_METHOD (Load_Txt_HasLowercasedText)
    {
        Win32FileContents  contents;
        std::wstring       text;

        Assert::AreEqual (S_OK, contents.Load ((m_dir / L"note.txt").wstring(), text));
        Assert::IsTrue  (text.find (s_kpszWord) != std::wstring::npos);
        Assert::IsTrue  (text.find (L"CassoWarySentinel") == std::wstring::npos, L"Text is lowercased");
        Assert::AreEqual ((DWORD) 0, contents.GetHostProcessId(), L"A direct read doesn't start a process");
    }

    TEST_METHOD (Load_Markdown_HasText)
    {
        Win32FileContents  contents;
        std::wstring       text;

        Assert::AreEqual (S_OK, contents.Load ((m_dir / L"readme.md").wstring(), text));
        Assert::IsTrue  (text.find (s_kpszWord) != std::wstring::npos);
    }

    TEST_METHOD (Load_Utf16Log_HasText)
    {
        Win32FileContents  contents;
        std::wstring       text;

        Assert::AreEqual (S_OK, contents.Load ((m_dir / L"service.log").wstring(), text));
        Assert::IsTrue  (text.find (s_kpszWord) != std::wstring::npos);
        Assert::IsTrue  (text.find (L"2026-11-05") != std::wstring::npos);
    }

    TEST_METHOD (Load_Docx_HasText)
    {
        ExpectFilterText (L"sample.docx");
    }

    TEST_METHOD (Load_Pdf_HasText)
    {
        ExpectFilterText (L"sample.pdf");
    }

    TEST_METHOD (Load_Jpg_HasMetadataText)
    {
        ExpectFilterText (L"sample.jpg");
    }

    TEST_METHOD (Load_Dll_HasNoText)
    {
        Win32FileContents  contents;
        std::wstring       text = L"stale";

        Assert::AreEqual (S_FALSE, contents.Load (GetSystemDll(), text));
        Assert::IsTrue   (text.empty());
    }

    TEST_METHOD (Load_Zip_HasNoText)
    {
        Win32FileContents  contents;
        std::wstring       text;

        Assert::AreEqual (S_FALSE, contents.Load ((m_dir / L"packed.zip").wstring(), text));
        Assert::IsTrue   (text.empty());
    }

    TEST_METHOD (Load_BinaryWithNoFilter_HasNoText)
    {
        Win32FileContents  contents;
        std::wstring       text;

        Assert::AreEqual (S_FALSE, contents.Load ((m_dir / L"blob.dat").wstring(), text));
    }

    TEST_METHOD (Load_EmptyTxt_HasNoText)
    {
        Win32FileContents  contents;
        std::wstring       text;

        Assert::AreEqual (S_FALSE, contents.Load ((m_dir / L"nothing.txt").wstring(), text));
    }

    TEST_METHOD (Load_MissingTxt_Fails)
    {
        Win32FileContents  contents;
        std::wstring       text;
        HRESULT            hr = contents.Load ((m_dir / L"absent.txt").wstring(), text);

        Assert::IsTrue (FAILED (hr));
    }

    TEST_METHOD (Load_LargeTxt_StopsAtTheLimit)
    {
        Win32FileContents  contents;
        std::wstring       text;

        WriteBytes (m_dir / L"large.txt", std::string (Win32FileContents::kMaxChars + 4096, 'q'));

        Assert::AreEqual (S_OK, contents.Load ((m_dir / L"large.txt").wstring(), text));
        Assert::AreEqual (Win32FileContents::kMaxChars, text.size());
    }

    TEST_METHOD (Load_FilteredFiles_ShareOneProcess)
    {
        std::wstring  docx = Fixture (L"sample.docx");
        std::wstring  pdf  = Fixture (L"sample.pdf");

        if (!HasFilter (docx, L".docx") || !HasFilter (pdf, L".pdf"))
        {
            return;
        }

        Win32FileContents  contents;
        std::wstring       text;

        Assert::AreEqual (S_OK, contents.Load (docx, text));

        DWORD  first = contents.GetHostProcessId();

        Assert::AreNotEqual ((DWORD) 0, first);

        Assert::AreEqual (S_OK, contents.Load (pdf, text));
        Assert::AreEqual (S_OK, contents.Load (docx, text));
        Assert::AreEqual (first, contents.GetHostProcessId(), L"The process serves every filtered file");
    }

    //  After a filter fails to open a file, the process serves the next file.
    TEST_METHOD (Load_FilterFailure_CostsThatFileOnly)
    {
        std::wstring  docx = Fixture (L"sample.docx");

        if (!HasFilter (docx, L".docx"))
        {
            return;
        }

        std::filesystem::path  broken = m_dir / L"broken.docx";
        Win32FileContents      contents;
        std::wstring           text;

        WriteBytes (broken, "this is not a zip file at all");

        Assert::AreEqual (S_OK, contents.Load (docx, text));

        DWORD  first = contents.GetHostProcessId();

        Assert::AreEqual (S_FALSE, contents.Load (broken.wstring(), text));
        Assert::IsTrue   (text.empty());

        Assert::AreEqual (S_FALSE, contents.Load ((m_dir / L"absent.docx").wstring(), text));
        Assert::AreEqual (first, contents.GetHostProcessId(), L"The process is the same one after a filter failure");

        Assert::AreEqual (S_OK, contents.Load (docx, text));
        Assert::IsTrue   (text.find (s_kpszWord) != std::wstring::npos);
    }

    //  A filter past the limit costs the file, the process is ended, and the
    //  next file reads in a new one.
    TEST_METHOD (Load_FilterPastTheLimit_CostsThatFileOnly)
    {
        std::wstring  docx = Fixture (L"sample.docx");

        if (!HasFilter (docx, L".docx"))
        {
            return;
        }

        Win32FileContents  contents;
        std::wstring       text;

        //  A process can't start and answer within a millisecond.
        contents.SetFilterTimeout (1);

        Assert::AreEqual (S_FALSE, contents.Load (docx, text));
        Assert::IsTrue   (text.empty());
        Assert::AreEqual ((DWORD) 0, contents.GetHostProcessId(), L"The process that missed the limit is ended");

        contents.SetFilterTimeout (Win32FileContents::kFilterTimeoutMs);

        Assert::AreEqual (S_OK, contents.Load (docx, text));
        Assert::IsTrue   (text.find (s_kpszWord) != std::wstring::npos);
        Assert::AreNotEqual ((DWORD) 0, contents.GetHostProcessId());

        Assert::AreEqual (S_OK, contents.Load ((m_dir / L"note.txt").wstring(), text), L"A direct read is unaffected");
    }

    //  A process that is killed between two files, as a filter's crash would.
    TEST_METHOD (Load_FilterProcessEnded_NextFileReadsInANewProcess)
    {
        std::wstring  docx = Fixture (L"sample.docx");

        if (!HasFilter (docx, L".docx"))
        {
            return;
        }

        Win32FileContents  contents;
        std::wstring       text;

        Assert::AreEqual (S_OK, contents.Load (docx, text));

        DWORD   first   = contents.GetHostProcessId();
        HANDLE  process = OpenProcess (PROCESS_TERMINATE | SYNCHRONIZE, FALSE, first);

        Assert::IsNotNull (process);
        Assert::IsTrue    (TerminateProcess (process, 1) != FALSE);
        Assert::AreEqual  (WAIT_OBJECT_0, WaitForSingleObject (process, 10000));
        CloseHandle (process);

        HRESULT  hr = contents.Load (docx, text);

        Assert::IsTrue (hr == S_OK || hr == S_FALSE, L"Only the file in flight is at risk");

        Assert::AreEqual (S_OK, contents.Load (docx, text));
        Assert::IsTrue   (text.find (s_kpszWord) != std::wstring::npos);
        Assert::AreNotEqual (first, contents.GetHostProcessId());
    }

    //  Ending the reader ends the process with it.
    TEST_METHOD (Destructor_EndsTheFilterProcess)
    {
        std::wstring  docx = Fixture (L"sample.docx");

        if (!HasFilter (docx, L".docx"))
        {
            return;
        }

        std::wstring  text;
        DWORD         id      = 0;
        HANDLE        process = nullptr;

        {
            Win32FileContents  contents;

            Assert::AreEqual (S_OK, contents.Load (docx, text));

            id      = contents.GetHostProcessId();
            process = OpenProcess (SYNCHRONIZE, FALSE, id);

            Assert::IsNotNull (process);
        }

        Assert::AreEqual (WAIT_OBJECT_0, WaitForSingleObject (process, 10000), L"The process ended with its reader");
        CloseHandle (process);
    }

private:

    std::filesystem::path  m_dir;

    static void WriteBytes (const std::filesystem::path & path, const std::string & bytes)
    {
        std::ofstream  out (path, std::ios::binary);

        out.write (bytes.data(), (std::streamsize) bytes.size());
    }


    static std::wstring GetSystemDll()
    {
        wchar_t  dir[MAX_PATH] = {};

        GetSystemDirectoryW (dir, MAX_PATH);

        return (std::filesystem::path (dir) / L"kernel32.dll").wstring();
    }


    //  The checked-in fixture's path, or empty when it is missing, which fails.
    static std::wstring Fixture (const wchar_t * name)
    {
        FixtureProvider        fixtures;
        std::filesystem::path  path = std::filesystem::path (fixtures.GetRoot()) / L"CassoExplorer" / L"Contents" / name;
        std::error_code        ec;
        bool                   exists = std::filesystem::exists (path, ec);

        Assert::IsTrue (exists, (std::wstring (L"Missing fixture ") + path.wstring()).c_str());

        return path.wstring();
    }


    //  Whether the machine has an IFilter for the fixture; logs NOT RUN when
    //  it has no filter.
    static bool HasFilter (const std::wstring & path, const wchar_t * extension)
    {
        bool  hasFilter = Win32FileContents::GetReader (path) == Win32FileContents::Reader::Filter;

        if (!hasFilter)
        {
            Logger::WriteMessage ((std::wstring (L"NOT RUN: no IFilter is registered for ") + extension + L" on this machine").c_str());
        }

        return hasFilter;
    }


    static void ExpectFilterText (const wchar_t * name)
    {
        std::wstring  path = Fixture (name);

        if (!HasFilter (path, std::filesystem::path (name).extension().c_str()))
        {
            return;
        }

        Win32FileContents  contents;
        std::wstring       text;

        Assert::AreEqual (S_OK, contents.Load (path, text), name);
        Assert::IsTrue   (text.find (s_kpszWord) != std::wstring::npos, (std::wstring (L"No sentinel word in ") + name).c_str());
    }
};
