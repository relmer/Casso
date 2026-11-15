#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/InfocomSave.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomSaveTests
//
//  A save disk built here as the interpreter writes one: a version 3
//  story's dynamic memory, with one object, HALL, and the room, score and
//  moves in its first globals, after three pages of state, from track 8 for
//  position 2, in physical sector order. The real save is in
//  ScenarioTests/GameDataTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InfocomSaveTests)
{
public:

    static constexpr size_t  kDiskBytes  = 143360;
    static constexpr size_t  kObjects    = 0x40;
    static constexpr size_t  kGlobals    = 0x100;
    static constexpr size_t  kNameTable  = 0x180;
    static constexpr size_t  kStatic     = 0x200;


    static void PutWord (std::vector<Byte> & bytes, size_t at, int value)
    {
        bytes[at]     = (Byte) ((value >> 8) & 0xFF);
        bytes[at + 1] = (Byte) (value & 0xFF);
    }


    //  The story's dynamic memory, `flags` the header's first flags byte.
    static std::vector<Byte> MakeMemory (Byte flags)
    {
        std::vector<Byte>  memory (kStatic, 0);
        size_t             entry = kObjects + 31 * 2;



        memory[0] = 3;
        memory[1] = flags;
        PutWord (memory, 0x02, 5);
        PutWord (memory, 0x0A, (int) kObjects);
        PutWord (memory, 0x0C, (int) kGlobals);
        PutWord (memory, 0x0E, (int) kStatic);
        memcpy (&memory[0x12], "123456", 6);

        //  Object 1's property table: its name in two words, "hall" as the
        //  Z-characters 13, 6, 17, 17, then two pads of 5.
        PutWord (memory, entry + 7, (int) kNameTable);
        memory[kNameTable] = 2;
        PutWord (memory, kNameTable + 1, (13 << 10) | (6 << 5) | 17);
        PutWord (memory, kNameTable + 3, 0x8000 | (17 << 10) | (5 << 5) | 5);

        PutWord (memory, kGlobals,     1);
        PutWord (memory, kGlobals + 2, 7);
        PutWord (memory, kGlobals + 4, 12);

        return memory;
    }


    //  A disk with `memory` saved at `position`, in DOS sector order.
    static std::vector<Byte> MakeDisk (const std::vector<Byte> & memory, int position)
    {
        static constexpr int  kDosSectorOfPhysical[16] = { 0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15 };

        std::vector<Byte>  disk (kDiskBytes, 0);
        std::vector<Byte>  stream (InfocomSave::kStatePages * InfocomSave::kPageBytes, 0xA5);



        stream.insert (stream.end(), memory.begin(), memory.end());
        stream.resize ((stream.size() + 255) / 256 * 256, 0);

        for (size_t page = 0; page * 256 < stream.size(); page++)
        {
            size_t  track = (size_t) position * InfocomSave::kTracksPerPosition + page / 16;
            size_t  at    = (track * 16 + (size_t) kDosSectorOfPhysical[page % 16]) * 256;

            memcpy (&disk[at], &stream[page * 256], 256);
        }

        return disk;
    }


    TEST_METHOD (TryReadPosition_ReadsTheRoomScoreAndMoves)
    {
        std::vector<Byte>       disk = MakeDisk (MakeMemory (0), 2);
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


    //  A version 3 story with the timed flag has the hours and minutes in
    //  the globals of the score and the moves.
    TEST_METHOD (Describe_ATimedStoryHasTheTime)
    {
        std::vector<Byte>       disk = MakeDisk (MakeMemory (0x02), 0);
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
        std::vector<Byte>                    disk = MakeDisk (MakeMemory (0), 7);
        std::vector<Byte>                    blank (kDiskBytes, 0);
        std::vector<InfocomSave::Position>   positions;



        InfocomSave::FindPositions (disk, positions);
        Assert::AreEqual ((size_t) 1, positions.size());
        Assert::AreEqual (7, positions[0].number);

        InfocomSave::FindPositions (blank, positions);
        Assert::IsTrue (positions.empty());
    }
};
