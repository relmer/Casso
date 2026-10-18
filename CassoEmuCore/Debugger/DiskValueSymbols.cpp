#include "Pch.h"

#include "Debugger/DiskValueSymbols.h"
#include "Debugger/IDebugTarget.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Machines/Apple2/Common/Disk2NibbleRecord.h"





static constexpr const char *  s_kDiskValues[] =
{
    "DISK.DRIVE",   "DISK.QTRACK",    "DISK.QTRACK1",  "DISK.QTRACK2",
    "DISK.MOTOR",   "DISK.WRITING",   "DISK.PROTECTED", "DISK.INSERTED",
    "DISK.ATRACK",  "DISK.ASECTOR",   "DISK.AVOLUME",
};

static constexpr Byte    kPrologue[]   = { 0xD5, 0xAA };
static constexpr Byte    kAddress16    = 0x96;
static constexpr Byte    kAddress13    = 0xB5;
static constexpr size_t  kFieldLength  = 11;      // the prologue and four 4-and-4 pairs





////////////////////////////////////////////////////////////////////////////////
//
//  DiskValueSymbols::IsDiskValue
//
////////////////////////////////////////////////////////////////////////////////

bool DiskValueSymbols::IsDiskValue (const std::string & name)
{
    std::string  upper = ToUpper (name);



    return std::ranges::any_of (s_kDiskValues, [&] (const char * value) { return upper == value; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskValueSymbols::TryResolve
//
////////////////////////////////////////////////////////////////////////////////

bool DiskValueSymbols::TryResolve (const std::string & name, const IDebugTarget & target, Word & value)
{
    DebugDiskState  state;
    std::string     upper    = ToUpper (name);
    bool            isKnown  = IsDiskValue (name) && target.TryGetDiskState (state);
    int             drive    = state.selectedDrive;
    AddressField    field;



    if (!isKnown)
    {
        return false;
    }

    if (upper.starts_with ("DISK.A"))
    {
        field = FindLastAddressField (target, drive);
    }

    if      (upper == "DISK.DRIVE")     { value = (Word) (drive + 1);                     }
    else if (upper == "DISK.QTRACK")    { value = (Word) state.quarterTrack[drive];       }
    else if (upper == "DISK.QTRACK1")   { value = (Word) state.quarterTrack[0];           }
    else if (upper == "DISK.QTRACK2")   { value = (Word) state.quarterTrack[1];           }
    else if (upper == "DISK.MOTOR")     { value = state.isMotorOn ? 1 : 0;                }
    else if (upper == "DISK.WRITING")   { value = state.isWriteMode ? 1 : 0;              }
    else if (upper == "DISK.PROTECTED") { value = state.isProtected[drive] ? 1 : 0;       }
    else if (upper == "DISK.INSERTED")  { value = state.hasDisk[drive] ? 1 : 0;           }
    else if (upper == "DISK.ATRACK")    { value = field.track;                            }
    else if (upper == "DISK.ASECTOR")   { value = field.sector;                           }
    else                                { value = field.volume;                           }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskValueSymbols::FindLastAddressField
//
//  The record read backward, the drive's own reads only: the latest
//  prologue whose eight nibbles after it decode to a volume, track and
//  sector whose exclusive or equals the checksum.
//
////////////////////////////////////////////////////////////////////////////////

DiskValueSymbols::AddressField DiskValueSymbols::FindLastAddressField (const IDebugTarget & target, int drive)
{
    const Disk2NibbleRecord  * record = target.GetDiskRecord();
    Byte                       wanted = (drive != 0) ? Disk2NibbleRecord::kDrive2 : 0;
    std::vector<Byte>          nibbles;
    AddressField               found;
    uint64_t                   first  = (record != nullptr) ? record->GetCount() - record->GetSize() : 0;
    uint64_t                   index  = (record != nullptr) ? record->GetCount() : 0;



    //  The drive's reads, latest first.
    while (index > first)
    {
        const Disk2NibbleRecord::Entry  & entry = record->GetEntry (--index);



        if ((entry.flags & (Disk2NibbleRecord::kDrive2 | Disk2NibbleRecord::kWritten)) == wanted)
        {
            nibbles.push_back (entry.nibble);
        }
    }

    std::ranges::reverse (nibbles);

    for (size_t at = nibbles.size(); at >= kFieldLength && found.track == kNoField; at--)
    {
        size_t  start    = at - kFieldLength;
        bool    isMark   = nibbles[start] == kPrologue[0] && nibbles[start + 1] == kPrologue[1] &&
                           (nibbles[start + 2] == kAddress16 || nibbles[start + 2] == kAddress13);
        Byte    volume   = DiskFieldFormat::Decode44 (nibbles[start + 3],  nibbles[start + 4]);
        Byte    track    = DiskFieldFormat::Decode44 (nibbles[start + 5],  nibbles[start + 6]);
        Byte    sector   = DiskFieldFormat::Decode44 (nibbles[start + 7],  nibbles[start + 8]);
        Byte    checksum = DiskFieldFormat::Decode44 (nibbles[start + 9],  nibbles[start + 10]);



        if (isMark && (volume ^ track ^ sector) == checksum)
        {
            found = { volume, track, sector };
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskValueSymbols::ToUpper
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskValueSymbols::ToUpper (const std::string & name)
{
    std::string  upper = name;



    std::ranges::transform (upper, upper.begin(), [] (char ch) { return (char) toupper ((unsigned char) ch); });
    return upper;
}
