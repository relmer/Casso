#pragma once

#include "Pch.h"
#include "Machines/Apple2/Common/InfocomSave.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomSaveBuilder
//
//  An Infocom save disk built in memory: a version 3 story's dynamic memory
//  with the room "hall", a score and moves, saved at a chosen position in
//  DOS sector order.
//
////////////////////////////////////////////////////////////////////////////////

class InfocomSaveBuilder
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

        std::vector<Byte>  disk   (kDiskBytes, 0);
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
};
