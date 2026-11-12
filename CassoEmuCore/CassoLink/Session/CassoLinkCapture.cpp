#include "Pch.h"

#include "CassoLink/Session/CassoLinkCapture.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/Session/CaptureStream.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::Capture
//
//  The whole tracks first, so each other position has its neighbor's
//  stream to compare its first read with.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCapture::Capture (CassoLinkHost & host, Byte drive, std::span<const Byte> module, const std::string & imageDate,
                                   const ProgressFn & progress, Result & outResult, int lastTrack)
{
    HRESULT                hr        = S_OK;
    std::vector<Position>  positions;
    int                    last      = std::clamp (lastTrack, 0, kTracks - 1) * 4;
    int                    done      = 0;



    outResult = Result();

    for (int position = 0; position <= last; position += 4)
    {
        Position  read;

        hr = ReadPosition (host, drive, module, position, kWholeReads, read);
        CHR (hr);

        hr = ReadMore (host, drive, module, read);
        CHR (hr);

        positions.push_back (std::move (read));

        if (progress)
        {
            progress (++done, last + 1);
        }
    }

    for (int position = 1; position <= last; position++)
    {
        Position  read;
        Stream    stream;
        Stream    below;
        Stream    above;
        bool      unstable = false;
        int       lower    = position / 4 * 4;
        int       upper    = std::min (lower + 4, last);

        if (position % 4 == 0)
        {
            continue;
        }

        hr = ReadPosition (host, drive, module, position, 1, read);
        CHR (hr);

        MakeStream (positions[(size_t) lower / 4], below, unstable);
        MakeStream (positions[(size_t) upper / 4], above, unstable);

        //  A position with a separate stream is read again.
        if (MakeStream (read, stream, unstable) && !IsSameStream (stream.bytes, below.bytes) && !IsSameStream (stream.bytes, above.bytes))
        {
            hr = ReadPosition (host, drive, module, position, kWholeReads, read);
            CHR (hr);

            hr = ReadMore (host, drive, module, read);
            CHR (hr);
        }

        positions.push_back (std::move (read));

        if (progress)
        {
            progress (++done, last + 1);
        }
    }

    hr = BuildWoz (positions, imageDate, drive, outResult);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::ReadPosition
//
//  The module is loaded again before each read when a read of the drive
//  overwrote it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCapture::ReadPosition (CassoLinkHost & host, Byte drive, std::span<const Byte> module, int position, int reads, Position & outPosition)
{
    HRESULT  hr = S_OK;



    outPosition          = Position();
    outPosition.position = position;

    for (int read = 0; read < reads; read++)
    {
        std::vector<Byte>  bytes;
        CassoLinkStatus    status;
        Byte               percent = (Byte) (position * 100 / (kLastPosition + 1));
        Byte               delay   = kWholeDelays[read % kWholeReads];

        hr = host.EnsureModule (module);
        CHR (hr);

        hr = host.CaptureQuarterTrack (drive, (Byte) position, delay, percent, percent, bytes, status);
        CHR (hr);

        CBREx (status.error == 0, HRESULT_FROM_WIN32 (ERROR_READ_FAULT));

        outPosition.reads.push_back (std::move (bytes));
        outPosition.delays.push_back (delay);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::ReadMore
//
//  More reads at 8 cycles while an FF byte's zero bits can't be counted
//  from its marks, up to kExtraReads.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCapture::ReadMore (CassoLinkHost & host, Byte drive, std::span<const Byte> module, Position & ioPosition)
{
    HRESULT  hr = S_OK;



    for (int extra = 0; extra < kExtraReads; extra++)
    {
        Stream             stream;
        Marks              marks;
        bool               unstable = false;
        std::vector<Byte>  bytes;
        CassoLinkStatus    status;
        Byte               percent  = (Byte) (ioPosition.position * 100 / (kLastPosition + 1));

        if (!CollectMarks (ioPosition, stream, marks, unstable) || !IsUncertain (stream, marks))
        {
            break;
        }

        hr = host.EnsureModule (module);
        CHR (hr);

        hr = host.CaptureQuarterTrack (drive, (Byte) ioPosition.position, 0, percent, percent, bytes, status);
        CHR (hr);

        CBREx (status.error == 0, HRESULT_FROM_WIN32 (ERROR_READ_FAULT));

        ioPosition.reads.push_back (std::move (bytes));
        ioPosition.delays.push_back (0);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::IsUncertain
//
//  An FF byte with some of its marks at 8 cycles set, but not all, and fewer
//  than kSureSamples of them.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::IsUncertain (const Stream & stream, const Marks & marks)
{
    for (size_t at = 0; at < stream.bytes.size(); at++)
    {
        int  set  = marks.set[0][at];
        int  read = marks.read[0][at];

        if ((stream.bytes[at] | 0x80) == s_kSyncByte && set > 0 && set < read && read < kSureSamples)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::BuildWoz
//
//  Each whole track's stream, then each other position's: a separate one when it
//  has one, or its nearest whole track's when it reads the same.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCapture::BuildWoz (const std::vector<Position> & positions, const std::string & imageDate, Byte drive, Result & outResult)
{
    HRESULT                 hr      = S_OK;
    DiskImage               image;
    std::map<int, Stream>   streams;
    std::map<int, int>      slots;
    WozLoader::Edits        edits;
    int                     slot    = 0;



    outResult = Result();

    for (const Position & position : positions)
    {
        Stream  stream;
        bool    unstable = false;

        if (!MakeStream (position, stream, unstable))
        {
            continue;
        }

        if (unstable)
        {
            outResult.unstable.push_back (position.position);
        }

        if (std::find (stream.weak.begin(), stream.weak.end(), true) != stream.weak.end())
        {
            outResult.weak.push_back (position.position);
        }

        streams.emplace (position.position, std::move (stream));
    }

    image.ClearQuarterTrackMap();

    for (auto & [position, stream] : streams)
    {
        int  whole = FindWholeTrack (streams, position);

        if (whole >= 0)
        {
            continue;
        }

        slots[position] = slot++;
    }

    image.EnsureTrackSlots (slot);

    for (auto & [position, stream] : streams)
    {
        auto  found   = slots.find (position);
        int   target  = (found != slots.end()) ? found->second : slots[FindWholeTrack (streams, position)];

        if (found != slots.end())
        {
            std::vector<Byte> &  packed = image.GetTrackBitsForWrite (target);

            image.ResizeTrack (target, stream.bits.size());
            std::fill (packed.begin(), packed.end(), (Byte) 0);

            for (size_t bit = 0; bit < stream.bits.size(); bit++)
            {
                packed[bit / 8] |= (Byte) (stream.bits[bit] << (7 - bit % 8));
            }

            outResult.streams.push_back (position);
        }

        image.SetQuarterTrackSlot (position, target);
    }

    hr = WozLoader::Serialize (image, outResult.woz);
    CHR (hr);

    edits.meta.push_back ({ "image_date", imageDate });
    edits.meta.push_back ({ "notes", std::format ("Raw capture by Casso Explorer from drive {} over CassoLink", drive) });

    hr = WozLoader::ApplyEdits (outResult.woz, edits);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::FindWholeTrack
//
//  The whole track below or above a position whose stream is the
//  position's, below first; -1 for a whole track's position or one with
//  a separate stream.
//
////////////////////////////////////////////////////////////////////////////////

int CassoLinkCapture::FindWholeTrack (const std::map<int, Stream> & streams, int position)
{
    auto  mine = streams.find (position);



    if (position % 4 == 0 || mine == streams.end())
    {
        return -1;
    }

    for (int whole : { position / 4 * 4, position / 4 * 4 + 4 })
    {
        auto  found = streams.find (whole);

        if (found != streams.end() && IsSameStream (mine->second.bytes, found->second.bytes))
        {
            return whole;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::MakeStream
//
//  The first read's revolution; each byte weak where a later read of the
//  same length differs, and a sync byte where every revolution of every
//  read marks it; and the bits. False when the first read doesn't repeat.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::MakeStream (const Position & position, Stream & outStream, bool & outUnstable)
{
    Marks  marks;



    if (!CollectMarks (position, outStream, marks, outUnstable))
    {
        return false;
    }

    CountZeros (outStream.bytes, marks, outStream.zeros);
    ToBits (outStream, outStream.bits);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::CollectMarks
//
//  The first read's revolution, each byte weak where a later read of the
//  same length differs, and each byte's marks from every revolution of every
//  read. False when the first read doesn't repeat.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::CollectMarks (const Position & position, Stream & outStream, Marks & outMarks, bool & outUnstable)
{
    size_t  length = 0;



    outStream          = Stream();
    outStream.position = position.position;
    outUnstable        = false;

    if (position.reads.empty() || !CaptureStream::FindRevolution (position.reads[0], length))
    {
        return false;
    }

    outStream.bytes.assign (position.reads[0].begin(), position.reads[0].begin() + (ptrdiff_t) length);
    outStream.weak.assign (length, false);

    for (int delay = 0; delay < 2; delay++)
    {
        outMarks.set[delay].assign (length, 0);
        outMarks.read[delay].assign (length, 0);
    }

    for (size_t read = 0; read < position.reads.size(); read++)
    {
        const std::vector<Byte>  & bytes  = position.reads[read];
        size_t                     other  = 0;
        size_t                     offset = 0;
        bool                       found  = read == 0 || CaptureStream::FindRevolution (bytes, other);

        if (read > 0 && (!found || other != length || !FindRotation (outStream.bytes, bytes, offset)))
        {
            outUnstable = true;
            continue;
        }

        int  delay = (read < position.delays.size() && position.delays[read] == 1) ? 1 : 0;

        for (size_t at = 0; at < length; at++)
        {
            size_t  mine = offset + at;

            outStream.weak[at] = outStream.weak[at] || (outStream.bytes[at] | 0x80) != (bytes[mine] | 0x80);

            //  Every revolution of the read that has this byte.
            for (size_t each = mine; each < bytes.size(); each += length)
            {
                outMarks.set[delay][at]  += CaptureStream::IsSync (bytes[each]) ? 1 : 0;
                outMarks.read[delay][at] += 1;
            }
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::CountZeros
//
//  Each run of FF is split into stretches of one count by the most likely
//  sequence of counts, from each byte's marks and a cost for each change
//  between neighbors: a run of sync bytes has one count, and a run that
//  mixes 8-bit and 9-bit FF, as some 13-sector disks have, changes only
//  between the kinds. Any other byte is counted from its marks alone, with
//  no zero bits much the likeliest.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::CountZeros (const std::vector<Byte> & bytes, const Marks & marks, std::vector<uint8_t> & outZeros)
{
    size_t  length = bytes.size();
    size_t  at     = 0;



    outZeros.assign (length, 0);

    while (at < length)
    {
        size_t                               end   = at;
        std::vector<std::array<double, 4>>   score;
        std::vector<std::array<int, 4>>      from;
        bool                                 isRun = (bytes[at] | 0x80) == s_kSyncByte;
        int                                  best  = 0;

        while (end < length && ((bytes[end] | 0x80) == s_kSyncByte) == isRun && (isRun || end == at))
        {
            end++;
        }

        score.resize (end - at);
        from.resize (end - at);

        for (size_t each = at; each < end; each++)
        {
            size_t  index = each - at;

            for (int zeros = 0; zeros < 4; zeros++)
            {
                double  emit  = GetMarkScore (zeros, marks, each) + std::log (isRun ? s_kRunPrior[zeros] : s_kLonePrior[zeros]);
                double  top   = -1e300;
                int     arg   = zeros;

                for (int prior = 0; index > 0 && prior < 4; prior++)
                {
                    double  path = score[index - 1][(size_t) prior] + std::log (prior == zeros ? 1.0 - s_kChange : s_kChange / 3.0);

                    if (path > top)
                    {
                        top = path;
                        arg = prior;
                    }
                }

                score[index][(size_t) zeros] = emit + (index > 0 ? top : 0.0);
                from[index][(size_t) zeros]  = arg;
            }
        }

        for (int zeros = 1; zeros < 4; zeros++)
        {
            best = (score.back()[(size_t) zeros] > score.back()[(size_t) best]) ? zeros : best;
        }

        for (size_t index = end - at; index-- > 0; )
        {
            outZeros[at + index] = (uint8_t) best;
            best                 = from[index][(size_t) best];
        }

        at = end;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::GetMarkScore
//
//  The log of the chance of a byte's marks if it has `zeros` zero bits
//  after it, from the chance of a mark at each delay with that many.
//
////////////////////////////////////////////////////////////////////////////////

double CassoLinkCapture::GetMarkScore (int zeros, const Marks & marks, size_t at)
{
    double  score = 0.0;



    for (int delay = 0; delay < 2; delay++)
    {
        double  chance = s_kMarkChance[delay][zeros];
        int     set    = marks.set[delay][at];
        int     unset  = marks.read[delay][at] - set;

        score += set * std::log (chance) + unset * std::log (1.0 - chance);
    }

    return score;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::FindRotation
//
//  Where in a read's first revolution the revolution starts: of the places
//  its first kAlignBytes bytes are, the one with the fewest bytes that
//  differ over the whole revolution. A track of blank sectors has long runs
//  of one byte, so its first bytes are in many places.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::FindRotation (std::span<const Byte> revolution, std::span<const Byte> read, size_t & outOffset)
{
    size_t  fewest = SIZE_MAX;



    outOffset = 0;

    for (size_t offset = 0; offset < revolution.size() && offset + revolution.size() <= read.size() && fewest > 0; offset++)
    {
        size_t  at     = 0;
        size_t  differ = 0;

        while (at < kAlignBytes && (revolution[at] | 0x80) == (read[offset + at] | 0x80))
        {
            at++;
        }

        if (at < kAlignBytes)
        {
            continue;
        }

        for (; at < revolution.size() && differ < fewest; at++)
        {
            differ += ((revolution[at] | 0x80) != (read[offset + at] | 0x80)) ? 1 : 0;
        }

        if (differ < fewest)
        {
            fewest    = differ;
            outOffset = offset;
        }
    }

    return fewest != SIZE_MAX;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::IsSameStream
//
//  Two revolutions of one length, the second a rotation of the first but
//  for one byte in kSameShare.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::IsSameStream (std::span<const Byte> a, std::span<const Byte> b)
{
    std::vector<Byte>  twice (b.begin(), b.end());
    size_t             offset = 0;
    size_t             differ = 0;



    if (a.size() != b.size() || a.size() < kAlignBytes)
    {
        return false;
    }

    twice.insert (twice.end(), b.begin(), b.end());

    if (!FindRotation (a, twice, offset))
    {
        return false;
    }

    for (size_t at = 0; at < a.size(); at++)
    {
        differ += ((a[at] | 0x80) != (twice[offset + at] | 0x80)) ? 1 : 0;
    }

    return differ * kSameShare <= a.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::ToBits
//
//  Each byte's eight bits, two zero bits after a sync byte, and a weak byte
//  as eight zero bits.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::ToBits (const Stream & stream, std::vector<uint8_t> & outBits)
{
    outBits.clear();

    for (size_t at = 0; at < stream.bytes.size(); at++)
    {
        Byte  value = stream.bytes[at] | 0x80;

        if (stream.weak[at])
        {
            outBits.insert (outBits.end(), 8, (uint8_t) 0);
            continue;
        }

        for (int bit = 7; bit >= 0; bit--)
        {
            outBits.push_back ((uint8_t) ((value >> bit) & 1));
        }

        outBits.insert (outBits.end(), stream.zeros[at], (uint8_t) 0);
    }
}