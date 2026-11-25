#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "FileTypes/WizardryMaze.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryMazeTests
//
//  Wizardry V's maze: the square a character's record saves, a level's
//  walls, and the disk sides found beside the saved game.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WizardryMazeTests)
{
public:

    static constexpr size_t  kDiskBytes = 143360;


    //  A disk side in block order, as a .po file has it, with its side
    //  number and one byte of its walls block.
    static std::vector<Byte> MakeBlockOrderSide (int side, Byte firstWall)
    {
        std::vector<Byte>  bytes (kDiskBytes, 0);



        bytes[WizardryMaze::kSideBlock * WizardryMaze::kWallsBytes + WizardryMaze::kSideAt] = (Byte) side;
        bytes[WizardryMaze::kWallsBlock * WizardryMaze::kWallsBytes]                        = firstWall;

        return bytes;
    }


    //  The same side as sectors in DOS order, as the decoder gets it.
    static std::vector<Byte> MakeSectors (int side, Byte firstWall)
    {
        std::vector<Byte>  sectors (kDiskBytes, 0);



        sectors[ProDosSkeleton::GetBlockByteOffset (kDiskBytes, WizardryMaze::kSideBlock, WizardryMaze::kSideAt)] = (Byte) side;
        sectors[ProDosSkeleton::GetBlockByteOffset (kDiskBytes, WizardryMaze::kWallsBlock, 0)]                   = firstWall;

        return sectors;
    }


    //  The words saved for the four squares the party quit the game on, and
    //  the far corner of the deepest level.
    TEST_METHOD (ReadSquare_DecodesTheSavedWord)
    {
        struct { int word; int x; int y; int level; }  cases[] =
        {
            { 0x0421, 1, 4,  1 },
            { 0x0439, 1, 7,  1 },
            { 0x0513, 3, 10, 1 },
            { 0x0551, 9, 10, 1 },
            { 7 | (7 << 3) | (15 << 6) | (8 << 10), 31, 31, 8 },
        };



        for (const auto & expected : cases)
        {
            WizardryMaze::Square  square = WizardryMaze::ReadSquare (expected.word);

            Assert::AreEqual (expected.x,     square.x);
            Assert::AreEqual (expected.y,     square.y);
            Assert::AreEqual (expected.level, square.level);
        }
    }


    //  A square's east side from the first table, its north side from the
    //  second, by the word for its column and 8-row band. Sides off the edge
    //  wrap to the far one.
    TEST_METHOD (GetWall_ReadsEachTableAndWraps)
    {
        std::vector<Byte>  walls (WizardryMaze::kWallsBytes, 0);
        size_t             band = 2 * (size_t) WizardryMaze::kSide;



        walls[2 * 3]                                       = 1 << 4;      // (3, 2) east: a wall
        walls[band + 2 * 5 + 1]                            = 2 << 2;      // (5, 13) east: a door
        walls[WizardryMaze::kNorthAt + 2 * 31]             = 3;           // (31, 0) north: a hidden door

        Assert::AreEqual (1, WizardryMaze::GetWall (walls, 3,  2,  false));
        Assert::AreEqual (2, WizardryMaze::GetWall (walls, 5,  13, false));
        Assert::AreEqual (3, WizardryMaze::GetWall (walls, 31, 0,  true));
        Assert::AreEqual (3, WizardryMaze::GetWall (walls, -1, 32, true), L"wraps to (31, 0)");
        Assert::AreEqual (0, WizardryMaze::GetWall (walls, 3,  2,  true));
    }


    //  Level n's walls are on side n + 1, whatever order the sides come in.
    TEST_METHOD (FindWalls_PicksTheSideOfTheLevel)
    {
        std::vector<std::vector<Byte>>  sides { MakeSectors (4, 0x44), MakeSectors (2, 0x22), MakeSectors (3, 0x33) };
        std::vector<Byte>               walls;



        Assert::IsTrue   (WizardryMaze::FindWalls (sides, 2, walls));
        Assert::AreEqual (WizardryMaze::kWallsBytes, walls.size());
        Assert::AreEqual ((int) 0x33, (int) walls[0]);
        Assert::IsTrue   (WizardryMaze::FindWalls (sides, 1, walls));
        Assert::AreEqual ((int) 0x22, (int) walls[0]);
        Assert::IsFalse  (WizardryMaze::FindWalls (sides, 5, walls), L"no side 6");
        Assert::IsFalse  (WizardryMaze::FindWalls (sides, 0, walls));
    }


    //  The maze sides beside the saved game: other files with its extension
    //  and a maze side's number. The saved game's disk, a file with another
    //  extension and one too big to be a disk side are passed over.
    TEST_METHOD (FindSides_KeepsTheMazeSidesInTheFolder)
    {
        FakeDiskFileIo                  fileIo;
        std::vector<std::vector<Byte>>  sides;
        std::vector<Byte>               big  (2 * 1024 * 1024, 0);



        fileIo.files["C:\\Wiz\\A.po"]   = MakeBlockOrderSide (1, 0x11);
        fileIo.files["C:\\Wiz\\B.po"]   = MakeBlockOrderSide (2, 0x22);
        fileIo.files["C:\\Wiz\\C.dsk"]  = MakeBlockOrderSide (3, 0x33);
        fileIo.files["C:\\Wiz\\Big.po"] = big;

        for (const auto & [path, bytes] : fileIo.files)
        {
            fileIo.stamps[path] = FileStamp { bytes.size(), 0 };
        }

        WizardryMaze::FindSides (fileIo, "C:\\Wiz\\A.po", sides);

        Assert::AreEqual ((size_t) 1, sides.size());
        Assert::AreEqual (2, WizardryMaze::GetSide (sides[0]));
        Assert::AreEqual ((int) 0x22, (int) sides[0][ProDosSkeleton::GetBlockByteOffset (sides[0].size(), WizardryMaze::kWallsBlock, 0)]);
    }
};
