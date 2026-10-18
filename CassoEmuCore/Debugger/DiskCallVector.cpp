#include "Pch.h"

#include "Debugger/DiskCallVector.h"
#include "Debugger/IDebugExpressionContext.h"





static constexpr Word  kIobSlot      = 0x01;
static constexpr Word  kIobDrive     = 0x02;
static constexpr Word  kIobVolume    = 0x03;
static constexpr Word  kIobTrack     = 0x04;
static constexpr Word  kIobSector    = 0x05;
static constexpr Word  kIobBuffer    = 0x08;
static constexpr Word  kIobCommand   = 0x0C;
static constexpr Word  kDriverCommand = 0x42;
static constexpr Word  kDriverUnit    = 0x43;
static constexpr Word  kDriverBuffer  = 0x44;
static constexpr Word  kDriverBlock   = 0x46;
static constexpr Byte  kUnitDriveTwo  = 0x80;
static constexpr int   kSlotShift     = 4;
static constexpr Byte  kSlotMask      = 0x07;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCallVector::GetVectorAddress
//
//  A driver breakpoint on drive 2 follows drive 2's vector, and one on
//  either drive follows drive 1's: both vectors of a slot usually have the
//  same routine, and the unit at $43 indicates the drive.
//
////////////////////////////////////////////////////////////////////////////////

Word DiskCallVector::GetVectorAddress (const DiskBreakSpec & spec, int slot)
{
    Word  base = (spec.drive == DiskDriveFilter::Drive2) ? kDriveTwoBase : kDriveOneBase;



    return (spec.event == DiskEvent::RwtsCall) ? kRwtsVector : (Word) (base + 2 * slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCallVector::TryResolve
//
////////////////////////////////////////////////////////////////////////////////

std::optional<Word> DiskCallVector::TryResolve (const DiskBreakSpec & spec, int slot, const IDebugExpressionContext & memory)
{
    bool  isRwts   = spec.event == DiskEvent::RwtsCall;
    Word  vector   = GetVectorAddress (spec, slot);
    bool  hasJump  = isRwts ? Peek (memory, kRwtsVector) == kJmp : Peek (memory, kProDosGlobal) == kJmp;
    Word  routine  = isRwts ? Peek16 (memory, (Word) (vector + 1)) : Peek16 (memory, vector);



    return hasJump ? std::optional<Word> (routine) : std::nullopt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCallVector::AddVectorBytes
//
//  The bytes a write to which can move the routine: the JMP and its address
//  for RWTS; ProDOS's global JMP and the device vector for the driver.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCallVector::AddVectorBytes (const DiskBreakSpec & spec, int slot, std::vector<Word> & bytes)
{
    Word  vector = GetVectorAddress (spec, slot);



    if (spec.event == DiskEvent::RwtsCall)
    {
        bytes.insert (bytes.end(), { kRwtsVector, (Word) (kRwtsVector + 1), (Word) (kRwtsVector + 2) });
    }
    else
    {
        bytes.insert (bytes.end(), { kProDosGlobal, vector, (Word) (vector + 1) });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCallVector::IsRequestMatch
//
////////////////////////////////////////////////////////////////////////////////

bool DiskCallVector::IsRequestMatch (const DiskBreakSpec & spec, const IDebugExpressionContext & context)
{
    DiskCallRequest  request = ReadRequest (spec, context);
    bool             isDrive = spec.drive == DiskDriveFilter::Either || request.drive == ((spec.drive == DiskDriveFilter::Drive1) ? 1 : 2);



    return isDrive &&
           (!spec.volume.has_value()  || *spec.volume  == request.volume)  &&
           (!spec.track.has_value()   || *spec.track   == request.track)   &&
           (!spec.sector.has_value()  || *spec.sector  == request.sector)  &&
           (!spec.block.has_value()   || *spec.block   == request.block)   &&
           (!spec.command.has_value() || *spec.command == request.command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCallVector::ReadRequest
//
////////////////////////////////////////////////////////////////////////////////

DiskCallRequest DiskCallVector::ReadRequest (const DiskBreakSpec & spec, const IDebugExpressionContext & context)
{
    DiskCallRequest  request;
    Word             iob     = GetIob (context);



    if (spec.event == DiskEvent::RwtsCall)
    {
        request.slot    = Peek (context, (Word) (iob + kIobSlot)) >> kSlotShift;
        request.drive   = Peek (context, (Word) (iob + kIobDrive));
        request.volume  = Peek (context, (Word) (iob + kIobVolume));
        request.track   = Peek (context, (Word) (iob + kIobTrack));
        request.sector  = Peek (context, (Word) (iob + kIobSector));
        request.buffer  = Peek16 (context, (Word) (iob + kIobBuffer));
        request.command = Peek (context, (Word) (iob + kIobCommand));
    }
    else
    {
        request.isDriver = true;
        request.command  = Peek (context, kDriverCommand);
        request.unit     = Peek (context, kDriverUnit);
        request.slot     = (request.unit >> kSlotShift) & kSlotMask;
        request.drive    = ((request.unit & kUnitDriveTwo) != 0) ? 2 : 1;
        request.buffer   = Peek16 (context, kDriverBuffer);
        request.block    = Peek16 (context, kDriverBlock);
    }

    return request;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCallVector::GetIob
//
//  RWTS's caller points Y (low) and A (high) at the IOB.
//
////////////////////////////////////////////////////////////////////////////////

Word DiskCallVector::GetIob (const IDebugExpressionContext & context)
{
    Word  a = 0;
    Word  y = 0;



    context.TryGetRegister ("A", a);
    context.TryGetRegister ("Y", y);
    return (Word) ((y & 0xFF) | ((a & 0xFF) << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCallVector::Peek
//
////////////////////////////////////////////////////////////////////////////////

Byte DiskCallVector::Peek (const IDebugExpressionContext & memory, Word address)
{
    Byte  value = 0;



    memory.TryPeek (address, value);
    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCallVector::Peek16
//
////////////////////////////////////////////////////////////////////////////////

Word DiskCallVector::Peek16 (const IDebugExpressionContext & memory, Word address)
{
    return (Word) (Peek (memory, address) | (Peek (memory, (Word) (address + 1)) << 8));
}
