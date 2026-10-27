#include "Pch.h"

#include "Shell/MachineHost.h"

#include "Core/StateHash.h"
#include "Core/StateWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost::ComputeStateFingerprint
//
//  Every part saves into one stream, which is then walked section by
//  section at its top level: each section's tag and version go into the
//  checksum, after the machine section's tag and version and the part
//  count. The
//  sections' contents, which change as the machine runs, do not.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineHost::ComputeStateFingerprint (uint64_t & outFingerprint)
{
    constexpr size_t           kTagBytes     = sizeof (uint32_t) + sizeof (uint16_t);
    constexpr size_t           kHeaderBytes  = kTagBytes + sizeof (uint32_t);
    HRESULT                    hr            = S_OK;
    size_t                     offset        = 0;
    size_t                     payload       = 0;
    bool                       isWhole       = false;
    uint64_t                   partCount     = 0;
    const Byte               * data          = nullptr;
    size_t                     size          = 0;
    StateWriter                writer;



    GetStateParts (m_stateParts);

    partCount      = m_stateParts.size();
    outFingerprint = StateHash::Hash (reinterpret_cast<const Byte *> (&kStateTag),     sizeof (kStateTag));
    outFingerprint = StateHash::Hash (reinterpret_cast<const Byte *> (&kStateVersion), sizeof (kStateVersion), outFingerprint);
    outFingerprint = StateHash::Hash (reinterpret_cast<const Byte *> (&partCount),     sizeof (partCount),     outFingerprint);

    for (const IMachineState * part : m_stateParts)
    {
        hr = part->SaveState (writer);
        CHR (hr);
    }

    data = writer.GetBytes().data();
    size = writer.GetBytes().size();

    while (offset + kHeaderBytes <= size)
    {
        outFingerprint = StateHash::Hash (data + offset, kTagBytes, outFingerprint);

        payload = static_cast<size_t> (data[offset + kTagBytes])
                | static_cast<size_t> (data[offset + kTagBytes + 1]) << 8
                | static_cast<size_t> (data[offset + kTagBytes + 2]) << 16
                | static_cast<size_t> (data[offset + kTagBytes + 3]) << 24;

        offset += kHeaderBytes + payload;
    }

    isWhole = offset == size;
    CBRA (isWhole);

Error:
    return hr;
}
