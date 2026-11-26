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
    std::vector<uint8_t>  rotated;
    size_t                runAt     = 0;
    size_t                runLength = 0;



    Split (bits, outStream, starts);

    if (!outStream.FindSyncRun (runAt, runLength) || runLength < s_kMinSyncRun)
    {
        return;
    }

    rotated.assign (bits.begin() + (ptrdiff_t) starts[runAt], bits.end());
    rotated.insert (rotated.end(), bits.begin(), bits.begin() + (ptrdiff_t) starts[runAt]);

    Split (rotated, outStream, starts);
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
    size_t  count     = values.size();
    size_t  bestAt    = 0;
    size_t  bestRun   = 0;
    size_t  middle    = 0;
    size_t  headSyncs = 0;
    size_t  tailSyncs = 0;
    size_t  total     = 0;
    size_t  low       = cells + s_kOverlapCells;
    size_t  high      = cells + 2 * s_kOverlapCells;



    FindSyncRun (bestAt, bestRun);

    if (bestRun < s_kMinSyncRun)
    {
        return false;
    }

    middle = (bestAt + bestRun / 2) % count;

    std::rotate (values.begin(), values.begin() + (ptrdiff_t) middle, values.end());
    std::rotate (zeros.begin(),  zeros.begin()  + (ptrdiff_t) middle, zeros.end());

    headSyncs = bestRun - bestRun / 2;
    tailSyncs = bestRun / 2;
    total     = GetCellCount();

    while (total > high && headSyncs > s_kMinSyncRun / 2)
    {
        total -= kBitsPerByte + zeros.front();

        values.erase (values.begin());
        zeros.erase  (zeros.begin());
        headSyncs--;
    }

    while (total > high && tailSyncs > s_kMinSyncRun / 2)
    {
        total -= kBitsPerByte + zeros.back();

        values.pop_back();
        zeros.pop_back();
        tailSyncs--;
    }

    while (total < low)
    {
        values.insert (values.begin(), s_kSyncValue);
        zeros.insert  (zeros.begin(),  (Byte) kSyncZeros);
        total += kBitsPerByte + kSyncZeros;
    }

    return total <= high;
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
        size_t  count = zeros[i];

        moduleValues.push_back (values[i]);
        moduleZeros.push_back  ((Byte) (count % kBitsPerByte));

        for (size_t eight = 0; eight < count / kBitsPerByte; eight++)
        {
            moduleValues.push_back (0);
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
