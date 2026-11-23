#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33PayloadCodec
//
//  The header DOS 3.3 stores inside a file's data: the length of a tokenized
//  program, and the load address and length of a binary. Strips it on read
//  and rebuilds it on write.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33PayloadCodec
{
public:
    //  Strips whatever header the file type has, and reports the load address
    //  when the type records one.
    static void     ApplyTypeHeader    (Byte typeByte, vector<Byte> & inOutBytes, FilePayload & outPayload);

    //  Rebuilds whatever header the type stores inside the file's data, the
    //  exact inverse of ApplyTypeHeader, so a read followed by a write
    //  reproduces the stored form.
    static HRESULT  ComposeStoredBytes (const FilePayload & payload, vector<Byte> & outStored);

private:
    //  Payload headers DOS 3.3 stores inside the file's data.
    static constexpr size_t  kBinaryHeaderBytes    = 4;   // load address + length
    static constexpr size_t  kTokenizedHeaderBytes = 2;   // length only
};
