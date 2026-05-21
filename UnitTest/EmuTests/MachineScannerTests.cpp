#include "Pch.h"
#include <map>

#include "Core/MachineScanner.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineScannerTests
//
//  Pure unit tests for MachineScanner. The scanner is injected with
//  in-memory directory / file fakes so no on-disk state is touched.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineScannerTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Helpers
    //
    ////////////////////////////////////////////////////////////////////////////

    using DirMap  = map<fs::path, vector<fs::path>>;
    using FileMap = map<fs::path, string>;


    static MachineScanner::DirectoryLister MakeLister (const DirMap & dirs)
    {
        return [dirs] (const fs::path & dir) -> vector<fs::path>
        {
            vector<fs::path>  out;
            auto              it = dirs.find (dir);


            if (it != dirs.end())
            {
                out = it->second;
            }

            return out;
        };
    }

    static MachineScanner::FileReader MakeReader (const FileMap & files)
    {
        return [files] (const fs::path & file, string & outText) -> HRESULT
        {
            HRESULT  hr = E_FAIL;
            auto     it = files.find (file);


            if (it != files.end())
            {
                outText = it->second;
                hr      = S_OK;
            }

            return hr;
        };
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Scan_NestedLayout_FindsAllMachines
    //
    //  The bug fix: layout is Machines/<Name>/<Name>.json, NOT
    //  Machines/*.json. Verify the scanner walks subdirectories.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Scan_NestedLayout_FindsAllMachines)
    {
        DirMap dirs =
        {
            { "C:/app/Machines", { "C:/app/Machines/Apple2", "C:/app/Machines/Apple2Plus" } },
        };
        FileMap files =
        {
            { "C:/app/Machines/Apple2/Apple2.json",         "{\"name\":\"Apple ][\"}"      },
            { "C:/app/Machines/Apple2Plus/Apple2Plus.json", "{\"name\":\"Apple ][ plus\"}" },
        };

        auto results = MachineScanner::Scan ({ "C:/app" }, MakeLister (dirs), MakeReader (files));

        Assert::AreEqual (size_t (2), results.size());
        Assert::AreEqual (wstring (L"Apple2"),         results[0].fileName);
        Assert::AreEqual (wstring (L"Apple ][")     ,  results[0].displayName);
        Assert::AreEqual (wstring (L"Apple2Plus"),     results[1].fileName);
        Assert::AreEqual (wstring (L"Apple ][ plus"),  results[1].displayName);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Scan_FlatLayout_ReturnsEmpty
    //
    //  The pre-fix bug: scanner expected Machines/*.json directly. Verify
    //  that flat layout (.json files at top-level Machines/) is now NOT
    //  picked up — only nested subdirectory layout counts.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Scan_FlatLayout_ReturnsEmpty)
    {
        DirMap dirs =
        {
            // No subdirectories, only files at top level (which the
            // lister never returns since it only lists directories).
            { "C:/app/Machines", { } },
        };
        FileMap files =
        {
            { "C:/app/Machines/Apple2.json", "{\"name\":\"Apple ][\"}" },
        };

        auto results = MachineScanner::Scan ({ "C:/app" }, MakeLister (dirs), MakeReader (files));

        Assert::AreEqual (size_t (0), results.size());
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Scan_FirstSearchPathWins
    //
    //  Multiple search paths each containing Machines/ — only the first
    //  one with a non-empty directory listing is used. Matches Main.cpp's
    //  FindFile semantics so picker and loader can never disagree.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Scan_FirstSearchPathWins)
    {
        DirMap dirs =
        {
            { "C:/app/Machines",     { "C:/app/Machines/Apple2" } },
            { "C:/project/Machines", { "C:/project/Machines/Apple2e" } },
        };
        FileMap files =
        {
            { "C:/app/Machines/Apple2/Apple2.json",         "{\"name\":\"App2\"}"  },
            { "C:/project/Machines/Apple2e/Apple2e.json",   "{\"name\":\"App2e\"}" },
        };

        auto results = MachineScanner::Scan (
            { "C:/app", "C:/project" }, MakeLister (dirs), MakeReader (files));

        Assert::AreEqual (size_t (1), results.size());
        Assert::AreEqual (wstring (L"Apple2"), results[0].fileName);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Scan_SkipsSearchPathsWithoutMachinesDir
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Scan_SkipsSearchPathsWithoutMachinesDir)
    {
        DirMap dirs =
        {
            { "C:/project/Machines", { "C:/project/Machines/Apple2" } },
        };
        FileMap files =
        {
            { "C:/project/Machines/Apple2/Apple2.json", "{\"name\":\"App2\"}" },
        };

        auto results = MachineScanner::Scan (
            { "C:/missing", "C:/project" }, MakeLister (dirs), MakeReader (files));

        Assert::AreEqual (size_t (1), results.size());
        Assert::AreEqual (wstring (L"Apple2"), results[0].fileName);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Scan_MissingJson_SkipsSubdir
    //
    //  A subdirectory under Machines/ without a matching <Name>.json is
    //  silently skipped — it may belong to a different asset family.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Scan_MissingJson_SkipsSubdir)
    {
        DirMap dirs =
        {
            { "C:/app/Machines", { "C:/app/Machines/Apple2", "C:/app/Machines/Stray" } },
        };
        FileMap files =
        {
            { "C:/app/Machines/Apple2/Apple2.json", "{\"name\":\"App2\"}" },
        };

        auto results = MachineScanner::Scan ({ "C:/app" }, MakeLister (dirs), MakeReader (files));

        Assert::AreEqual (size_t (1), results.size());
        Assert::AreEqual (wstring (L"Apple2"), results[0].fileName);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Scan_UnparseableJson_FallsBackToFileName
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Scan_UnparseableJson_FallsBackToFileName)
    {
        DirMap dirs =
        {
            { "C:/app/Machines", { "C:/app/Machines/Junk" } },
        };
        FileMap files =
        {
            { "C:/app/Machines/Junk/Junk.json", "this is not json" },
        };

        auto results = MachineScanner::Scan ({ "C:/app" }, MakeLister (dirs), MakeReader (files));

        Assert::AreEqual (size_t (1), results.size());
        Assert::AreEqual (wstring (L"Junk"), results[0].fileName);
        Assert::AreEqual (wstring (L"Junk"), results[0].displayName);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Scan_JsonWithoutNameField_FallsBackToFileName
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Scan_JsonWithoutNameField_FallsBackToFileName)
    {
        DirMap dirs =
        {
            { "C:/app/Machines", { "C:/app/Machines/Apple2" } },
        };
        FileMap files =
        {
            { "C:/app/Machines/Apple2/Apple2.json", "{\"cpu\":\"6502\"}" },
        };

        auto results = MachineScanner::Scan ({ "C:/app" }, MakeLister (dirs), MakeReader (files));

        Assert::AreEqual (size_t (1), results.size());
        Assert::AreEqual (wstring (L"Apple2"), results[0].displayName);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Scan_NoSearchPaths_ReturnsEmpty
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Scan_NoSearchPaths_ReturnsEmpty)
    {
        auto results = MachineScanner::Scan ({}, MakeLister ({}), MakeReader ({}));

        Assert::AreEqual (size_t (0), results.size());
    }
};
