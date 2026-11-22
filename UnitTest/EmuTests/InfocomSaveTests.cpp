#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/InfocomSave.h"
#include "InfocomSaveBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomSaveTests
//
//  InfocomSaveBuilder's save disk, read back. The real save is in
//  ScenarioTests/GameDataTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InfocomSaveTests)
{
public:

    TEST_METHOD (TryReadPosition_ReadsTheRoomScoreAndMoves)
    {
        std::vector<Byte>       disk = InfocomSaveBuilder::MakeDisk (InfocomSaveBuilder::MakeMemory (0), 2);
        InfocomSave::Position   position;



        Assert::IsTrue   (InfocomSave::TryReadPosition (disk, 2, position));
        Assert::AreEqual (3,  position.version);
        Assert::AreEqual (5,  position.release);
        Assert::AreEqual (std::string ("123456"), position.serial);
        Assert::AreEqual (std::wstring (L"hall"), position.room);
        Assert::AreEqual (7,  position.score);
        Assert::AreEqual (12, position.moves);
        Assert::AreEqual (std::string ("release 5: hall, score 7, 12 moves"), InfocomSave::Describe (position));

        Assert::IsFalse  (InfocomSave::TryReadPosition (disk, 1, position), L"nothing saved there");
    }


    //  The player is the object in the room that a global variable has, its
    //  tree is what it carries, and the rest of the room's tree is what's
    //  there. Every object is named "hall" by the builder's one name table.
    TEST_METHOD (TryReadPosition_ReadsWhatIsCarriedAndWhatIsHere)
    {
        constexpr size_t       kEntries = InfocomSaveBuilder::kObjects + 31 * 2;
        constexpr size_t       kSize    = 9;
        std::vector<Byte>      memory   = InfocomSaveBuilder::MakeMemory (0);
        InfocomSave::Position  position;



        for (size_t n = 1; n <= 4; n++)
        {
            InfocomSaveBuilder::PutWord (memory, kEntries + (n - 1) * kSize + 7, (int) InfocomSaveBuilder::kNameTable);
        }

        memory[kEntries + 0 * kSize + 6] = 2;      // the hall has the player
        memory[kEntries + 1 * kSize + 4] = 1;
        memory[kEntries + 1 * kSize + 5] = 4;      // and the box after it
        memory[kEntries + 1 * kSize + 6] = 3;      // the player has the lamp
        memory[kEntries + 2 * kSize + 4] = 2;
        memory[kEntries + 3 * kSize + 4] = 1;
        InfocomSaveBuilder::PutWord (memory, InfocomSaveBuilder::kGlobals + 3 * 2, 2);

        Assert::IsTrue (InfocomSave::TryReadPosition (InfocomSaveBuilder::MakeDisk (memory, 0), 0, position));
        Assert::IsTrue (position.carried == std::vector<InfocomSave::Item> ({ { 0, L"hall" } }), L"The lamp");
        Assert::IsTrue (position.here    == std::vector<InfocomSave::Item> ({ { 0, L"hall" } }), L"The box, and not the player");
    }

    //  A version 3 story with the timed flag has the hours and minutes in
    //  the globals of the score and the moves.
    TEST_METHOD (Describe_ATimedStoryHasTheTime)
    {
        std::vector<Byte>       disk = InfocomSaveBuilder::MakeDisk (InfocomSaveBuilder::MakeMemory (0x02), 0);
        InfocomSave::Position   position;



        Assert::IsTrue   (InfocomSave::TryReadPosition (disk, 0, position));
        Assert::IsTrue   (position.isTimed);
        Assert::AreEqual (std::string ("release 5: hall, time 7:12 AM"), InfocomSave::Describe (position));
    }


    //  A story in the table by its release and serial number has its title,
    //  and the maximum score where it has one.
    TEST_METHOD (Describe_AKnownStoryHasItsTitleAndMaximumScore)
    {
        InfocomSave::Position  position;



        position.release = 88;
        position.serial  = "840726";
        position.room    = L"Kitchen";
        position.score   = 10;
        position.moves   = 4;

        Assert::AreEqual (std::string ("Zork I (release 88): Kitchen, score 10 of 350, 4 moves"), InfocomSave::Describe (position));

        position.serial = "840727";
        Assert::AreEqual (std::string ("release 88: Kitchen, score 10, 4 moves"), InfocomSave::Describe (position), L"another serial number is another story");

        position.release = 27;
        position.serial  = "831005";
        position.isTimed = true;
        position.score   = 9;
        position.moves   = 5;

        Assert::AreEqual (std::string ("Deadline (release 27): Kitchen, time 9:05 AM"), InfocomSave::Describe (position));
        Assert::IsNull   (InfocomSave::FindStory (88, "123456"));
    }


    TEST_METHOD (FindPositions_OnlyTheSavedOnes)
    {
        std::vector<Byte>                    disk = InfocomSaveBuilder::MakeDisk (InfocomSaveBuilder::MakeMemory (0), 7);
        std::vector<Byte>                    blank (InfocomSaveBuilder::kDiskBytes, 0);
        std::vector<InfocomSave::Position>   positions;



        InfocomSave::FindPositions (disk, positions);
        Assert::AreEqual ((size_t) 1, positions.size());
        Assert::AreEqual (7, positions[0].number);

        InfocomSave::FindPositions (blank, positions);
        Assert::IsTrue (positions.empty());
    }
};
