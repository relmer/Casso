#pragma once

#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendTestDisks
//
//  The disk images and manifest rows the Full Send tests are built from.
//
////////////////////////////////////////////////////////////////////////////////

namespace FullSendTestDisks
{
    using Image = std::vector<Byte>;

    constexpr size_t  kTrackBytes = 16 * 256;

    inline Image  Dos (const char * program, Byte seed = 1)
    {
        Image        buffer (NibblizationLayer::kImageByteSize, 0);
        Image        next;
        FilePayload  payload;

        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;
        payload.bytes.assign (600, seed);

        AssertSucceeded (Dos33Skeleton::Write (buffer, 254));
        AssertSucceeded (Dos33Volume (buffer).Write (FilePath::Parse (program), payload, next));

        return next;
    }

    //  A ProDOS disk with one file, so it isn't blank.
    inline Image  ProDos (const char * volume)
    {
        Image        buffer (NibblizationLayer::kImageByteSize, 0);
        Image        next;
        FilePayload  payload;

        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;
        payload.bytes.assign (600, 1);

        AssertSucceeded (ProDosSkeleton::Write (buffer, volume));
        AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("DATA"), payload, next));

        return next;
    }

    //  A formatted disk with no files.
    inline Image  BlankProDos (const char * volume)
    {
        Image  buffer (NibblizationLayer::kImageByteSize, 0);

        AssertSucceeded (ProDosSkeleton::Write (buffer, volume));

        return buffer;
    }

    inline Image  BlankDos()
    {
        Image  buffer (NibblizationLayer::kImageByteSize, 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, 254));

        return buffer;
    }

    //  A DOS 3.3 disk whose one file is a BASIC program with the given bytes.
    inline Image  DosProgram (std::initializer_list<Byte> program)
    {
        Image        next;
        FilePayload  payload;

        payload.type  = Dos33Volume::kTypeApplesoft;
        payload.bytes = program;

        AssertSucceeded (Dos33Volume (BlankDos()).Write (FilePath::Parse ("HELLO"), payload, next));

        return next;
    }

    //  An unrecognized disk, with bytes of its seed in every sector.
    inline Image  Other (Byte seed)
    {
        return Image (NibblizationLayer::kImageByteSize, seed);
    }

    inline FullSendRow  Row (int sequence, const std::string & file, char fingerprint = 'a', char hash = 'b')
    {
        FullSendRow  row;

        row.sequence    = sequence;
        row.file        = file;
        row.drive       = 2;
        row.fingerprint = std::string (64, fingerprint);
        row.hash        = std::string (64, hash);
        row.time        = "2026-10-30T12:00:00Z";
        row.unreadable  = 3;
        row.status      = FullSendManifest::kStatusProtected;

        return row;
    }
}
