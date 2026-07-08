#include "Pch.h"

#include "Devices/Printer/PrintJobPersistence.h"
#include "Devices/Printer/PrintRaster.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  PrintJobPersistenceTests
//
//  The real save/load path -- strip serialized to an indexed PNG + JSON and
//  reconstructed through actual WIC -- plus graceful rejection of corrupt
//  buffers. In-memory only (Test Isolation); the shell adds the file I/O.
//
////////////////////////////////////////////////////////////////////////////////

namespace PrintJobPersistenceTests
{
    TEST_CLASS (PrintJobPersistenceTests)
    {
    public:

        TEST_METHOD_INITIALIZE (InitCom)
        {
            HRESULT   hr = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
            m_ownsCom = (hr == S_OK || hr == S_FALSE);
        }


        TEST_METHOD_CLEANUP (UninitCom)
        {
            if (m_ownsCom)
            {
                CoUninitialize ();
            }
        }


        TEST_METHOD (RoundTripThroughPngAndJson)
        {
            PrintRaster    original;
            PrintRaster    reloaded;
            vector<Byte>   png;
            string         json;

            original.Strike (0,    0, InkPrimary::Black);
            original.Strike (100,  3, InkPrimary::Yellow);
            original.Strike (100,  3, InkPrimary::Red);      // orange (0x6)
            original.Strike (1279, 5, InkPrimary::Blue);
            original.AdvanceRows (200);
            original.MarkFormFeed ();                        // boundary at 1584

            Assert::IsTrue (SUCCEEDED (PrintJobPersistence::Save (original, png, json)));
            Assert::IsTrue (png.size () > 8);
            Assert::IsTrue (SUCCEEDED (PrintJobPersistence::Load (png, json, reloaded)));

            Assert::AreEqual (original.RowsUsed (), reloaded.RowsUsed ());
            Assert::AreEqual (original.PaperRow (), reloaded.PaperRow ());
            Assert::AreEqual ((Byte) InkPrimary::Black, reloaded.CellAt (0, 0));
            Assert::AreEqual ((Byte) 0x6,               reloaded.CellAt (100, 3));
            Assert::AreEqual ((Byte) InkPrimary::Blue,  reloaded.CellAt (1279, 5));
            Assert::AreEqual ((size_t) 1, reloaded.PageBoundaryRows ().size ());
            Assert::AreEqual (PrinterGrid::kPageRows, reloaded.PageBoundaryRows ()[0]);
        }


        TEST_METHOD (EmptyStripRoundTrips)
        {
            PrintRaster    original;
            PrintRaster    reloaded;
            vector<Byte>   png;
            string         json;

            Assert::IsTrue (SUCCEEDED (PrintJobPersistence::Save (original, png, json)));
            Assert::IsTrue (SUCCEEDED (PrintJobPersistence::Load (png, json, reloaded)));

            Assert::AreEqual (0, reloaded.RowsUsed ());
            Assert::IsFalse (reloaded.RowsUsed () > 0);
        }


        TEST_METHOD (CorruptJsonRejected)
        {
            PrintRaster    original;
            PrintRaster    reloaded;
            vector<Byte>   png;
            string         json;

            original.Strike (0, 0, InkPrimary::Black);
            Assert::IsTrue (SUCCEEDED (PrintJobPersistence::Save (original, png, json)));

            Assert::IsFalse (SUCCEEDED (PrintJobPersistence::Load (png, "{ broken", reloaded)));
        }


        TEST_METHOD (CorruptPngRejected)
        {
            PrintRaster    reloaded;
            vector<Byte>   junk = { 0x00, 0x01, 0x02, 0x03, 0x04 };
            string         json = "{ \"formatVersion\": 1, \"rowsUsed\": 0, "
                                  "\"paperRow\": 0, \"capReached\": false }";

            Assert::IsFalse (SUCCEEDED (PrintJobPersistence::Load (junk, json, reloaded)));
        }


    private:
        bool   m_ownsCom = false;
    };
}
