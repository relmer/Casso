#include "Pch.h"

#include "Debugger/Reverse/KeyframeUnpacker.h"
#include "Debugger/Reverse/KeyframeStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeUnpacker::HoldsWhole
//
////////////////////////////////////////////////////////////////////////////////

bool KeyframeUnpacker::HoldsWhole (
    uint64_t  position,
    uint64_t  checksum) const
{
    return m_hasWhole && m_wholePosition == position && m_wholeChecksum == checksum;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeUnpacker::Unpack
//
//  The group's whole snapshot is unpacked when it came with the copy and
//  kept, else the one held is used; a difference is then unpacked and XORed
//  onto it. A copy that left the whole snapshot out for one this unpacker
//  does not hold is a caller bug.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeUnpacker::Unpack (
    const PackedKeyframe  & packed,
    std::vector<Byte>     & outState)
{
    HRESULT  hr         = S_OK;
    bool     hasWhole   = !packed.whole.empty();
    bool     isHeld     = false;
    bool     isSameSize = false;



    if (hasWhole)
    {
        m_hasWhole = false;

        hr = m_compressor.Decompress (packed.whole.data(), packed.whole.size(), packed.stateBytes, m_whole);
        CHR (hr);

        m_wholePosition = packed.wholePosition;
        m_wholeChecksum = packed.wholeChecksum;
        m_hasWhole      = true;
    }

    isHeld = HoldsWhole (packed.wholePosition, packed.wholeChecksum);
    CBRA (isHeld);

    isSameSize = m_whole.size() == packed.stateBytes;
    CBRAEx (isSameSize, E_INVALIDARG);

    if (packed.isWhole)
    {
        outState.assign (m_whole.begin(), m_whole.end());
        BAIL_OUT_IF (true, S_OK);
    }

    hr = m_compressor.Decompress (packed.difference.data(), packed.difference.size(), packed.stateBytes, outState);
    CHR (hr);

    KeyframeStore::XorBytes (outState.data(), m_whole.data(), outState.data(), outState.size());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeUnpacker::Forget
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeUnpacker::Forget()
{
    m_hasWhole = false;
}
