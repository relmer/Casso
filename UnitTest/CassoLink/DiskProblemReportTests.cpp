#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/FullSend/DiskProblemReport.h"
#include "Update/ZipArchive.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskProblemReportTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskProblemReportTests)
{
public:

    TEST_METHOD (TheReport_HasTheNoteAndTheKnownFacts_AndLeavesOutTheUnknown)
    {
        DiskProblemFacts  facts  = MakeFacts();
        std::string       report = DiskProblemReport::FormatReport (facts);

        Assert::IsTrue (report.find ("Boots to a prompt") != std::string::npos);
        Assert::IsTrue (report.find ("Status: raw-capture") != std::string::npos);
        Assert::IsTrue (report.find ("Apple: Apple //c") != std::string::npos);
        Assert::IsTrue (report.find ("Windows:") == std::string::npos, L"no Windows version was given");
    }



    TEST_METHOD (TheZip_HasEachFile_ButAnEmptyOne)
    {
        DiskProblemFacts       facts = MakeFacts();
        std::vector<Byte>      zip;
        std::vector<ZipEntry>  entries;
        std::set<std::string>  paths;

        AssertSucceeded (DiskProblemReport::MakeZip (facts, zip));
        AssertSucceeded (ZipArchive::Extract (zip, entries));

        for (const ZipEntry & entry : entries)
        {
            paths.insert (entry.path);
        }

        Assert::IsTrue (paths == std::set<std::string> { "Report.txt", "Zork I.woz", "Zork I.woz.reads", "Capture log.csv" });
    }



    TEST_METHOD (TheIssue_IsEncoded_AndCutToFitAUrl)
    {
        DiskProblemFacts  facts = MakeFacts();
        std::wstring      url;

        facts.note.assign (DiskProblemReport::kMaxUrlBody * 2, 'x');
        url = DiskProblemReport::MakeIssueUrl (facts, "Zork I report.zip");

        Assert::IsTrue (url.starts_with (DiskProblemReport::kNewIssueUrl));
        Assert::IsTrue (url.find (L"title=%5BDisk%5D%3A%20Zork%20I") != std::wstring::npos, url.c_str());
        Assert::IsTrue (url.size() < DiskProblemReport::kMaxUrlBody * 3 + 1000);
    }



    TEST_METHOD (TheLog_IsTheHeaderAndTheRowsInTheSpan)
    {
        std::string  log    = "Time,Connection,Kind,Message\r\n"
                              "2026-11-20 08:00:00.100,COM3,Activity,before\r\n"
                              "2026-11-20 08:10:00.200,COM3,Activity,\"first\r\ncontinued\"\r\n"
                              "2026-11-20 08:20:00.300,COM3,Activity,last\r\n"
                              "2026-11-20 08:30:00.400,COM3,Activity,after\r\n";
        std::string  picked = DiskProblemReport::PickLogRows (log, "2026-11-20 08:05:00", "2026-11-20 08:20:00");

        Assert::AreEqual (std::string ("Time,Connection,Kind,Message\r\n"
                                       "2026-11-20 08:10:00.200,COM3,Activity,\"first\r\ncontinued\"\r\n"
                                       "2026-11-20 08:20:00.300,COM3,Activity,last\r\n"), picked);
    }

private:

    static DiskProblemFacts MakeFacts()
    {
        DiskProblemFacts  facts;

        facts.image       = "Zork I.woz";
        facts.imageBytes  = { 1, 2, 3 };
        facts.reads       = { 4, 5 };
        facts.hasRow      = true;
        facts.row.status  = "raw-capture";
        facts.captureLog  = "Time,Connection,Kind,Message\r\n";
        facts.appleModel  = "Apple //c";
        facts.note        = "Boots to a prompt";

        return facts;
    }
};
