#include "Pch.h"

#include "Machines/Apple2/Common/InfocomSave.h"
#include "Core/TextEncoding.h"
#include "FileTypes/ZMachineStory.h"





//  The DOS-order sector that has each physical sector of a track.
static constexpr int  s_kDosSectorOfPhysical[InfocomSave::kSectorsPerTrack] = { 0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15 };

//  The flag in a version 3 header's first flags byte for a story whose
//  status line has the time.
static constexpr Byte  s_kTimedFlag = 0x02;





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomSave::GetWord
//
////////////////////////////////////////////////////////////////////////////////

Word InfocomSave::GetWord (const std::vector<Byte> & memory, size_t at)
{
    return (at + 1 < memory.size()) ? (Word) ((memory[at] << 8) | memory[at + 1]) : (Word) 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomSave::TryReadPosition
//
//  The position's pages in physical order, then the header checked as a
//  story's: a version from 1 to 3, a serial number of six digits, and the
//  objects, the globals and the abbreviations in dynamic memory, which fits
//  the position.
//
////////////////////////////////////////////////////////////////////////////////

bool InfocomSave::TryReadPosition (const std::vector<Byte> & sectors, int number, Position & outPosition)
{
    constexpr size_t        kTrackBytes = kSectorsPerTrack * kPageBytes;
    std::vector<Byte>       memory;
    ZMachineStory::Header   header;
    size_t                  firstTrack  = (size_t) number * kTracksPerPosition;
    bool                    isValid     = number >= 0 && number < kPositions && (firstTrack + kTracksPerPosition) * kTrackBytes <= sectors.size();
    Word                    globals     = 0;
    int                     room        = 0;



    outPosition = Position();

    for (size_t page = kStatePages; isValid && page < (size_t) kTracksPerPosition * kSectorsPerTrack; page++)
    {
        size_t  at = (firstTrack + page / kSectorsPerTrack) * kTrackBytes + (size_t) s_kDosSectorOfPhysical[page % kSectorsPerTrack] * kPageBytes;

        memory.insert (memory.end(), sectors.begin() + (ptrdiff_t) at, sectors.begin() + (ptrdiff_t) (at + kPageBytes));
    }

    isValid = isValid && memory[0] >= 1 && memory[0] <= 3;

    for (size_t at = 0x12; isValid && at < 0x18; at++)
    {
        isValid = memory[at] >= '0' && memory[at] <= '9';
    }

    if (isValid)
    {
        header.version       = memory[0];
        header.flags1        = memory[1];
        header.release       = GetWord (memory, 0x02);
        header.objects       = GetWord (memory, 0x0A);
        header.globals       = GetWord (memory, 0x0C);
        header.staticMemory  = GetWord (memory, 0x0E);
        header.abbreviations = GetWord (memory, 0x18);

        isValid = header.staticMemory >= kHeaderBytes && header.staticMemory <= memory.size()
               && header.objects >= kHeaderBytes && header.objects < header.staticMemory
               && header.globals >= kHeaderBytes && header.globals + 6u <= header.staticMemory
               && header.abbreviations < header.staticMemory;
    }

    if (isValid)
    {
        memory.resize (header.staticMemory);

        globals = header.globals;
        room    = GetWord (memory, globals);

        outPosition.number  = number;
        outPosition.version = header.version;
        outPosition.release = header.release;
        outPosition.serial.assign ((const char *) &memory[0x12], 6);
        outPosition.isTimed = header.version == 3 && (header.flags1 & s_kTimedFlag) != 0;
        outPosition.score   = (int16_t) GetWord (memory, globals + 2u);
        outPosition.moves   = (int16_t) GetWord (memory, globals + 4u);
        outPosition.room    = (room > 0) ? ZMachineStory::ReadObjectName (memory, header, (size_t) room) : std::wstring();
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomSave::FindPositions
//
////////////////////////////////////////////////////////////////////////////////

void InfocomSave::FindPositions (const std::vector<Byte> & sectors, std::vector<Position> & outPositions)
{
    Position  position;



    outPositions.clear();

    for (int number = 0; number < kPositions; number++)
    {
        if (TryReadPosition (sectors, number, position))
        {
            outPositions.push_back (position);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomSave::Describe
//
////////////////////////////////////////////////////////////////////////////////

std::string InfocomSave::Describe (const Position & position)
{
    std::string  text = std::format ("release {}, serial number {}", position.release, position.serial);



    if (!position.room.empty())
    {
        text += ", " + TextEncoding::WideToNarrow (position.room);
    }

    text += position.isTimed ? std::format (", time {}:{:02}", position.score, position.moves)
                             : std::format (", score {}, {} {}", position.score, position.moves, (position.moves == 1) ? "move" : "moves");

    return text;
}
