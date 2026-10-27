#include "Pch.h"

#include "Debugger/Reverse/InputJournal.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InputJournalBranchTests
//
//  A fork moves the records after it out of the journal rather than drop
//  them, and a switch puts a branch's records back; the indices stay
//  absolute, so each record's index is the same along its path.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InputJournalBranchTests)
{
public:

    static void RecordKeys (InputJournal & journal, uint64_t & position, Byte first, int count)
    {
        for (int i = 0; i < count; i++)
        {
            position++;
            journal.Record (position, InputKind::KeyPress, static_cast<Byte> (first + i), 0, std::string_view());
        }
    }


    TEST_METHOD (MoveAfterTakesTheRecordsOutInOrder)
    {
        InputJournal              journal;
        std::vector<InputRecord>  moved;
        uint64_t                  position = 0;



        journal.SetOn             (true);
        journal.SetPositionSource (&position);

        RecordKeys (journal, position, 'A', 6);

        journal.MoveAfter (4, moved);

        Assert::AreEqual<size_t> (4,   journal.GetEndIndex(), L"the journal ends at the fork");
        Assert::AreEqual<size_t> (2,   moved.size(),          L"two records moved");
        Assert::AreEqual<int>    ('E', moved[0].value,        L"in order");
        Assert::AreEqual<int>    ('F', moved[1].value,        L"in order");
    }


    TEST_METHOD (AppendPutsABranchsRecordsBackAtTheirIndices)
    {
        InputJournal              journal;
        std::vector<InputRecord>  first;
        std::vector<InputRecord>  second;
        uint64_t                  position = 0;



        journal.SetOn             (true);
        journal.SetPositionSource (&position);

        RecordKeys (journal, position, 'A', 6);
        journal.MoveAfter (4, first);

        RecordKeys (journal, position, 'a', 3);
        journal.MoveAfter (4, second);

        journal.Append (first);

        Assert::AreEqual<size_t> (6,   journal.GetEndIndex(),       L"the first path's records are back");
        Assert::AreEqual<int>    ('E', journal.GetRecord (4).value, L"at index 4, its index before the fork");
        Assert::AreEqual<int>    ('F', journal.GetRecord (5).value, L"and 5");
        Assert::AreEqual<int>    ('D', journal.GetRecord (3).value, L"after the records before the fork");
        Assert::AreEqual<size_t> (3,   second.size(),               L"the other path's records, moved out");
    }


    TEST_METHOD (AMoveCountsAsACutAndAnAppendDoesNot)
    {
        InputJournal              journal;
        std::vector<InputRecord>  moved;
        uint64_t                  position = 0;
        uint64_t                  cuts     = 0;



        journal.SetOn             (true);
        journal.SetPositionSource (&position);

        RecordKeys (journal, position, 'A', 3);
        cuts = journal.GetCutCount();

        journal.MoveAfter (1, moved);
        Assert::AreEqual<uint64_t> (cuts + 1, journal.GetCutCount(), L"a move drops records from the end");

        journal.Append (moved);
        Assert::AreEqual<uint64_t> (cuts + 1, journal.GetCutCount(), L"an append adds after the end");
    }


    TEST_METHOD (AnAppendWorksWithTheJournalOff)
    {
        InputJournal              journal;
        std::vector<InputRecord>  moved;
        uint64_t                  position = 0;



        journal.SetOn             (true);
        journal.SetPositionSource (&position);

        RecordKeys (journal, position, 'A', 3);
        journal.MoveAfter (0, moved);
        journal.SetOn (false);

        journal.Append (moved);

        Assert::AreEqual<size_t> (3, journal.GetEndIndex(), L"the records are back with the journal off");
    }
};
