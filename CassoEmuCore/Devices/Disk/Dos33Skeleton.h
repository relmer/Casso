#pragma once

#include "Pch.h"

#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Skeleton
//
//  Pure DOS 3.3 filesystem structures for a fresh 143,360-byte sector buffer
//  (spec 017 R-004/R-006). Write lays down an INIT-compatible empty volume:
//  VTOC at T17 S0 (catalog chain T17 S15 -> S1, tracks 0-2 + 17 allocated in
//  the free bitmap), every other sector zero. InstallDos copies the DOS image
//  (tracks 0-2) from a caller-supplied System Master sector image so the disk
//  boots; Dos33FileWriter adds the minimal HELLO greeting a booted DOS runs.
//
//  The buffer is always in DOS 3.3 LOGICAL sector layout (track-major,
//  256-byte sectors); format ordering is applied later by the builder.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33Skeleton
{
public:
    //  Empty formatted volume (data-only). Buffer must be kImageByteSize.
    static HRESULT  Write (vector<Byte> & buffer, Byte volumeNumber);

    //  Copies tracks 0-2 verbatim from the System Master image (R-006).
    static HRESULT  InstallDos (vector<Byte> & buffer, const vector<Byte> & masterSectors);
};





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33FileWriter
//
//  Writes a file into a Dos33Skeleton-formatted buffer: catalog entry,
//  track/sector list, data sectors, VTOC bitmap kept honest. v1 needs only
//  the minimal Applesoft HELLO greeting (R-006).
//
////////////////////////////////////////////////////////////////////////////////

class Dos33FileWriter
{
public:
    static HRESULT  WriteHello (vector<Byte> & buffer);
};
