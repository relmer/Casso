#include "Pch.h"

#include "CassoLink/Session/RawWriteStream.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::FromBits
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteStream::FromBits (std::span<const uint8_t> bits, RawWriteStream & outStream)
{
    std::vector<size_t>   starts;
    std::vector<uint8_t>  framed;
    std::vector<uint8_t>  rotated;
    size_t                first     = FindByteStart (bits);
    size_t                runAt     = 0;
    size_t                runLength = 0;
    bool                  sync      = false;



    framed.assign (bits.begin() + (ptrdiff_t) first, bits.end());
    framed.insert (framed.end(), bits.begin(), bits.begin() + (ptrdiff_t) first);

    Split (framed, outStream, starts);

    //  The write starts at the seam, so the end of the write overlaps sync
    //  bytes or junk rather than a sector.
    if (!outStream.FindSeam (runAt, runLength, sync))
    {
        return;
    }

    rotated.assign (framed.begin() + (ptrdiff_t) starts[runAt], framed.end());
    rotated.insert (rotated.end(), framed.begin(), framed.begin() + (ptrdiff_t) starts[runAt]);

    Split (rotated, outStream, starts);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::FindByteStart
//
////////////////////////////////////////////////////////////////////////////////

size_t RawWriteStream::FindByteStart (std::span<const uint8_t> bits)
{
    size_t  count = bits.size();
    size_t  shift = 0;



    //  A byte starts at a one bit when none is being shifted in, and ends
    //  with its eighth bit; the first turn settles the framing.
    for (size_t at = 0; at < 2 * count; at++)
    {
        uint8_t  bit = bits[at % count];

        if (shift == 0 && bit == 0)
        {
            continue;
        }

        if (shift == 0 && at >= count)
        {
            return at - count;
        }

        shift = (shift + 1) % kBitsPerByte;
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::Split
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteStream::Split (std::span<const uint8_t> bits, RawWriteStream & outStream, std::vector<size_t> & outStarts)
{
    size_t  at = 0;



    outStream.values.clear();
    outStream.zeros.clear();
    outStarts.clear();

    while (at < bits.size())
    {
        Byte    value = 0;
        size_t  zeros = 0;

        outStarts.push_back (at);

        for (size_t i = 0; i < kBitsPerByte; i++)
        {
            value = (Byte) ((value << 1) | ((at < bits.size() && bits[at] != 0) ? 1 : 0));
            at++;
        }

        while (at < bits.size() && bits[at] == 0 && zeros < kMaxZeros)
        {
            zeros++;
            at++;
        }

        outStream.values.push_back (value);
        outStream.zeros.push_back ((Byte) zeros);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::FindSyncRun
//
////////////////////////////////////////////////////////////////////////////////

bool RawWriteStream::FindSyncRun (size_t & outAt, size_t & outLength) const
{
    size_t  count = values.size();



    outAt     = 0;
    outLength = 0;

    for (size_t at = 0; at < count; at++)
    {
        size_t  run = 0;

        if (!IsSyncByte (at) || IsSyncByte ((at + count - 1) % count))
        {
            continue;
        }

        while (run < count && IsSyncByte ((at + run) % count))
        {
            run++;
        }

        if (run > outLength)
        {
            outAt     = at;
            outLength = run;
        }
    }

    return outLength > 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::FindSeam
//
////////////////////////////////////////////////////////////////////////////////

bool RawWriteStream::FindSeam (size_t & outAt, size_t & outLength, bool & outSync) const
{
    size_t               count  = values.size();
    size_t               most   = 0;
    size_t               group  = 0;
    std::vector<size_t>  prologs;



    outSync = FindSyncRun (outAt, outLength) && outLength >= kSyncRun;

    if (outSync || count == 0)
    {
        return count != 0;
    }

    for (size_t at = 0; at < count; at++)
    {
        if (values[at] == s_kPrologFirst && values[(at + 1) % count] == s_kPrologSecond && values[(at + 2) % count] == s_kPrologSecond)
        {
            prologs.push_back (at);
        }
    }

    if (prologs.empty())
    {
        return outLength > 0;
    }

    //  The prolog with the most prologs in the s_kGroupBytes from it.
    for (size_t i = 0; i < prologs.size(); i++)
    {
        size_t  within = 0;

        for (size_t other : prologs)
        {
            within += (other + count - prologs[i]) % count < s_kGroupBytes ? 1 : 0;
        }

        if (within > most)
        {
            most  = within;
            group = prologs[i];
        }
    }

    outAt     = group;
    outLength = 0;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::FromNibbles
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteStream::FromNibbles (std::span<const Byte> nibbles, RawWriteStream & outStream)
{
    size_t  length = FindRevolution (nibbles);
    size_t  at     = 0;



    outStream.values.assign (nibbles.begin(), nibbles.begin() + length);
    outStream.zeros.assign  (length, 0);

    while (at < length)
    {
        size_t  run = 0;

        while (at + run < length && nibbles[at + run] == s_kSyncValue)
        {
            run++;
        }

        if (run >= kSyncRun)
        {
            std::fill_n (outStream.zeros.begin() + at, run, (Byte) kSyncZeros);
        }

        at += std::max<size_t> (run, 1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::FindRevolution
//
////////////////////////////////////////////////////////////////////////////////

size_t RawWriteStream::FindRevolution (std::span<const Byte> nibbles)
{
    for (size_t length = s_kMinRevolution; length + s_kMatchBytes <= nibbles.size(); length++)
    {
        if (std::equal (nibbles.begin(), nibbles.begin() + s_kMatchBytes, nibbles.begin() + length))
        {
            return length;
        }
    }

    return nibbles.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::GetCellCount
//
////////////////////////////////////////////////////////////////////////////////

size_t RawWriteStream::GetCellCount() const
{
    size_t  cells = values.size() * kBitsPerByte;



    for (Byte count : zeros)
    {
        cells += count;
    }

    return cells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::FitToTurn
//
////////////////////////////////////////////////////////////////////////////////

bool RawWriteStream::FitToTurn (size_t cells)
{
    constexpr size_t  kSyncCells = kBitsPerByte + kSyncZeros;



    size_t  count     = values.size();
    size_t  bestAt    = 0;
    size_t  bestRun   = 0;
    size_t  middle    = 0;
    size_t  headSyncs = 0;
    size_t  tailSyncs = 0;
    size_t  headCells = 0;
    size_t  total     = 0;
    size_t  low       = cells + s_kOverlapCells - s_kLeadInCells - s_kLeadOutCells;
    bool    sync      = false;



    //  A track shorter than the turn is lengthened at the seam, and only a
    //  sync run is trimmed.
    if (!FindSeam (bestAt, bestRun, sync))
    {
        return false;
    }

    middle = (bestAt + bestRun / 2) % count;

    std::rotate (values.begin(), values.begin() + (ptrdiff_t) middle, values.end());
    std::rotate (zeros.begin(),  zeros.begin()  + (ptrdiff_t) middle, zeros.end());

    headSyncs = sync ? bestRun - bestRun / 2 : 0;
    tailSyncs = sync ? bestRun / 2 : 0;
    total     = GetCellCount();

    for (size_t i = 0; i < headSyncs; i++)
    {
        headCells += kBitsPerByte + zeros[i];
    }

    //  The overlap must fall on sync bytes rather than on the first byte after
    //  them. With no sync run the seam is at a prolog, and a short run has too
    //  few sync bytes ahead of the seam, so sync bytes are added.
    while (headSyncs < s_kSeamSyncs || total < low)
    {
        values.insert (values.begin(), s_kSyncValue);
        zeros.insert  (zeros.begin(),  (Byte) kSyncZeros);
        total     += kSyncCells;
        headCells += kSyncCells;
        headSyncs++;
    }

    //  The end of the write overlaps the stream by the cells past the turn,
    //  short of the last s_kResyncSyncs sync bytes ahead of the seam.
    while (total > GetLongestFit (cells, headCells) && tailSyncs > s_kTailSyncs)
    {
        total -= kBitsPerByte + zeros.back();

        values.pop_back();
        zeros.pop_back();
        tailSyncs--;
    }

    //  A weak-bit stretch is a long run of zero bits, which reads as noise at
    //  any length, so the longest runs give up bits next.
    while (total > GetLongestFit (cells, headCells))
    {
        auto    longest = std::max_element (zeros.begin(), zeros.end());
        size_t  spare   = (longest != zeros.end() && *longest > s_kWeakZeros) ? *longest - s_kWeakZeros : 0;
        size_t  cut     = std::min (spare, total - GetLongestFit (cells, headCells));

        if (cut == 0)
        {
            break;
        }

        *longest = (Byte) (*longest - cut);
        total   -= cut;
    }

    //  Then the longest runs of sync bytes between sectors, one sync byte at
    //  a time, to no fewer than s_kGapSyncs.
    while (total > GetLongestFit (cells, headCells))
    {
        bool  dropped = DropGapSync (headSyncs, values.size() - tailSyncs, total);

        if (!dropped)
        {
            break;
        }
    }

    return total <= GetLongestFit (cells, headCells);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::DropGapSync
//
////////////////////////////////////////////////////////////////////////////////

bool RawWriteStream::DropGapSync (size_t first, size_t end, size_t & ioTotal)
{
    size_t  bestAt  = 0;
    size_t  bestRun = 0;
    size_t  at      = first;



    while (at < end)
    {
        size_t  run = 0;

        while (at + run < end && IsSyncByte (at + run))
        {
            run++;
        }

        if (run > bestRun)
        {
            bestAt  = at;
            bestRun = run;
        }

        at += std::max<size_t> (run, 1);
    }

    if (bestRun <= s_kGapSyncs)
    {
        return false;
    }

    ioTotal -= kBitsPerByte + zeros[bestAt];

    values.erase (values.begin() + (ptrdiff_t) bestAt);
    zeros.erase  (zeros.begin()  + (ptrdiff_t) bestAt);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::GetLongestFit
//
////////////////////////////////////////////////////////////////////////////////

size_t RawWriteStream::GetLongestFit (size_t cells, size_t headCells)
{
    constexpr size_t  kResyncCells = s_kResyncSyncs * (kBitsPerByte + kSyncZeros);



    size_t  overlap = (headCells > kResyncCells) ? headCells - kResyncCells : 0;



    return cells + overlap - s_kLeadOutCells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::ToBits
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteStream::ToBits (std::vector<uint8_t> & outBits) const
{
    outBits.clear();

    for (size_t i = 0; i < values.size(); i++)
    {
        for (size_t bit = 0; bit < kBitsPerByte; bit++)
        {
            outBits.push_back ((uint8_t) ((values[i] >> (kBitsPerByte - 1 - bit)) & 1));
        }

        outBits.insert (outBits.end(), zeros[i], 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::ToWrittenBits
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteStream::ToWrittenBits (std::vector<uint8_t> & outBits) const
{
    std::vector<Byte>  stream;
    size_t             count = 0;



    outBits.clear();

    if (!BuildModuleStream (stream, count))
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        for (size_t bit = 0; bit < kBitsPerByte; bit++)
        {
            outBits.push_back ((uint8_t) ((stream[i] >> (kBitsPerByte - 1 - bit)) & 1));
        }

        outBits.insert (outBits.end(), stream[kMaxBytes + i], 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::BuildModuleStream
//
////////////////////////////////////////////////////////////////////////////////

bool RawWriteStream::BuildModuleStream (std::vector<Byte> & outStream, size_t & outCount) const
{
    std::vector<Byte>  moduleValues;
    std::vector<Byte>  moduleZeros;



    outStream.clear();
    outCount = 0;

    for (size_t i = 0; i < values.size(); i++)
    {
        size_t  count  = zeros[i];
        Byte    filler = count >= s_kWeakZeros ? s_kWeakFlux : (Byte) 0;

        moduleValues.push_back (values[i]);
        moduleZeros.push_back  ((Byte) (count % kBitsPerByte));

        //  A weak-bit stretch keeps one flux change each eight cells: a real
        //  drive's read amplifier, given none for that long, takes time to
        //  settle again and loses the data between weak stretches. Seven
        //  cells with no change still read as noise.
        for (size_t eight = 0; eight < count / kBitsPerByte; eight++)
        {
            moduleValues.push_back (filler);
            moduleZeros.push_back  (0);
        }
    }

    if (moduleValues.empty() || moduleValues.size() > kMaxBytes)
    {
        return false;
    }

    outCount = moduleValues.size();

    moduleValues.resize (kMaxBytes, 0);
    moduleZeros.resize  (kMaxBytes, 0);

    outStream = std::move (moduleValues);
    outStream.insert (outStream.end(), moduleZeros.begin(), moduleZeros.end());

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::IsRawImage
//
////////////////////////////////////////////////////////////////////////////////

bool RawWriteStream::IsRawImage (std::span<const Byte> fileBytes)
{
    static constexpr Byte  kWoz1[] = { 'W', 'O', 'Z', '1' };
    static constexpr Byte  kWoz2[] = { 'W', 'O', 'Z', '2' };



    bool  woz = fileBytes.size() >= std::size (kWoz1) && (std::equal (std::begin (kWoz1), std::end (kWoz1), fileBytes.begin()) || std::equal (std::begin (kWoz2), std::end (kWoz2), fileBytes.begin()));



    return woz || fileBytes.size() == NibbleImageCodec::kNibImageSize || fileBytes.size() == NibbleImageCodec::kNb2ImageSize;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::GetImageTracks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawWriteStream::GetImageTracks (std::span<const Byte> fileBytes, std::vector<RawWriteTrack> & outTracks, int & outTooLong)
{
    HRESULT               hr        = S_OK;
    DiskImage             image;
    std::vector<Byte>     raw (fileBytes.begin(), fileBytes.end());
    std::vector<Byte>     moduleStream;
    std::vector<uint8_t>  bits;
    size_t                trackSize = fileBytes.size() / NibbleImageCodec::kTrackCount;
    size_t                count     = 0;
    bool                  nib       = fileBytes.size() == NibbleImageCodec::kNibImageSize || fileBytes.size() == NibbleImageCodec::kNb2ImageSize;
    bool                  known     = IsRawImage (fileBytes);



    outTracks.clear();
    outTooLong = -1;
    CBREx (known, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (nib)
    {
        for (int track = 0; track < NibbleImageCodec::kTrackCount; track++)
        {
            RawWriteTrack  entry;

            entry.position = track * DiskImage::kQuarterTracksPerWholeTrack;
            FromNibbles (fileBytes.subspan ((size_t) track * trackSize, trackSize), entry.stream);
            outTracks.push_back (std::move (entry));
        }
    }
    else
    {
        hr = WozLoader::Load (raw, image);
        CHR (hr);

        for (int position = 0; position < kWritablePositions; position++)
        {
            int            slot     = image.ResolveQuarterTrack (position);
            int            whole    = position - position % DiskImage::kQuarterTracksPerWholeTrack;
            int            next     = whole + DiskImage::kQuarterTracksPerWholeTrack;
            bool           distinct = position == whole || (slot != image.ResolveQuarterTrack (whole) && (next >= DiskImage::kQuarterTrackCount || slot != image.ResolveQuarterTrack (next)));
            RawWriteTrack  entry;

            if (slot < 0 || !distinct || image.GetTrackBitCount (slot) == 0)
            {
                continue;
            }

            bits.clear();

            for (size_t at = 0; at < image.GetTrackBitCount (slot); at++)
            {
                bits.push_back (image.ReadBit (slot, at));
            }

            entry.position = position;
            FromBits (bits, entry.stream);
            outTracks.push_back (std::move (entry));
        }

        DropOffCenterReads (outTracks);
    }

    for (const RawWriteTrack & track : outTracks)
    {
        if (!track.stream.BuildModuleStream (moduleStream, count))
        {
            outTooLong = track.position;
            CBREx (false, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::DropOffCenterReads
//
//  A quarter position that is only the whole tracks beside it read off
//  center isn't written, since its write would overwrite them. A whole track
//  beside a half track that is written, with no sector of its own and mostly
//  that half track's edge, as Karateka's track 3 beside 2.5, is the blank
//  between them on the disk that was read: it is erased with sync bytes
//  before the half track is written, rather than written after it over the
//  half track's edge. Each decision is made before the list changes.
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteStream::DropOffCenterReads (std::vector<RawWriteTrack> & ioTracks)
{
    std::set<int>                          dropped;
    std::map<int, std::vector<int>>        erasesBefore;
    std::vector<RawWriteTrack>             kept;
    int                                    perTrack = DiskImage::kQuarterTracksPerWholeTrack;



    auto  find = [&ioTracks, &dropped] (int position) -> const RawWriteStream *
    {
        for (const RawWriteTrack & track : ioTracks)
        {
            if (track.position == position && !dropped.contains (position))
            {
                return &track.stream;
            }
        }

        return nullptr;
    };

    for (const RawWriteTrack & track : ioTracks)
    {
        int  whole = track.position - track.position % perTrack;

        if (track.position != whole && IsOffCenterRead (track.stream, find (whole), find (whole + perTrack)))
        {
            dropped.insert (track.position);
        }
    }

    for (const RawWriteTrack & track : ioTracks)
    {
        const RawWriteStream  * below = find (track.position - perTrack / 2);
        const RawWriteStream  * above = find (track.position + perTrack / 2);
        int                     first = below != nullptr ? track.position - perTrack / 2 : track.position + perTrack / 2;

        if (track.position % perTrack != 0 || (below == nullptr && above == nullptr))
        {
            continue;
        }

        if (IsOffCenterRead (track.stream, below, above) && GetSharedPart (track.stream, below, above) >= s_kBleedShare)
        {
            dropped.insert (track.position);
            erasesBefore[first].push_back (track.position);
        }
    }

    for (const RawWriteTrack & track : ioTracks)
    {
        for (int erased : erasesBefore[track.position])
        {
            RawWriteTrack  erase;

            erase.position = erased;

            //  Sync bytes in runs as an ordinary track's gaps are: thousands of
            //  $FF in a row arrived damaged on every try over a real //c's
            //  serial port at 115,200 baud. It is a little shorter than the
            //  track, so FitToTurn lengthens it at its seam to the turn.
            for (const RawWriteTrack & source : ioTracks)
            {
                size_t  count = source.position == erased ? source.stream.GetCellCount() * s_kErasePercent / 100 / (kBitsPerByte + kSyncZeros) : 0;

                for (size_t i = 0; i < count; i++)
                {
                    bool  end = i % s_kEraseRun == s_kEraseRun - 1 || i == count - 1;

                    erase.stream.values.push_back (end ? s_kEraseEnd : s_kSyncValue);
                    erase.stream.zeros.push_back  (end ? (Byte) 0 : (Byte) kSyncZeros);
                }
            }

            kept.push_back (std::move (erase));
        }

        if (!dropped.contains (track.position))
        {
            kept.push_back (track);
        }
    }

    ioTracks = std::move (kept);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::GetSharedPart
//
//  The part of a stream's s_kChunkBytes-byte chunks found in either neighbor,
//  each neighbor looked at around twice so a chunk that wraps is found.
//
////////////////////////////////////////////////////////////////////////////////

double RawWriteStream::GetSharedPart (const RawWriteStream & stream, const RawWriteStream * below, const RawWriteStream * above)
{
    std::vector<Byte>  turns[2];
    size_t             chunks = 0;
    size_t             found  = 0;



    for (size_t i = 0; i < 2; i++)
    {
        const RawWriteStream  * side = i == 0 ? below : above;

        if (side != nullptr)
        {
            turns[i] = side->values;
            turns[i].insert (turns[i].end(), side->values.begin(), side->values.end());
        }
    }

    for (size_t at = 0; at + s_kChunkBytes <= stream.values.size(); at += s_kChunkBytes)
    {
        auto  first = stream.values.begin() + (ptrdiff_t) at;
        auto  last  = first + (ptrdiff_t) s_kChunkBytes;
        bool  hit   = false;

        for (const std::vector<Byte> & turn : turns)
        {
            hit = hit || std::search (turn.begin(), turn.end(), first, last) != turn.end();
        }

        chunks++;
        found += hit ? 1 : 0;
    }

    return chunks == 0 ? 0.0 : (double) found / (double) chunks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteStream::IsOffCenterRead
//
//  Sectors, not all of the stream, are compared: a slip in framing after a
//  weak-bit stretch changes the bytes for a while, but a sector starts at its
//  prolog. Each stream is looked at around twice, so a sector that wraps past
//  its end is found too.
//
////////////////////////////////////////////////////////////////////////////////

bool RawWriteStream::IsOffCenterRead (const RawWriteStream & quarter, const RawWriteStream * below, const RawWriteStream * above)
{
    std::vector<Byte>  turns[2];
    std::vector<Byte>  own   = quarter.values;
    size_t             count = quarter.values.size();



    for (size_t i = 0; i < 2; i++)
    {
        const RawWriteStream  * side = i == 0 ? below : above;

        if (side != nullptr)
        {
            turns[i] = side->values;
            turns[i].insert (turns[i].end(), side->values.begin(), side->values.end());
        }
    }

    own.insert (own.end(), quarter.values.begin(), quarter.values.end());

    for (size_t at = 0; at < count && count >= s_kSectorBytes; at++)
    {
        auto  first = own.begin() + (ptrdiff_t) at;
        auto  last  = first + (ptrdiff_t) s_kSectorBytes;
        bool  found = false;

        if (own[at] != s_kPrologFirst || own[at + 1] != s_kPrologSecond)
        {
            continue;
        }

        for (const std::vector<Byte> & turn : turns)
        {
            found = found || std::search (turn.begin(), turn.end(), first, last) != turn.end();
        }

        if (!found)
        {
            return false;
        }
    }

    return true;
}
