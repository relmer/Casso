#include "Pch.h"

#include "Devices/Disk/Archives/NuLzw.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzw::ComputeCrc
//
////////////////////////////////////////////////////////////////////////////////

Word NuLzw::ComputeCrc (std::span<const Byte> data, Word seed)
{
    constexpr Word  kPolynomial = 0x1021;
    constexpr Word  kHighBit    = 0x8000;
    constexpr int   kByteBits   = 8;



    Word  crc = seed;



    for (Byte b : data)
    {
        crc = (Word) (crc ^ (b << kByteBits));

        for (int bit = 0; bit < kByteBits; bit++)
        {
            crc = (crc & kHighBit) ? (Word) ((crc << 1) ^ kPolynomial) : (Word) (crc << 1);
        }
    }

    return crc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzw::GetCodeWidth
//
//  The width of the next code, from the table's next free entry: 9 bits until
//  the entry before $200, then 10, 11 and 12, each a code earlier than the
//  classic scheme changes width.
//
////////////////////////////////////////////////////////////////////////////////

int NuLzw::GetCodeWidth (Word next)
{
    constexpr int  kWidth9  = 9;
    constexpr int  kWidth10 = 10;
    constexpr int  kWidth11 = 11;
    constexpr int  kWidth12 = 12;



    unsigned  following = (unsigned) next + 1;



    return (following < 0x200) ? kWidth9
         : (following < 0x400) ? kWidth10
         : (following < 0x800) ? kWidth11
                               : kWidth12;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzw::ResetState
//
////////////////////////////////////////////////////////////////////////////////

void NuLzw::ResetState (State & state, bool clearedInLine)
{
    state.next          = kFirstCode;
    state.prev          = 0;
    state.isFirst       = true;
    state.clearedInLine = clearedInLine;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzw::EndChunk
//
//  An LZW/2 chunk that ends with the table empty and one code read since the
//  stream began, or since a stored chunk, starts the next chunk afresh; after
//  a clear in the stream, the code read since carries over.
//
////////////////////////////////////////////////////////////////////////////////

void NuLzw::EndChunk (State & state)
{
    if (state.next == kFirstCode && !state.clearedInLine)
    {
        state.isFirst = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzw::AppendString
//
////////////////////////////////////////////////////////////////////////////////

void NuLzw::AppendString (const State & state, Word code, std::vector<Byte> & outBytes)
{
    size_t  start = outBytes.size();



    while (code >= kFirstCode)
    {
        outBytes.push_back (state.suffix[code]);
        code = state.prefix[code];
    }

    outBytes.push_back ((Byte) code);
    std::reverse (outBytes.begin() + (ptrdiff_t) start, outBytes.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzw::ExpandCodes
//
//  Codes from a byte-aligned position until `wanted` bytes are produced, then
//  the position after the last partial byte. A code past the table's next
//  entry is damage; a code equal to it is the entry being defined, the
//  previous string followed by its own first byte.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NuLzw::ExpandCodes (
    std::span<const Byte>   in,
    size_t                & ioPosition,
    size_t                  wanted,
    bool                    allowClear,
    State                 & state,
    std::vector<Byte>     & outBytes,
    std::string           & outError)
{
    HRESULT   hr       = S_OK;
    uint64_t  bitPos   = (uint64_t) ioPosition * 8;
    uint64_t  bitLimit = (uint64_t) in.size() * 8;
    size_t    start    = outBytes.size();
    bool      exact    = false;



    while (outBytes.size() - start < wanted)
    {
        int   width     = GetCodeWidth (state.next);
        Word  code      = 0;
        Byte  firstChar = 0;
        bool  inside    = bitPos + (uint64_t) width <= bitLimit;
        bool  defined   = false;
        bool  usable    = false;

        CBRFEx (inside, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The LZW data ends before its chunk is complete.");

        for (int bit = 0; bit < width; bit++, bitPos++)
        {
            code = (Word) (code | (((in[(size_t) (bitPos / 8)] >> (bitPos % 8)) & 1) << bit));
        }

        if (allowClear && code == kClearCode)
        {
            ResetState (state, true);
            continue;
        }

        if (state.isFirst)
        {
            CBRFEx (code < kClearCode, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "An LZW chunk starts with a code that isn't a byte.");

            outBytes.push_back ((Byte) code);
            state.prev    = code;
            state.isFirst = false;
            continue;
        }

        defined = code < state.next && code != kClearCode;
        usable  = defined || (code == state.next && state.next < kTableSize);
        CBRFEx (usable, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The LZW data has a code its table doesn't define.");

        firstChar = (code == state.next) ? (state.prev < kFirstCode ? (Byte) state.prev : state.first[state.prev])
                                         : (code < kFirstCode ? (Byte) code : state.first[code]);

        if (state.next < kTableSize)
        {
            state.prefix[state.next] = state.prev;
            state.suffix[state.next] = firstChar;
            state.first[state.next]  = state.prev < kFirstCode ? (Byte) state.prev : state.first[state.prev];
            state.next++;
        }

        AppendString (state, code, outBytes);
        state.prev = code;
    }

    exact = outBytes.size() - start == wanted;
    CBRFEx (exact, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "An LZW chunk expands past its length.");

    ioPosition = (size_t) ((bitPos + 7) / 8);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzw::ExpandRle
//
//  An escape byte, a value and a count make count + 1 copies of the value;
//  every other byte is itself. The chunk must come to exactly 4,096 bytes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NuLzw::ExpandRle (std::span<const Byte> in, Byte escape, std::vector<Byte> & outChunk, std::string & outError)
{
    HRESULT  hr     = S_OK;
    size_t   i      = 0;
    bool     hasRun = false;
    bool     whole  = false;



    outChunk.clear();

    for (i = 0; i < in.size(); i++)
    {
        if (in[i] != escape)
        {
            outChunk.push_back (in[i]);
            continue;
        }

        hasRun = i + 2 < in.size();
        CBRFEx (hasRun, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "A run ends partway through its escape.");

        outChunk.insert (outChunk.end(), (size_t) in[i + 2] + 1, in[i + 1]);
        i += 2;
    }

    whole = outChunk.size() == kChunkBytes;
    CBRFEx (whole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "A run-length chunk doesn't expand to 4,096 bytes.");

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzw::ExpandChunks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NuLzw::ExpandChunks (
    std::span<const Byte>   in,
    size_t                  position,
    size_t                  outLength,
    bool                    isLzw2,
    std::vector<Byte>     & outBytes,
    std::string           & outError)
{
    constexpr Word  kLzwFlag     = 0x8000;
    constexpr Word  kLengthMask  = 0x1FFF;



    HRESULT                 hr     = S_OK;
    size_t                  chunks = (outLength + kChunkBytes - 1) / kChunkBytes;
    Byte                    escape = in[position - 1];
    size_t                  chunk  = 0;
    std::unique_ptr<State>  state  = std::make_unique<State>();
    std::vector<Byte>       coded;
    std::vector<Byte>       expanded;



    outBytes.clear();

    for (chunk = 0; chunk < chunks; chunk++)
    {
        size_t  headerBytes = isLzw2 ? 2 : 3;
        Word    word        = 0;
        size_t  rleLength   = 0;
        bool    isLzw       = false;
        bool    hasHeader   = position + headerBytes <= in.size();

        CBRFEx (hasHeader, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The LZW data ends before its last chunk.");

        word      = (Word) (in[position] | (in[position + 1] << 8));
        rleLength = isLzw2 ? (size_t) (word & kLengthMask) : (size_t) word;
        isLzw     = isLzw2 ? (word & kLzwFlag) != 0 : in[position + 2] != 0;
        position += headerBytes;

        CBRFEx (rleLength <= kChunkBytes, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "An LZW chunk is longer than 4,096 bytes.");

        coded.clear();

        if (isLzw)
        {
            if (isLzw2)
            {
                //  The compressed length is skipped: one Macintosh program
                //  writes it in the other byte order.
                position += 2;
            }
            else
            {
                ResetState (*state, false);
            }

            hr = ExpandCodes (in, position, rleLength, isLzw2, *state, coded, outError);
            CHR (hr);

            if (isLzw2)
            {
                EndChunk (*state);
            }
        }
        else
        {
            bool  stored = position + rleLength <= in.size();

            CBRFEx (stored, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "A stored chunk runs past the end of the data.");

            coded.assign (in.begin() + (ptrdiff_t) position, in.begin() + (ptrdiff_t) (position + rleLength));
            position += rleLength;

            ResetState (*state, false);
        }

        if (rleLength == kChunkBytes)
        {
            outBytes.insert (outBytes.end(), coded.begin(), coded.end());
            continue;
        }

        hr = ExpandRle (coded, escape, expanded, outError);
        CHR (hr);

        outBytes.insert (outBytes.end(), expanded.begin(), expanded.end());
    }

    outBytes.resize (outLength);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzw::ExpandLzw1
//
//  The header's CRC is of every chunk padded to 4,096 bytes, from a seed of
//  zero, and is checked once the whole thread is expanded.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NuLzw::ExpandLzw1 (std::span<const Byte> in, size_t outLength, std::vector<Byte> & outBytes, std::string & outError)
{
    constexpr size_t  kHeaderBytes = 4;



    HRESULT            hr        = S_OK;
    bool               hasHeader = in.size() >= kHeaderBytes;
    bool               matches   = false;
    Word               expected  = 0;
    size_t             padded    = 0;
    std::vector<Byte>  whole;



    outBytes.clear();

    if (outLength == 0)
    {
        BAIL_OUT_IF (true, S_OK);
    }

    CBRFEx (hasHeader, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The LZW/1 data has no header.");

    expected = (Word) (in[0] | (in[1] << 8));
    padded   = ((outLength + kChunkBytes - 1) / kChunkBytes) * kChunkBytes;

    hr = ExpandChunks (in, kHeaderBytes, padded, false, whole, outError);
    CHR (hr);

    matches = ComputeCrc (whole, 0) == expected;
    CBRFEx (matches, HRESULT_FROM_WIN32 (ERROR_CRC), outError = "The LZW/1 data doesn't match its CRC.");

    whole.resize (outLength);
    outBytes = std::move (whole);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzw::ExpandLzw2
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NuLzw::ExpandLzw2 (std::span<const Byte> in, size_t outLength, std::vector<Byte> & outBytes, std::string & outError)
{
    constexpr size_t  kHeaderBytes = 2;



    HRESULT  hr        = S_OK;
    bool     hasHeader = in.size() >= kHeaderBytes;



    outBytes.clear();

    if (outLength == 0)
    {
        BAIL_OUT_IF (true, S_OK);
    }

    CBRFEx (hasHeader, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The LZW/2 data has no header.");

    hr = ExpandChunks (in, kHeaderBytes, outLength, true, outBytes, outError);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzw::CompressRle
//
//  A run of four or more of a byte, and any run of the escape byte, is the
//  escape, the byte and the count less one; anything else is itself.
//
////////////////////////////////////////////////////////////////////////////////

void NuLzw::CompressRle (std::span<const Byte> chunk, Byte escape, std::vector<Byte> & outBytes)
{
    size_t  i = 0;



    outBytes.clear();

    while (i < chunk.size())
    {
        Byte    value = chunk[i];
        size_t  run   = 1;

        while (i + run < chunk.size() && chunk[i + run] == value && run < kMaxRun)
        {
            run++;
        }

        if (run >= kMinRun || value == escape)
        {
            outBytes.push_back (escape);
            outBytes.push_back (value);
            outBytes.push_back ((Byte) (run - 1));
        }
        else
        {
            outBytes.insert (outBytes.end(), run, value);
        }

        i += run;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NuLzw::CompressLzw2
//
//  Each code is written at the width a decoder reads it at, and the table is
//  added to in the order a decoder adds to it: an entry for the previous
//  code and the first byte of each code after it. When the table reaches its
//  last entry, a clear code empties it.
//
////////////////////////////////////////////////////////////////////////////////

void NuLzw::CompressLzw2 (std::span<const Byte> in, std::vector<Byte> & outBytes)
{
    constexpr Word  kLzwFlag = 0x8000;



    std::unique_ptr<State>              state  = std::make_unique<State>();
    std::unordered_map<uint32_t, Word>  table;
    std::vector<Byte>                   chunk;
    std::vector<Byte>                   rle;
    std::vector<Byte>                   coded;
    size_t                              offset = 0;



    outBytes.clear();
    outBytes.push_back (0);
    outBytes.push_back (kRleEscape);

    for (offset = 0; offset < in.size(); offset += kChunkBytes)
    {
        size_t                   take    = std::min (kChunkBytes, in.size() - offset);
        uint32_t                 bits    = 0;
        int                      count   = 0;
        std::span<const Byte>    data;
        size_t                   i       = 0;
        bool                     shrinks = false;

        auto  write = [&] (Word code, int width)
        {
            bits  |= (uint32_t) code << count;
            count += width;

            while (count >= 8)
            {
                coded.push_back ((Byte) (bits & 0xFF));
                bits  >>= 8;
                count  -= 8;
            }
        };

        auto  firstOf = [&] (Word code) -> Byte { return code < kFirstCode ? (Byte) code : state->first[code]; };

        auto  emit = [&] (Word code)
        {
            write (code, GetCodeWidth (state->next));

            if (!state->isFirst && state->next < kTableSize)
            {
                table[((uint32_t) state->prev << 8) | firstOf (code)] = state->next;
                state->first[state->next] = firstOf (state->prev);
                state->next++;
            }

            state->isFirst = false;
            state->prev    = code;

            if (state->next > kLastEntry)
            {
                write (kClearCode, GetCodeWidth (state->next));
                table.clear();
                ResetState (*state, true);
            }
        };

        chunk.assign (in.begin() + (ptrdiff_t) offset, in.begin() + (ptrdiff_t) (offset + take));
        chunk.resize (kChunkBytes, 0);

        CompressRle (chunk, kRleEscape, rle);

        data = (rle.size() < kChunkBytes) ? std::span<const Byte> (rle) : std::span<const Byte> (chunk);

        coded.clear();

        while (i < data.size())
        {
            Word    code = data[i];
            size_t  j    = i + 1;

            while (j < data.size())
            {
                auto  found = table.find (((uint32_t) code << 8) | data[j]);

                if (found == table.end())
                {
                    break;
                }

                code = found->second;
                j++;
            }

            emit (code);
            i = j;
        }

        if (count > 0)
        {
            coded.push_back ((Byte) (bits & 0xFF));
        }

        shrinks = coded.size() + 2 < data.size();

        outBytes.push_back ((Byte) ((data.size() | (shrinks ? kLzwFlag : 0)) & 0xFF));
        outBytes.push_back ((Byte) ((data.size() | (shrinks ? kLzwFlag : 0)) >> 8));

        if (shrinks)
        {
            size_t  length = coded.size() + 4;

            outBytes.push_back ((Byte) (length & 0xFF));
            outBytes.push_back ((Byte) (length >> 8));
            outBytes.insert (outBytes.end(), coded.begin(), coded.end());

            EndChunk (*state);
        }
        else
        {
            outBytes.insert (outBytes.end(), data.begin(), data.end());

            table.clear();
            ResetState (*state, false);
        }
    }
}
