#pragma once

#include "Pch.h"

#include "FakeDiskFileIo.h"
#include "DiskRunnerFixtures.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RealDiskFixtures
//
//  The Merlin disks the disk command scenarios run against, read from the
//  fixture folder, and the independently extracted copy of MAKE DUMP they are
//  checked against.
//
////////////////////////////////////////////////////////////////////////////////

class RealDiskFixtures
{
public:
    static void          SeedRealDisk            (FakeDiskFileIo & io, const char * fixture = "Disks/Merlin-proDos2.23.dsk", const char * asPath = DiskRunnerFixtures::kImage);
    static void          SeedProDosDisk          (FakeDiskFileIo & io);
    static vector<Byte>  ExpectedMakeDumpPayload ();
    static void          AssertIsMakeDumpPayload (const vector<Byte> & actual);
    static vector<Byte>  OriginalImageBytes      ();
};
