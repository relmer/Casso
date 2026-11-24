#include "Pch.h"

#include "Machines/Apple2/Common/Dos33PayloadCodec.h"
#include "Machines/Apple2/Common/Dos33Volume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33PayloadCodec::ApplyTypeHeader
//
//  DOS 3.3 stores a file's length -- and, for binaries, its load address --
//  inside the file's own first bytes rather than in the catalog. Stripping that
//  header is what turns "the sectors this file occupies" into "the file".
//
//  A header claiming more than the sectors hold is trusted only as far as the
//  data goes: a truncated file is reported at the length actually present
//  rather than padded out to the length it claims.
//
////////////////////////////////////////////////////////////////////////////////

void Dos33PayloadCodec::ApplyTypeHeader (Byte typeByte, vector<Byte> & inOutBytes, FilePayload & outPayload)
{
    Byte    type    = (Byte) (typeByte & ~Dos33Volume::kLockedBit);
    size_t  claimed = 0;
    size_t  header  = 0;



    if (type == Dos33Volume::kTypeBinary)
    {
        if (inOutBytes.size() < kBinaryHeaderBytes)
        {
            return;
        }

        outPayload.loadAddress    = (Word) (inOutBytes[0] | (inOutBytes[1] << 8));
        outPayload.hasLoadAddress = true;

        claimed = (size_t) (inOutBytes[2] | (inOutBytes[3] << 8));
        header  = kBinaryHeaderBytes;
    }
    else if (type == Dos33Volume::kTypeApplesoft || type == Dos33Volume::kTypeInteger)
    {
        if (inOutBytes.size() < kTokenizedHeaderBytes)
        {
            return;
        }

        claimed = (size_t) (inOutBytes[0] | (inOutBytes[1] << 8));
        header  = kTokenizedHeaderBytes;
    }
    else if (type == Dos33Volume::kTypeText)
    {
        // Sequential text ends at the first zero byte; the rest of the last
        // sector is padding the guest never sees.
        size_t  end = 0;

        while (end < inOutBytes.size() && inOutBytes[end] != 0)
        {
            end++;
        }

        inOutBytes.resize (end);
        return;
    }
    else
    {
        return;   // no header this layer knows how to strip
    }

    inOutBytes.erase (inOutBytes.begin(), inOutBytes.begin() + (ptrdiff_t) header);

    if (claimed < inOutBytes.size())
    {
        inOutBytes.resize (claimed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33PayloadCodec::ComposeStoredBytes
//
//  The exact inverse of ApplyTypeHeader: DOS 3.3 records a file's length -- and
//  a binary's load address -- inside the file's own first bytes, so placing a
//  file means rebuilding that header rather than storing the payload as it came.
//
//  A binary with no load address is refused rather than defaulted. $0000 is a
//  legal load address, so a default would be indistinguishable from an answer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Dos33PayloadCodec::ComposeStoredBytes (const FilePayload & payload, vector<Byte> & outStored)
{
    HRESULT  hr      = S_OK;
    Byte     type    = (Byte) (payload.type & ~Dos33Volume::kLockedBit);
    size_t   length  = payload.bytes.size();
    bool     fits    = length <= 0xFFFF;
    bool     hasAddr = payload.hasLoadAddress;



    outStored.clear();

    if (type == Dos33Volume::kTypeBinary)
    {
        CBREx (hasAddr, HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER));
        CBREx (fits,    HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE));

        outStored.push_back ((Byte) (payload.loadAddress & 0xFF));
        outStored.push_back ((Byte) (payload.loadAddress >> 8));
        outStored.push_back ((Byte) (length & 0xFF));
        outStored.push_back ((Byte) ((length >> 8) & 0xFF));
    }
    else if (type == Dos33Volume::kTypeApplesoft || type == Dos33Volume::kTypeInteger)
    {
        CBREx (fits, HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE));

        outStored.push_back ((Byte) (length & 0xFF));
        outStored.push_back ((Byte) ((length >> 8) & 0xFF));
    }

    outStored.insert (outStored.end(), payload.bytes.begin(), payload.bytes.end());

Error:
    return hr;
}





