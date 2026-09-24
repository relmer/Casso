#include "Pch.h"

#include "Ui/Debugger/Panes/SourceDocuments.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceDocumentsTests
    //
    //  A document per file (FR-054, SC-027): a file is opened once and brought
    //  forward after, the one used longest ago gives way when every slot is
    //  taken but never the PC's, and what is open is saved by name and line.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceDocumentsTests)
    {
    public:

        TEST_METHOD (AFileIsOpenedOnceAndBroughtForwardAfter)
        {
            SourceDocuments  documents;
            int              main     = documents.Open (0);
            int              included = documents.Open (1);



            Assert::AreNotEqual (main, included, L"each file its own document");
            Assert::AreEqual    (main, documents.Open (0), L"back to the first file is its document again");
            Assert::AreEqual    (2, documents.GetCount());
        }


        TEST_METHOD (TheDocumentUsedLongestAgoGivesWayButNeverThePcs)
        {
            SourceDocuments  documents;



            for (int file = 0; file < SourceDocuments::kMaxDocuments; file++)
            {
                documents.Open (file);
            }

            //  File 0 is the oldest, but it holds the PC.
            documents.Open (100, 0);

            Assert::AreNotEqual (-1, documents.Find (0),   L"the PC's document stays");
            Assert::AreEqual    (-1, documents.Find (1),   L"the next oldest gave way");
            Assert::AreNotEqual (-1, documents.Find (100));
            Assert::AreEqual    (SourceDocuments::kMaxDocuments, documents.GetCount());
        }


        TEST_METHOD (ClosingADocumentClosesOnlyIt)
        {
            SourceDocuments  documents;
            int              first  = documents.Open (4);
            int              second = documents.Open (5);



            documents.Close (first);

            Assert::IsFalse  (documents.IsOpen (first));
            Assert::IsTrue   (documents.IsOpen (second));
            Assert::AreEqual (-1, documents.Find (4));
        }


        TEST_METHOD (TheOpenDocumentsSaveAndParseByNameAndLine)
        {
            SourceDocuments                       documents;
            std::vector<SourceDocuments::Saved>   saved;
            auto                                  nameOf = [] (int file) { return file == 0 ? std::string ("main.a65") : std::string ("my file%.inc"); };



            documents.SetLine (documents.Open (0), 14);
            documents.SetLine (documents.Open (1), 2);

            saved = SourceDocuments::Parse ("code2=0300 " + documents.Format (nameOf) + " panel=mmu");

            Assert::AreEqual ((size_t) 2, saved.size(), L"the other views' entries are passed over");
            Assert::IsTrue   (saved[0] == SourceDocuments::Saved { "main.a65", 14 });
            Assert::IsTrue   (saved[1] == SourceDocuments::Saved { "my file%.inc", 2 }, L"a space and a percent sign survive");
        }
    };
}
