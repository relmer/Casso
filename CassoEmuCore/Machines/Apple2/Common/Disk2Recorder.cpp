#include "Pch.h"

#include "Machines/Apple2/Common/Disk2Recorder.h"
#include "Machines/Apple2/Common/Disk2Drive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Recorder::NoteRead
//
//  A nibble the drive assembled and the CPU took, once per nibble rather
//  than once per poll of the latch: to the watcher, the drive's access log,
//  and the recent nibbles while a disk is in the drive.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Recorder::NoteRead (Disk2Drive & drive, int index, Byte nibble, Byte flags, uint64_t cycle)
{
    m_watcher.ObserveNibble (nibble, cycle, GetInstructionAddress());
    drive.NoteRead (nibble);

    if (drive.HasDisk())
    {
        m_record.AddRead (index, nibble, flags, GetInstructionAddress(), drive.GetEngine().GetReadNibbles());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Recorder::NoteWrite
//
//  A load of the data latch in write mode. Every one goes to the watcher; the
//  recent nibbles have one per load while the motor runs, its spin-down
//  included, flagged when write protection drops it. A load with the motor
//  stopped writes nothing, and the //c's mode-register load is not a latch
//  load, so neither goes in the record.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2Recorder::NoteWrite (Disk2Drive & drive, int index, Byte nibble, bool isMotorOn)
{
    Byte  flags = 0;



    m_watcher.ObserveWrittenNibble (nibble);

    if (!isMotorOn || !drive.HasDisk())
    {
        return;
    }

    flags = static_cast<Byte> (Disk2NibbleRecord::kWritten | (drive.IsWriteProtected() ? Disk2NibbleRecord::kDropped : 0));

    m_record.Add (index, nibble, flags, GetInstructionAddress());
}
