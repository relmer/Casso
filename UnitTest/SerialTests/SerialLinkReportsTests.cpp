#include "Pch.h"

#include "Devices/Serial/SerialLinkReports.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SerialLinkReportsTestNs
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  SerialLinkReportsTests
    //
    //  The problems serial attachments post for the infobar: one per port,
    //  the newest last, and a generation that changes only when the list
    //  does.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SerialLinkReportsTests)
    {
    public:
        TEST_METHOD (APostIsListedWithItsPort)
        {
            SerialLinkReports                        reports;
            std::vector<SerialLinkReports::Report>   listed;



            reports.Post ("Serial port 2", "COM3 is in use by another program.");
            listed = reports.GetReports();

            Assert::AreEqual<size_t> (1, listed.size());
            Assert::AreEqual (std::string ("Serial port 2"), listed[0].portTitle);
            Assert::AreEqual (std::string ("COM3 is in use by another program."), listed[0].text);
        }


        TEST_METHOD (APortsNewPostReplacesItsOldOneAndMovesLast)
        {
            SerialLinkReports                        reports;
            std::vector<SerialLinkReports::Report>   listed;



            reports.Post ("Serial port 1", "first");
            reports.Post ("Serial port 2", "second");
            reports.Post ("Serial port 1", "third");
            listed = reports.GetReports();

            Assert::AreEqual<size_t> (2, listed.size());
            Assert::AreEqual (std::string ("second"), listed[0].text);
            Assert::AreEqual (std::string ("third"),  listed[1].text, L"the newest is last");
        }


        TEST_METHOD (AnEmptyTextClearsThePort)
        {
            SerialLinkReports  reports;



            reports.Post ("Serial port 1", "gone");
            reports.Post ("Serial port 1", "");

            Assert::AreEqual<size_t> (0, reports.GetReports().size());
        }


        TEST_METHOD (TheGenerationChangesOnlyWithTheList)
        {
            SerialLinkReports  reports;
            uint64_t           start = reports.GetGeneration();



            reports.Post ("Serial port 1", "");
            Assert::AreEqual (start, reports.GetGeneration(), L"clearing a port with no report changes nothing");

            reports.Post ("Serial port 1", "down");
            Assert::AreEqual (start + 1, reports.GetGeneration());

            reports.Post ("Serial port 1", "down");
            Assert::AreEqual (start + 1, reports.GetGeneration(), L"the same text again changes nothing");

            reports.Clear();
            Assert::AreEqual (start + 2, reports.GetGeneration());
            Assert::AreEqual<size_t> (0, reports.GetReports().size());
        }
    };
}
