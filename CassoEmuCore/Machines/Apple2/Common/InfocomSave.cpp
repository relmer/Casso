#include "Pch.h"

#include "Machines/Apple2/Common/InfocomSave.h"
#include "Core/TextEncoding.h"
#include "FileTypes/ZMachineStory.h"





//  The DOS-order sector that has each physical sector of a track.
static constexpr int  s_kDosSectorOfPhysical[InfocomSave::kSectorsPerTrack] = { 0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15 };

//  The flag in a version 3 header's first flags byte for a story whose
//  status line has the time.
static constexpr Byte  s_kTimedFlag = 0x02;

//  Every version 1 to 3 story with a serial number of six digits, by its
//  release and serial number, from Paul David Doherty's Infocom Fact Sheet
//  (version 4.95). The fact sheet doesn't list scores, so the maximum
//  scores are added separately, and are zero for a timed story and for
//  one whose maximum isn't known.
static constexpr InfocomSave::Story  s_kStories[] =
{
    {   1, "841226", "A Mind Forever Voyaging",                0 },
    {  97, "851218", "Ballyhoo",                             200 },
    {  99, "861014", "Ballyhoo",                             200 },
    {  23, "840809", "Cutthroats",                           250 },
    {  25, "840917", "Cutthroats",                           250 },
    {  18, "820311", "Deadline",                               0 },
    {  19, "820427", "Deadline",                               0 },
    {  21, "820512", "Deadline",                               0 },
    {  22, "820809", "Deadline",                               0 },
    {  26, "821108", "Deadline",                               0 },
    {  27, "831005", "Deadline",                               0 },
    {  28, "850129", "Deadline",                               0 },
    {  10, "830810", "Enchanter",                            400 },
    {  15, "831107", "Enchanter",                            400 },
    {  16, "831118", "Enchanter",                            400 },
    {  16, "840518", "Enchanter",                            400 },
    {  24, "851118", "Enchanter",                            400 },
    {  29, "860820", "Enchanter",                            400 },
    {  62, "840706", "The Hitchhiker's Guide to the Galaxy", 400 },
    { 108, "840809", "The Hitchhiker's Guide to the Galaxy", 400 },
    { 119, "840822", "The Hitchhiker's Guide to the Galaxy", 400 },
    {  47, "840914", "The Hitchhiker's Guide to the Galaxy", 400 },
    {  56, "841221", "The Hitchhiker's Guide to the Galaxy", 400 },
    {  58, "851002", "The Hitchhiker's Guide to the Galaxy", 400 },
    {  59, "851108", "The Hitchhiker's Guide to the Galaxy", 400 },
    {  60, "861002", "The Hitchhiker's Guide to the Galaxy", 400 },
    { 235, "861118", "Hollywood Hijinx",                     150 },
    {  37, "861215", "Hollywood Hijinx",                     150 },
    {  22, "830916", "Infidel",                              400 },
    {  22, "840522", "Infidel",                              400 },
    {  48, "860103", "Leather Goddesses of Phobos",          316 },
    {  57, "860121", "Leather Goddesses of Phobos",          316 },
    { 118, "860325", "Leather Goddesses of Phobos",          316 },
    { 160, "860521", "Leather Goddesses of Phobos",          316 },
    {  50, "860711", "Leather Goddesses of Phobos",          316 },
    {  59, "860730", "Leather Goddesses of Phobos",          316 },
    {  59, "861114", "Leather Goddesses of Phobos",          316 },
    { 203, "870506", "Lurking Horror",                       100 },
    { 219, "870912", "Lurking Horror",                       100 },
    { 221, "870918", "Lurking Horror",                       100 },
    {   4, "860918", "Moonmist",                               0 },
    {   9, "861022", "Moonmist",                               0 },
    {  13, "880501", "Moonmist",                               0 },
    {   1, "830306", "Planetfall",                            80 },
    {   1, "830517", "Planetfall",                            80 },
    {   1, "830614", "Planetfall",                            80 },
    {   6, "830619", "Planetfall",                            80 },
    {  20, "830708", "Planetfall",                            80 },
    {  26, "831014", "Planetfall",                            80 },
    {  29, "840118", "Planetfall",                            80 },
    {  37, "851003", "Planetfall",                            80 },
    {  39, "880501", "Planetfall",                            80 },
    {  26, "870730", "Plundered Hearts",                      25 },
    {  86, "840320", "Seastalker",                           100 },
    {  15, "840501", "Seastalker",                           100 },
    {  15, "840522", "Seastalker",                           100 },
    {  15, "840612", "Seastalker",                           100 },
    {  15, "840716", "Seastalker",                           100 },
    {  17, "850208", "Seastalker",                           100 },
    {  16, "850515", "Seastalker",                           100 },
    {  16, "850603", "Seastalker",                           100 },
    {  18, "850919", "Seastalker",                           100 },
    {  17, "831001", "Sorcerer",                             400 },
    {  67, "831208", "Sorcerer",                             400 },
    {  85, "840106", "Sorcerer",                             400 },
    {   4, "840131", "Sorcerer",                             400 },
    {   6, "840508", "Sorcerer",                             400 },
    {  13, "851021", "Sorcerer",                             400 },
    {  15, "851108", "Sorcerer",                             400 },
    {  18, "860904", "Sorcerer",                             400 },
    {  63, "850916", "Spellbreaker",                         600 },
    {  87, "860904", "Spellbreaker",                         600 },
    {  15, "820901", "Starcross",                            400 },
    {  17, "821021", "Starcross",                            400 },
    {  18, "830114", "Starcross",                            400 },
    {  28, "861221", "Stationfall",                           80 },
    {  36, "870106", "Stationfall",                           80 },
    {  63, "870218", "Stationfall",                           80 },
    {  87, "870326", "Stationfall",                           80 },
    { 107, "870430", "Stationfall",                           80 },
    {  14, "841005", "Suspect",                                0 },
    {  18, "850222", "Suspect",                                0 },
    {   5, "830222", "Suspended",                              0 },
    {   7, "830419", "Suspended",                              0 },
    {   8, "830521", "Suspended",                              0 },
    {   8, "840521", "Suspended",                              0 },
    {  68, "850501", "Wishbringer",                          100 },
    {  69, "850920", "Wishbringer",                          100 },
    {  13, "830524", "Witness",                                0 },
    {  18, "830910", "Witness",                                0 },
    {  20, "831119", "Witness",                                0 },
    {  21, "831208", "Witness",                                0 },
    {  22, "840924", "Witness",                                0 },
    {  23, "840925", "Witness",                                0 },
    {  23, "820428", "Zork I",                               350 },
    {  25, "820515", "Zork I",                               350 },
    {  26, "820803", "Zork I",                               350 },
    {  28, "821013", "Zork I",                               350 },
    {  30, "830330", "Zork I",                               350 },
    {  75, "830929", "Zork I",                               350 },
    {  76, "840509", "Zork I",                               350 },
    {  88, "840726", "Zork I",                               350 },
    { 119, "880429", "Zork I",                               350 },
    {  15, "820308", "Zork II",                              400 },
    {  17, "820427", "Zork II",                              400 },
    {  18, "820512", "Zork II",                              400 },
    {  18, "820517", "Zork II",                              400 },
    {  19, "820721", "Zork II",                              400 },
    {  22, "830331", "Zork II",                              400 },
    {  23, "830411", "Zork II",                              400 },
    {  22, "840518", "Zork II",                              400 },
    {  48, "840904", "Zork II",                              400 },
    {  63, "860811", "Zork II",                              400 },
    {  10, "820818", "Zork III",                               7 },
    {  12, "821025", "Zork III",                               7 },
    {  15, "830331", "Zork III",                               7 },
    {  16, "830410", "Zork III",                               7 },
    {  15, "840518", "Zork III",                               7 },
    {  17, "840727", "Zork III",                               7 },
    {  25, "860811", "Zork III",                               7 },
    {   2, "840207", "Mini-Zork I",                            0 },
    {  34, "871124", "Mini-Zork I",                            0 },
    {   2, "871123", "Mini-Zork II",                           0 },
    {  15, "840330", "Zork I Demo",                            0 },
    {   5, "840512", "Sampler I",                              0 },
    {  24, "840627", "Sampler I",                              0 },
    {  26, "840731", "Sampler I",                              0 },
    {  52, "850402", "Sampler I",                              0 },
    {  53, "850407", "Sampler I",                              0 },
    {  55, "850823", "Sampler I",                              0 },
    {   8, "870119", "Mini-Ballyhoo",                          0 },
    {  97, "870601", "Sampler II",                             0 },
    {   8, "870601", "Sampler III",                            0 },
};





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
//  A timed story's hours are 0 to 23, written on a 12-hour clock as on
//  its status line.
//
////////////////////////////////////////////////////////////////////////////////

std::string InfocomSave::Describe (const Position & position)
{
    const Story *  story = FindStory (position.release, position.serial);
    std::string    text  = story ? std::format ("{} (release {})", story->title, position.release) : std::format ("release {}", position.release);
    std::string    max   = (story && story->maxScore > 0) ? std::format (" of {}", story->maxScore) : std::string();
    int            hours = (position.score % 12 == 0) ? 12 : position.score % 12;



    if (!position.room.empty())
    {
        text += ": " + TextEncoding::WideToNarrow (position.room);
    }

    text += position.isTimed ? std::format (", time {}:{:02} {}", hours, position.moves, (position.score < 12) ? "AM" : "PM")
                             : std::format (", score {}{}, {} {}", position.score, max, position.moves, (position.moves == 1) ? "move" : "moves");

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomSave::FindStory
//
////////////////////////////////////////////////////////////////////////////////

const InfocomSave::Story * InfocomSave::FindStory (int release, const std::string & serial)
{
    const Story *  found = nullptr;



    for (const Story & story : s_kStories)
    {
        if (found == nullptr && story.release == release && serial == story.serial)
        {
            found = &story;
        }
    }

    return found;
}
