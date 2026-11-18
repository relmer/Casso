#include "Pch.h"

#include "Core/StateHash.h"
#include "Debugger/Reverse/KeyframeCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCodec::Reserve
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeCodec::Reserve (size_t stateRoom)
{
    m_latestWhole.reserve (stateRoom);
    m_scratch.reserve     (stateRoom);
    m_olderWhole.reserve  (stateRoom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCodec::Release
//
//  Frees every buffer.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeCodec::Release()
{
    m_latestWhole = std::vector<Byte>();
    m_scratch     = std::vector<Byte>();
    m_olderWhole  = std::vector<Byte>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCodec::GetBufferBytes
//
////////////////////////////////////////////////////////////////////////////////

size_t KeyframeCodec::GetBufferBytes() const
{
    return m_latestWhole.capacity() + m_scratch.capacity() + m_olderWhole.capacity();
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCodec::Pack
//
//  The checksum of the state, then the state packed whole, which becomes
//  the newest whole snapshot, or its XOR against that snapshot packed. A
//  whole state's buffer becomes the newest whole snapshot, and the old
//  one's buffer goes back to the caller in `state`, so neither is copied.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeCodec::Pack (std::vector<Byte> & state, bool isWhole, std::vector<Byte> & outPacked, uint64_t & outChecksum)
{
    HRESULT  hr         = S_OK;
    bool     isSameSize = false;



    outChecksum = ComputeChecksum (state.data(), state.size());

    if (isWhole)
    {
        hr = m_compressor.Compress (state.data(), state.size(), outPacked);
        CHR (hr);

        m_latestWhole.swap (state);
    }
    else
    {
        isSameSize = state.size() == m_latestWhole.size();
        CBRA (isSameSize);

        m_scratch.resize (state.size());

        XorBytes (state.data(), m_latestWhole.data(), m_scratch.data(), state.size());

        hr = m_compressor.Compress (m_scratch.data(), m_scratch.size(), outPacked);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCodec::Unpack
//
//  The whole snapshot of the keyframe's group, unpacked again unless it is
//  the newest group's, and for a difference, the difference XORed onto it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeCodec::Unpack (
    const Packed         & whole,
    const Packed         & keyframe,
    bool                   isNewestGroup,
    std::vector<Byte>    & outState)
{
    HRESULT        hr   = S_OK;
    const Byte   * base = nullptr;



    if (!isNewestGroup)
    {
        hr = m_compressor.Decompress (whole.data, whole.packedBytes, whole.stateBytes, m_olderWhole);
        CHR (hr);
    }

    base = isNewestGroup ? m_latestWhole.data() : m_olderWhole.data();

    if (keyframe.isWhole)
    {
        outState.assign (base, base + keyframe.stateBytes);
    }
    else
    {
        hr = m_compressor.Decompress (keyframe.data, keyframe.packedBytes, keyframe.stateBytes, outState);
        CHR (hr);

        XorBytes (outState.data(), base, outState.data(), outState.size());
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCodec::ReloadLatest
//
//  The newest group's whole snapshot, unpacked after a truncation.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeCodec::ReloadLatest (const Packed & whole)
{
    return m_compressor.Decompress (whole.data, whole.packedBytes, whole.stateBytes, m_latestWhole);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCodec::ComputeChecksum
//
//  StateHash over the whole state. It detects a replay that diverged, not
//  tampering.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t KeyframeCodec::ComputeChecksum (
    const Byte  * data,
    size_t        size)
{
    return StateHash::Hash (data, size);
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCodec::XorBytes
//
//  out = a XOR b over count bytes, eight at a time; out may be a. Through
//  raw pointers and words, so the loop does not reload a vector's pointers
//  after every byte it stores.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeCodec::XorBytes (
    const Byte  * a,
    const Byte  * b,
    Byte        * out,
    size_t        count)
{
    uint64_t  wordA = 0;
    uint64_t  wordB = 0;
    size_t    i     = 0;



    for (i = 0; i + sizeof (wordA) <= count; i += sizeof (wordA))
    {
        memcpy (&wordA, a + i, sizeof (wordA));
        memcpy (&wordB, b + i, sizeof (wordB));

        wordA ^= wordB;

        memcpy (out + i, &wordA, sizeof (wordA));
    }

    for (; i < count; i++)
    {
        out[i] = static_cast<Byte> (a[i] ^ b[i]);
    }
}
