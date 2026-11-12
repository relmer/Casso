#include "Pch.h"

#include "CassoLink/Session/CassoLinkCapture.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/Session/CaptureStream.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Update/ZipArchive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::Capture
//
//  The whole tracks first, so each other position has its neighbors'
//  streams to compare its first read with, and so the gaps to read are
//  known.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCapture::Capture (CassoLinkHost & host, Byte drive, std::span<const Byte> module, const std::string & imageDate,
                                   const ProgressFn & progress, Result & outResult, int lastTrack)
{
    HRESULT                hr        = S_OK;
    std::vector<Position>  positions;
    std::vector<bool>      standard;
    int                    last      = std::clamp (lastTrack, 0, kTracks - 1) * 4;
    Run                    run       { host, drive, module, progress, outResult };



    outResult   = Result();
    run.started = GetTickCount64();
    run.planned = (last / 4 + 1) * kWholeReads;

    for (int position = 0; position <= last; position += 4)
    {
        Position  read;
        Stream    previous;
        bool      unstable = false;

        //  Each whole track after the first waits on a marker on the one
        //  before, so the tracks can be placed relative to each other.
        if (!positions.empty() && MakeStream (positions.back(), previous, unstable))
        {
            FindMarker (previous.bytes, read.syncMarker);
            read.syncFrom = read.syncMarker.empty() ? -1 : positions.back().position;
        }

        hr = ReadPosition (run, position, kWholeReads, read);
        CHR (hr);

        standard.push_back (IsStandardTrack (read));
        positions.push_back (std::move (read));

        //  The gap below this track is read when either track beside it isn't
        //  standard, so its three positions count as soon as both are known.
        if (standard.size() >= 2 && !(standard[standard.size() - 2] && standard.back()))
        {
            run.planned += 3;
        }
    }

    run.pass = 2;

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

        outResult.betweenTotal++;

        if (standard[(size_t) lower / 4] && standard[(size_t) upper / 4])
        {
            continue;
        }

        outResult.betweenRead++;

        read = Position();

        hr = ReadPosition (run, position, 1, read);
        CHR (hr);

        MakeStream (positions[(size_t) lower / 4], below, unstable);
        MakeStream (positions[(size_t) upper / 4], above, unstable);

        //  A position with a separate stream is read again.
        if (MakeStream (read, stream, unstable) && !IsSameStream (stream.bytes, below.bytes) && !IsSameStream (stream.bytes, above.bytes))
        {
            run.planned += kWholeReads;

            hr = ReadPosition (run, position, kWholeReads, read);
            CHR (hr);

        }

        positions.push_back (std::move (read));
    }

    hr = BuildWoz (positions, last, imageDate, drive, outResult);
    CHR (hr);

    outResult.positions = std::move (positions);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::SerializeReads
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::SerializeReads (const std::vector<Position> & positions, std::vector<Byte> & outFile)
{
    auto  put = [&outFile] (uint32_t value, int bytes)
    {
        for (int each = 0; each < bytes; each++)
        {
            outFile.push_back ((Byte) (value >> (each * 8)));
        }
    };



    outFile.assign (std::begin (s_kReadsMagic), std::end (s_kReadsMagic));
    put (s_kReadsVersion, 2);
    put ((uint32_t) positions.size(), 4);

    for (const Position & position : positions)
    {
        put ((uint32_t) position.position, 2);
        put ((uint32_t) position.reads.size(), 4);
        put ((uint32_t) (uint16_t) position.syncFrom, 2);
        put ((uint32_t) position.syncMarker.size(), 1);
        outFile.insert (outFile.end(), position.syncMarker.begin(), position.syncMarker.end());

        for (size_t read = 0; read < position.reads.size(); read++)
        {
            put ((read < position.delays.size()) ? position.delays[read] : 0, 1);
            put ((uint32_t) position.reads[read].size(), 4);
            outFile.insert (outFile.end(), position.reads[read].begin(), position.reads[read].end());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::ParseReads
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCapture::ParseReads (std::span<const Byte> file, std::vector<Position> & outPositions)
{
    HRESULT   hr      = S_OK;
    size_t    at      = 0;
    uint32_t  count   = 0;
    uint32_t  version = 0;
    bool      valid   = file.size() >= sizeof (s_kReadsMagic) + 6 && std::equal (std::begin (s_kReadsMagic), std::end (s_kReadsMagic), file.begin());
    auto      get     = [&file, &at, &valid] (int bytes)
    {
        uint32_t  value = 0;

        valid = valid && at + (size_t) bytes <= file.size();

        for (int each = 0; valid && each < bytes; each++)
        {
            value |= (uint32_t) file[at++] << (each * 8);
        }

        return value;
    };



    outPositions.clear();

    at      = sizeof (s_kReadsMagic);
    version = get (2);
    valid   = valid && (version == 1 || version == s_kReadsVersion);
    count   = get (4);

    for (uint32_t each = 0; valid && each < count; each++)
    {
        Position  position;
        uint32_t  reads = 0;

        position.position = (int) get (2);
        reads             = get (4);

        if (version >= 2)
        {
            uint32_t  marker = 0;

            position.syncFrom = (int) (int16_t) get (2);
            marker            = get (1);
            valid             = valid && at + marker <= file.size();

            if (valid)
            {
                position.syncMarker.assign (file.begin() + (ptrdiff_t) at, file.begin() + (ptrdiff_t) (at + marker));
                at += marker;
            }
        }

        for (uint32_t read = 0; valid && read < reads; read++)
        {
            Byte      delay  = (Byte) get (1);
            uint32_t  length = get (4);

            valid = valid && at + length <= file.size();

            if (valid)
            {
                position.delays.push_back (delay);
                position.reads.emplace_back (file.begin() + (ptrdiff_t) at, file.begin() + (ptrdiff_t) (at + length));
                at += length;
            }
        }

        outPositions.push_back (std::move (position));
    }

    valid = valid && at == file.size();
    CBREx (valid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::ReadOnce
//
//  The module is loaded again before the read when a read of the drive
//  overwrote it. The Apple's bar has the capture's share done.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCapture::ReadOnce (Run & run, int position, Byte delay, int read, int reads, Byte syncPosition, std::span<const Byte> marker, std::vector<Byte> & outBytes, bool & outMissed)
{
    HRESULT          hr       = S_OK;
    CassoLinkStatus  status;
    CaptureTiming    timing;
    Progress         report;
    ULONGLONG        elapsed  = 0;
    Byte             after    = 0;



    outMissed = false;

    //  The Apple's bar has the share the capture has once this read is done,
    //  so the last read draws it full.
    after = (Byte) std::max (run.percent, std::min (100, (run.done + 1) * 100 / std::max (run.planned, run.done + 1)));

    //  A request the line drops is made again, with the module loaded again
    //  when the program no longer has it.
    for (int attempt = 0; attempt < s_kReadAttempts; attempt++)
    {
        hr = run.host.EnsureModule (run.module);

        if (SUCCEEDED (hr))
        {
            hr = run.host.CaptureQuarterTrack (run.drive, (Byte) position, delay, after, after, syncPosition, marker, outBytes, status, timing);
        }

        if (SUCCEEDED (hr) || hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED))
        {
            break;
        }
    }

    CHR (hr);

    //  A marker that didn't pass: the caller reads again without one.
    outMissed = syncPosition != CassoLinkHost::kNoSync && status.error == CassoLinkHost::kErrorNotFound;
    BAIL_OUT_IF (outMissed, S_OK);

    CBREx (status.error == 0, HRESULT_FROM_WIN32 (ERROR_READ_FAULT));

    run.done++;
    run.result.reads++;
    run.result.diskMs      += timing.diskMs;
    run.result.lineMs      += timing.lineMs;
    run.result.sentBytes   += timing.sentBytes;
    run.result.storedBytes += outBytes.size();

    //  The share never goes back, though the plan grows when a position has a
    //  separate stream, and it is 100 only when the capture ends.
    run.percent = std::max (run.percent, std::min (99, run.done * 100 / std::max (run.planned, run.done)));
    elapsed     = GetTickCount64() - run.started;

    report.position    = position;
    report.read        = read;
    report.reads       = reads;
    report.pass        = run.pass;
    report.percent     = run.percent;
    report.secondsLeft = (run.done >= s_kPaceReads) ? (double) elapsed / 1000.0 / run.done * std::max (0, run.planned - run.done) : -1.0;

    if (run.progress)
    {
        run.progress (report);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::ReadPosition
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCapture::ReadPosition (Run & run, int position, int reads, Position & outPosition)
{
    HRESULT  hr = S_OK;



    outPosition.position = position;
    outPosition.reads.clear();
    outPosition.delays.clear();

    for (int read = 0; read < reads; read++)
    {
        std::vector<Byte>  bytes;
        Byte               delay  = kWholeDelays[read % kWholeReads];
        bool               sync   = read == 0 && outPosition.syncFrom >= 0;
        bool               missed = false;

        hr = ReadOnce (run, position, delay, read + 1, reads, sync ? (Byte) outPosition.syncFrom : CassoLinkHost::kNoSync,
                       sync ? std::span<const Byte> (outPosition.syncMarker) : std::span<const Byte>(), bytes, missed);
        CHR (hr);

        if (missed)
        {
            outPosition.syncFrom = -1;

            hr = ReadOnce (run, position, delay, read + 1, reads, CassoLinkHost::kNoSync, {}, bytes, missed);
            CHR (hr);
        }

        outPosition.reads.push_back (std::move (bytes));
        outPosition.delays.push_back (delay);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::IsStandardTrack
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::IsStandardTrack (const Position & position)
{
    Stream  stream;
    bool    unstable  = false;
    bool    thirteen  = false;
    int     addressed = 0;
    bool    hasStream = MakeStream (position, stream, unstable);
    bool    clean     = hasStream && !unstable && std::find (stream.weak.begin(), stream.weak.end(), true) == stream.weak.end();
    int     sectors   = clean ? CountSectors (stream.bytes, position.position / 4, thirteen, addressed) : 0;



    return clean && sectors == (thirteen ? 13 : 16);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::CountSectors
//
//  The sectors of a revolution with an address field for the track, its
//  checksum right, and a data field's prolog after it; a 16-sector data
//  field must also decode. A field across the revolution's end is found in
//  the revolution's start, which follows its end on the disk. outThirteen
//  is set by the first address field found, and outAddressed is the
//  sectors with an address field for the track, whether or not their data
//  decodes.
//
////////////////////////////////////////////////////////////////////////////////

int CassoLinkCapture::CountSectors (std::span<const Byte> revolution, int track, bool & outThirteen, int & outAddressed)
{
    std::vector<Byte>  bytes;
    std::vector<Byte>  sector;
    std::set<int>      found;
    std::set<int>      addressed;
    bool               kindKnown = false;
    size_t             wrap      = std::min (revolution.size(), (size_t) NibblizationLayer::kDataNibbleCount + 2 * s_kDataWithin);



    outThirteen = false;

    for (Byte value : revolution)
    {
        bytes.push_back (value | 0x80);
    }

    bytes.insert (bytes.end(), bytes.begin(), bytes.begin() + (ptrdiff_t) wrap);

    for (size_t at = 0; at + 11 < bytes.size() && at < revolution.size(); at++)
    {
        bool    is16    = bytes[at] == s_kProlog0     && bytes[at + 1] == s_kProlog1 && bytes[at + 2] == s_kAddress16;
        bool    is13    = (bytes[at] == s_kProlog0    && bytes[at + 1] == s_kProlog1 && bytes[at + 2] == s_kAddress13) ||
                          (bytes[at] == s_kRdosProlog0 && bytes[at + 1] == s_kProlog1 && bytes[at + 2] == s_kRdosAddress);
        auto    decode  = [&bytes, at] (size_t field) { return (Byte) (((bytes[at + 3 + field * 2] << 1) | 1) & bytes[at + 4 + field * 2]); };
        Byte    volume  = 0;
        Byte    onTrack = 0;
        Byte    number  = 0;
        size_t  data    = 0;
        bool    hasData = false;

        if (!is16 && !is13)
        {
            continue;
        }

        volume  = decode (0);
        onTrack = decode (1);
        number  = decode (2);

        if ((volume ^ onTrack ^ number) != decode (3) || onTrack != track || number >= (is13 ? 13 : 16))
        {
            continue;
        }

        addressed.insert (number);

        if (!kindKnown)
        {
            kindKnown   = true;
            outThirteen = is13;
        }

        for (data = at + 11; data + 3 + NibblizationLayer::kDataNibbleCount <= bytes.size() && data < at + 11 + s_kDataWithin && !hasData; data++)
        {
            hasData = bytes[data] == s_kProlog0 && bytes[data + 1] == s_kProlog1 && bytes[data + 2] == s_kData;
        }

        //  The loop has gone one past the prolog.
        if (hasData && is16)
        {
            hasData = NibblizationLayer::DecodeDataNibbles (&bytes[data + 2], sector);
        }

        if (hasData && is13 == outThirteen)
        {
            found.insert (number);
        }
    }

    outAddressed = (int) addressed.size();

    return (int) found.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::IsCrosstalk
//
//  Whether a revolution read between two tracks has address fields for
//  either one with sectors whose data fields don't decode.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::IsCrosstalk (std::span<const Byte> revolution, int position)
{
    bool  crosstalk = false;



    for (int track : { position / 4, position / 4 + 1 })
    {
        bool  thirteen  = false;
        int   addressed = 0;
        int   good      = CountSectors (revolution, track, thirteen, addressed);

        crosstalk = crosstalk || addressed > good;
    }

    return crosstalk;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::FindMarker
//
//  The first address field in a revolution whose s_kMarkerBytes bytes are
//  nowhere else in it, as the marker a read of the next track waits on;
//  empty on a track without such a field.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::FindMarker (const std::vector<Byte> & revolution, std::vector<Byte> & outMarker)
{
    std::vector<Byte>  bytes;



    outMarker.clear();

    for (Byte value : revolution)
    {
        bytes.push_back (value | 0x80);
    }

    for (size_t at = 0; outMarker.empty() && at + s_kMarkerBytes <= bytes.size(); at++)
    {
        std::span<const Byte>  field (bytes.data() + at, s_kMarkerBytes);
        size_t                 count = 0;

        if (bytes[at] != s_kProlog0 || bytes[at + 1] != s_kProlog1 || (bytes[at + 2] != s_kAddress16 && bytes[at + 2] != s_kAddress13))
        {
            continue;
        }

        for (size_t other = 0; other + s_kMarkerBytes <= bytes.size(); other++)
        {
            count += std::equal (field.begin(), field.end(), bytes.begin() + (ptrdiff_t) other) ? 1 : 0;
        }

        if (count == 1)
        {
            outMarker.assign (field.begin(), field.end());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::GetBitOffset
//
//  The bits of a stream before one of its bytes, as ToBits writes them.
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoLinkCapture::GetBitOffset (const Stream & stream, size_t byte)
{
    return byte < stream.starts.size() ? stream.starts[byte] : stream.bits.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::Write32
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::Write32 (std::vector<Byte> & bytes, size_t at, uint32_t value)
{
    for (size_t each = 0; each < 4; each++)
    {
        bytes[at + each] = (Byte) (value >> (each * 8));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::PlaceTracks
//
//  Each whole track's rotation: the bits its stream is turned by in the WOZ,
//  so that a bit's place in every track is the same angle. Track 0 isn't
//  turned. A track whose first read waited on a marker on the track before
//  is placed by it: the marker's last bit is at that track's angle for it,
//  the read's first byte s_kSyncCycles later, a revolution being the
//  track's bits at s_kBitCycles each, and the stream's first byte the bytes
//  before it after that. A track without one starts at angle 0, and the
//  tracks aren't synchronized.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::PlaceTracks (const std::vector<Position> & positions, const std::map<int, Stream> & streams, std::map<int, size_t> & outRotations, bool & outSynchronized)
{
    std::map<int, double>  angles;     // each placed track's angle of its stream's first bit



    outRotations.clear();
    outSynchronized = true;

    for (const Position & position : positions)
    {
        auto    mine     = streams.find (position.position);
        auto    previous = streams.find (position.syncFrom);
        auto    placed   = angles.find (position.syncFrom);
        double  angle    = 0.0;

        if (position.position % 4 != 0 || mine == streams.end())
        {
            continue;
        }

        if (position.syncFrom >= 0 && previous != streams.end() && placed != angles.end())
        {
            const Stream                 & before = previous->second;
            const Stream                 & stream = mine->second;
            std::vector<Byte>              bytes;
            std::vector<Byte>::iterator    found;
            double                         marker = 0.0;
            double                         bits   = (double) stream.bits.size();

            for (Byte value : before.bytes)
            {
                bytes.push_back (value | 0x80);
            }

            found = std::search (bytes.begin(), bytes.end(), position.syncMarker.begin(), position.syncMarker.end());

            if (found != bytes.end())
            {
                size_t  last = (size_t) (found - bytes.begin()) + position.syncMarker.size();

                marker = placed->second + (double) (GetBitOffset (before, last - 1) + 8) / (double) before.bits.size();
                angle  = marker + s_kSyncCycles / (s_kBitCycles * bits) + (double) stream.start / (double) stream.bytes.size();
            }
            else
            {
                outSynchronized = false;
            }
        }
        else if (position.position != 0)
        {
            outSynchronized = false;
        }

        angle -= std::floor (angle);
        angles[position.position]       = angle;
        outRotations[position.position] = (size_t) std::lround (angle * (double) mine->second.bits.size()) % std::max<size_t> (mine->second.bits.size(), 1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::BuildWoz
//
//  Each whole track's stream, then each other position's: a separate one when it
//  has one, or its nearest whole track's when it reads the same. A position
//  up to one past lastPosition that wasn't read has a standard disk's entry:
//  a quarter track its nearer whole track's stream, a half track none.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkCapture::BuildWoz (const std::vector<Position> & positions, int lastPosition, const std::string & imageDate, Byte drive, Result & outResult)
{
    HRESULT                 hr      = S_OK;
    DiskImage               image;
    std::map<int, Stream>   streams;
    std::map<int, int>      slots;
    std::set<int>           read;
    std::map<int, size_t>   rotations;
    WozLoader::Edits        edits;
    int                     slot    = 0;



    outResult.woz.clear();
    outResult.unstable.clear();
    outResult.weak.clear();
    outResult.streams.clear();

    for (const Position & position : positions)
    {
        Stream  stream;
        bool    unstable = false;
        bool    hasWeak  = false;

        read.insert (position.position);

        if (!MakeStream (position, stream, unstable))
        {
            continue;
        }

        hasWeak = std::find (stream.weak.begin(), stream.weak.end(), true) != stream.weak.end();

        //  A head between two tracks reads both at once: its reads differ, or
        //  it has a neighbor's address fields with data fields that don't
        //  decode. Such a stream is dropped, and the position has a standard
        //  disk's entry. A stream a protection wrote there repeats cleanly.
        if (position.position % 4 != 0 && (unstable || hasWeak || IsCrosstalk (stream.bytes, position.position)))
        {
            read.erase (position.position);
            continue;
        }

        if (unstable)
        {
            outResult.unstable.push_back (position.position);
        }

        if (hasWeak)
        {
            outResult.weak.push_back (position.position);
        }

        streams.emplace (position.position, std::move (stream));
    }

    PlaceTracks (positions, streams, rotations, outResult.synchronized);

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

            size_t  turn = rotations.contains (position) ? rotations[position] : 0;

            for (size_t bit = 0; bit < stream.bits.size(); bit++)
            {
                size_t  to = (bit + turn) % stream.bits.size();

                packed[to / 8] |= (Byte) (stream.bits[bit] << (7 - to % 8));
            }

            outResult.streams.push_back (position);
        }

        image.SetQuarterTrackSlot (position, target);
    }

    for (int position = 1; position <= lastPosition + 1 && position < CassoLinkCodec::kQuarterTracks; position++)
    {
        int   nearer = (position % 4 == 1) ? position - 1 : position + 1;
        auto  found  = slots.find (nearer);

        if (position % 4 == 0 || position % 4 == 2 || read.contains (position) || found == slots.end())
        {
            continue;
        }

        image.SetQuarterTrackSlot (position, found->second);
    }

    hr = WozLoader::Serialize (image, outResult.woz);
    CHR (hr);

    edits.meta.push_back ({ "image_date", imageDate });
    edits.meta.push_back ({ "notes", std::format ("Raw capture by Casso Explorer from drive {} over CassoLink", drive) });

    hr = WozLoader::ApplyEdits (outResult.woz, edits);
    CHR (hr);

    //  The INFO chunk's synchronized flag, which Serialize writes as 0, and
    //  the header's CRC of everything after the header again.
    if (outResult.synchronized && outResult.woz.size() > s_kInfoSynchronized)
    {
        outResult.woz[s_kInfoSynchronized] = 1;
        Write32 (outResult.woz, s_kWozCrcOffset, ZipArchive::ComputeCrc32 (std::span<const Byte> (outResult.woz).subspan (s_kWozHeaderBytes)));
    }

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
    ToBits (outStream, outStream.bits, outStream.starts);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::CollectMarks
//
//  The first read's revolution, from a stable start (FindReference), made the
//  consensus of every revolution of every read aligned to it (AlignAll,
//  MakeConsensus), and every revolution aligned to that. A byte is weak
//  where at least two alignments, and at least half of them, put it in a
//  stretch that differs, longer than the few bytes a write splice frames
//  differently on each pass. Each
//  byte's marks come from the alignments that read it the same. A read with
//  no revolution that aligns makes the stream unstable. False when the first
//  read doesn't repeat.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::CollectMarks (const Position & position, Stream & outStream, Marks & outMarks, bool & outUnstable)
{
    size_t                  start  = 0;
    size_t                  length = 0;
    std::vector<Alignment>  alignments;
    std::vector<Byte>       agreed;
    std::vector<int>        differs;



    outStream          = Stream();
    outStream.position = position.position;
    outUnstable        = false;

    if (position.reads.empty() || !FindReference (position.reads[0], start, length))
    {
        return false;
    }

    outStream.start = start;
    outStream.bytes.assign (position.reads[0].begin() + (ptrdiff_t) start, position.reads[0].begin() + (ptrdiff_t) (start + length));

    AlignAll (position, outStream.bytes, alignments, outUnstable);
    MakeConsensus (position, outStream.bytes, alignments, agreed);

    //  Where most passes read a stretch otherwise than the first revolution
    //  did, the stream has theirs, and everything is aligned to it again.
    if (agreed != outStream.bytes)
    {
        outStream.bytes = std::move (agreed);

        AlignAll (position, outStream.bytes, alignments, outUnstable);
    }

    length = outStream.bytes.size();
    outStream.weak.assign (length, false);
    differs.assign (length, 0);

    for (int delay = 0; delay < 2; delay++)
    {
        outMarks.set[delay].assign (length, 0);
        outMarks.read[delay].assign (length, 0);
    }

    for (const Alignment & alignment : alignments)
    {
        const std::vector<Byte>  & bytes = position.reads[alignment.read];
        int                        delay = (alignment.read < position.delays.size() && position.delays[alignment.read] == 1) ? 1 : 0;

        for (size_t at = 0; at < length; at++)
        {
            int  mine = alignment.map[at];

            differs[at] += alignment.stretch[at] ? 1 : 0;

            if (mine >= 0 && (bytes[(size_t) mine] | 0x80) == (outStream.bytes[at] | 0x80))
            {
                outMarks.set[delay][at]  += CaptureStream::IsSync (bytes[(size_t) mine]) ? 1 : 0;
                outMarks.read[delay][at] += 1;
            }
        }
    }

    for (size_t at = 0; at < length; at++)
    {
        outStream.weak[at] = differs[at] >= s_kWeakAlignments && (size_t) differs[at] * 2 >= alignments.size();
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::AlignAll
//
//  The best alignment of each revolution of each read to a stream: a start
//  a sector away can align too on a track of empty sectors, with more
//  differences. A read none of whose revolutions aligns makes the stream
//  unstable.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::AlignAll (const Position & position, const std::vector<Byte> & revolution, std::vector<Alignment> & outAlignments, bool & outUnstable)
{
    size_t  length = revolution.size();



    outAlignments.clear();
    outUnstable = false;

    for (size_t read = 0; read < position.reads.size(); read++)
    {
        std::vector<Alignment>  found;
        std::vector<size_t>     taken;

        for (size_t from : FindAnchors (revolution, position.reads[read]))
        {
            Alignment  alignment;

            alignment.read = read;
            alignment.from = from;

            if (Align (revolution, position.reads[read], from, alignment.map, alignment.differ, alignment.stretch))
            {
                found.push_back (std::move (alignment));
            }
        }

        std::sort (found.begin(), found.end(), [] (const Alignment & a, const Alignment & b) { return a.differ < b.differ; });

        for (Alignment & alignment : found)
        {
            if (std::any_of (taken.begin(), taken.end(), [&alignment, length] (size_t from)
                { return (alignment.from > from ? alignment.from - from : from - alignment.from) + s_kBand < length; }))
            {
                continue;
            }

            taken.push_back (alignment.from);
            outAlignments.push_back (std::move (alignment));
        }

        outUnstable = outUnstable || taken.empty();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::MakeConsensus
//
//  The stream with each stretch most alignments differ on replaced by the
//  bytes most of them read there, when more than half read the same. A
//  stretch is the bytes the alignments mark as differing, read with more
//  or fewer, or missing, within s_kConsensusGap of each other; each
//  alignment's version is the read's bytes between those of the stretch's
//  neighbors, when the alignment reads both neighbors the same.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::MakeConsensus (const Position & position, const std::vector<Byte> & revolution, const std::vector<Alignment> & alignments, std::vector<Byte> & outAgreed)
{
    size_t                  length = revolution.size();
    std::vector<size_t>     marked (length, 0);
    std::vector<size_t>     region;
    size_t                  copied = 0;



    outAgreed.clear();

    for (const Alignment & alignment : alignments)
    {
        const std::vector<Byte>  & bytes = position.reads[alignment.read];

        for (size_t at = 0; at < length; at++)
        {
            int   mine = alignment.map[at];
            bool  odd  = mine < 0 || (bytes[(size_t) mine] | 0x80) != (revolution[at] | 0x80);

            odd = odd || (at > 0 && mine >= 0 && alignment.map[at - 1] >= 0 && mine - alignment.map[at - 1] > 1);

            marked[at] += odd ? 1 : 0;
        }
    }

    for (size_t at = 1; at + 1 < length; at++)
    {
        if (marked[at] * 2 > alignments.size())
        {
            region.push_back (at);
        }
    }

    for (size_t first = 0; first < region.size(); )
    {
        size_t                                last   = first;
        size_t                                before = 0;
        size_t                                after  = 0;
        std::map<std::vector<Byte>, size_t>   votes;
        std::vector<Byte>                     mineBytes;
        const std::vector<Byte>             * winner = nullptr;
        size_t                                most   = 0;
        size_t                                cast   = 0;

        while (last + 1 < region.size() && region[last + 1] - region[last] <= s_kConsensusGap)
        {
            last++;
        }

        before = region[first] - 1;
        after  = region[last] + 1;
        first  = last + 1;

        for (size_t at = before + 1; at < after; at++)
        {
            mineBytes.push_back (revolution[at] | 0x80);
        }

        for (const Alignment & alignment : alignments)
        {
            const std::vector<Byte>  & bytes = position.reads[alignment.read];
            int                        lo    = alignment.map[before];
            int                        hi    = alignment.map[after];
            std::vector<Byte>          theirs;

            if (lo < 0 || hi <= lo || (bytes[(size_t) lo] | 0x80) != (revolution[before] | 0x80) || (bytes[(size_t) hi] | 0x80) != (revolution[after] | 0x80))
            {
                continue;
            }

            for (int at = lo + 1; at < hi; at++)
            {
                theirs.push_back (bytes[(size_t) at] | 0x80);
            }

            votes[theirs]++;
            cast++;
        }

        for (const auto & [bytes, count] : votes)
        {
            if (count > most)
            {
                most   = count;
                winner = &bytes;
            }
        }

        if (winner == nullptr || *winner == mineBytes || most * 2 <= cast)
        {
            continue;
        }

        outAgreed.insert (outAgreed.end(), revolution.begin() + (ptrdiff_t) copied, revolution.begin() + (ptrdiff_t) before + 1);
        outAgreed.insert (outAgreed.end(), winner->begin(), winner->end());
        copied = after;
    }

    outAgreed.insert (outAgreed.end(), revolution.begin() + (ptrdiff_t) copied, revolution.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::FindReference
//
//  A revolution of a read that starts and ends in stretches every pass reads
//  nearly the same. The length is the one at which the read differs from
//  itself least (FindLength), then from the first start, in steps of
//  s_kStartStep, whose kAlignBytes bytes have several values, once in the
//  revolution (IsUnique), the start repeats within s_kAnchorMismatch bytes
//  within s_kWindowDrift of that length, the drift a write splice between
//  the two can add. A start in a write splice, whose bytes are
//  framed differently on each pass, doesn't repeat and is skipped.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::FindReference (std::span<const Byte> read, size_t & outStart, size_t & outLength)
{
    size_t  estimate = FindLength (read);
    bool    found    = false;



    outStart  = 0;
    outLength = 0;

    for (size_t start = 0; estimate > 0 && !found && start + estimate + s_kWindowDrift + kAlignBytes <= read.size(); start += s_kStartStep)
    {
        std::set<Byte>  values;
        size_t          fewest = SIZE_MAX;

        for (size_t at = 0; at < kAlignBytes; at++)
        {
            values.insert (read[start + at] | 0x80);
        }

        if (values.size() < s_kAnchorValues)
        {
            continue;
        }

        for (size_t length = estimate - s_kWindowDrift; length <= estimate + s_kWindowDrift; length++)
        {
            size_t  differ = CountDiffer (read, start, read, start + length, s_kAnchorMismatch);

            if (differ <= s_kAnchorMismatch && differ < fewest)
            {
                fewest    = differ;
                outLength = length;
            }
        }

        found    = fewest != SIZE_MAX && IsUnique (read, start, outLength);
        outStart = start;
    }

    if (!found)
    {
        outStart  = 0;
        outLength = 0;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::FindLength
//
//  The revolution's length, near enough: the distance from an address
//  field to the same field again (FindRepeat) when the read has one;
//  otherwise, of the lengths from
//  s_kShortestRevolution to s_kLongestRevolution, the one at which the
//  fewest of every s_kSampleStep'th byte of the read's first s_kSampleBytes
//  differ from all the bytes within s_kSampleDrift of the byte that length
//  later, as each write splice can shift the rest by a byte. 0 when even
//  that one differs in more than one sample in s_kNoiseShare, as noise from
//  an unformatted track does.
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoLinkCapture::FindLength (std::span<const Byte> read)
{
    size_t  best    = FindRepeat (read);
    size_t  fewest  = SIZE_MAX;
    size_t  samples = 0;



    if (best != 0)
    {
        return best;
    }

    for (size_t length = s_kShortestRevolution; length <= s_kLongestRevolution && length < read.size(); length++)
    {
        size_t  end    = std::min (s_kSampleBytes, read.size() - length);
        size_t  differ = 0;
        size_t  count  = 0;

        for (size_t at = s_kSampleDrift; at < end && differ < fewest; at += s_kSampleStep)
        {
            bool  same = false;

            for (size_t other = at + length - s_kSampleDrift; !same && other <= at + length + s_kSampleDrift && other < read.size(); other++)
            {
                same = (read[at] | 0x80) == (read[other] | 0x80);
            }

            differ += same ? 0 : 1;
            count++;
        }

        if (differ < fewest)
        {
            fewest  = differ;
            best    = length;
            samples = count;
        }
    }

    return (fewest * s_kNoiseShare <= samples) ? best : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::FindRepeat
//
//  The bytes from the read's first address field to the same s_kMarkerBytes
//  bytes again, marks ignored, between s_kShortestRevolution and
//  s_kLongestRevolution later: a revolution, as an address field is once on
//  its track. A shift-tolerant sample matches at almost any length on a
//  track of one value repeated, which a protected disk's filler is. 0 for a
//  read with no address field that repeats.
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoLinkCapture::FindRepeat (std::span<const Byte> read)
{
    size_t  length = 0;



    for (size_t at = 0; length == 0 && at + s_kLongestRevolution + s_kMarkerBytes <= read.size(); at++)
    {
        Byte  first  = read[at]     | 0x80;
        Byte  second = read[at + 1] | 0x80;
        Byte  third  = read[at + 2] | 0x80;
        bool  disk   = first == s_kProlog0     && second == s_kProlog1 && (third == s_kAddress16 || third == s_kAddress13);
        bool  rdos   = first == s_kRdosProlog0 && second == s_kProlog1 && third == s_kRdosAddress;

        if (!disk && !rdos)
        {
            continue;
        }

        for (size_t later = s_kShortestRevolution; length == 0 && later <= s_kLongestRevolution; later++)
        {
            size_t  same = 0;

            while (same < s_kMarkerBytes && (read[at + same] | 0x80) == (read[at + later + same] | 0x80))
            {
                same++;
            }

            length = same == s_kMarkerBytes ? later : 0;
        }
    }

    return length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::CountDiffer
//
//  The bytes of kAlignBytes from `a` at aAt that differ from those of `b`
//  at bAt, marks ignored, counting no further than one past `limit`.
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoLinkCapture::CountDiffer (std::span<const Byte> a, size_t aAt, std::span<const Byte> b, size_t bAt, size_t limit)
{
    size_t  differ = 0;



    for (size_t at = 0; at < kAlignBytes && differ <= limit; at++)
    {
        differ += ((a[aAt + at] | 0x80) != (b[bAt + at] | 0x80)) ? 1 : 0;
    }

    return differ;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::IsUnique
//
//  Whether a start's kAlignBytes bytes are nowhere else in its revolution,
//  so each revolution of a read has one place to align from. A window in an
//  empty sector, or in a sector with the same data as another, isn't. Two
//  address fields can differ in as few as two bytes, so the test is exact;
//  FindAnchors' looser match can find a window more than once, and
//  CollectMarks takes the best alignment of each revolution.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::IsUnique (std::span<const Byte> read, size_t start, size_t length)
{
    bool  unique = true;



    for (size_t other = start + kAlignBytes / 2; unique && other + s_kWindowDrift < start + length && other + kAlignBytes <= read.size(); other++)
    {
        unique = CountDiffer (read, start, read, other, 0) > 0;
    }

    return unique;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::FindAnchors
//
//  The places in a read a revolution may start, in order, each with room
//  for the whole revolution after it: where its first kAlignBytes bytes
//  are, and where the windows s_kWindowStep apart in it are, less their
//  offset, for a read that framed the first bytes differently; each within
//  s_kAnchorMismatch bytes. A window found elsewhere in the revolution too
//  is passed over. Places within s_kWindowDrift of one already found are
//  the same place.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<size_t> CassoLinkCapture::FindAnchors (std::span<const Byte> revolution, std::span<const Byte> read)
{
    std::vector<size_t>  anchors;



    for (size_t window = 0; window + kAlignBytes <= revolution.size(); window += s_kWindowStep)
    {
        if (window > 0 && !IsUnique (revolution, window, revolution.size() - window))
        {
            continue;
        }

        for (size_t at = window; at + kAlignBytes <= read.size(); at++)
        {
            size_t  start = at - window;

            if (start + revolution.size() + s_kBand > read.size() || CountDiffer (revolution, window, read, at, s_kAnchorMismatch) > s_kAnchorMismatch)
            {
                continue;
            }

            if (std::none_of (anchors.begin(), anchors.end(), [start] (size_t known) { return (start > known ? start - known : known - start) <= s_kWindowDrift; }))
            {
                anchors.push_back (start);
            }
        }
    }

    std::sort (anchors.begin(), anchors.end());

    return anchors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::Align
//
//  A revolution aligned with a read from `from`, allowing bytes the read has
//  more or fewer within s_kBand of the diagonal, at least cost: a byte read
//  differently, one more or one fewer each cost one. The map has each byte's
//  place in the read, -1 for one the read doesn't have. A byte in a stretch
//  of differences longer than s_kSpliceBytes, with no more than
//  s_kSpliceBytes alike between them, is marked in outStretch. False when
//  more than one byte in s_kAlignShare differs.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::Align (std::span<const Byte> revolution, std::span<const Byte> read, size_t from,
                              std::vector<int> & outMap, size_t & outDiffer, std::vector<bool> & outStretch)
{
    const int              band   = (int) s_kBand;
    const int              width  = 2 * band + 1;
    size_t                 n      = revolution.size();
    size_t                 m      = std::min (n + s_kBand, read.size() - from);
    std::vector<int>       cost  ((n + 1) * (size_t) width, INT_MAX / 2);
    std::vector<Byte>      step  ((n + 1) * (size_t) width, 0);
    std::vector<size_t>    marked;
    auto                   cell   = [width, band] (size_t i, int j) { return i * (size_t) width + (size_t) (j - (int) i + band); };
    int                    bestJ  = -1;
    int                    best   = INT_MAX;
    size_t                 i      = n;
    int                    j      = 0;



    outMap.assign (n, -1);
    outStretch.assign (n, false);
    outDiffer = 0;

    for (int column = 0; column <= band && column <= (int) m; column++)
    {
        cost[cell (0, column)] = column;
        step[cell (0, column)] = s_kStepMore;
    }

    for (size_t row = 1; row <= n; row++)
    {
        for (int column = std::max (0, (int) row - band); column <= std::min ((int) m, (int) row + band); column++)
        {
            int   here = INT_MAX / 2;
            Byte  how  = 0;

            if (column > 0)
            {
                bool  same = (revolution[row - 1] | 0x80) == (read[from + (size_t) column - 1] | 0x80);

                here = cost[cell (row - 1, column - 1)] + (same ? 0 : 1);
                how  = s_kStepPair;
            }

            if (column - (int) row + 1 <= band && cost[cell (row - 1, column)] + 1 < here)
            {
                here = cost[cell (row - 1, column)] + 1;
                how  = s_kStepFewer;
            }

            if (column > 0 && column - 1 - (int) row >= -band && cost[cell (row, column - 1)] + 1 < here)
            {
                here = cost[cell (row, column - 1)] + 1;
                how  = s_kStepMore;
            }

            cost[cell (row, column)] = here;
            step[cell (row, column)] = how;
        }
    }

    for (int column = std::max (0, (int) n - band); column <= std::min ((int) m, (int) n + band); column++)
    {
        if (cost[cell (n, column)] < best)
        {
            best  = cost[cell (n, column)];
            bestJ = column;
        }
    }

    outDiffer = (size_t) best;

    if (bestJ < 0 || outDiffer * s_kAlignShare > n)
    {
        return false;
    }

    for (j = bestJ; i > 0; )
    {
        Byte  how = step[cell (i, j)];

        if (how == s_kStepPair)
        {
            outMap[i - 1] = (int) from + j - 1;

            if ((revolution[i - 1] | 0x80) != (read[from + (size_t) j - 1] | 0x80))
            {
                marked.push_back (i - 1);
            }

            i--;
            j--;
        }
        else if (how == s_kStepFewer)
        {
            marked.push_back (i - 1);
            i--;
        }
        else
        {
            marked.push_back ((i > 0) ? i - 1 : 0);
            j--;
        }
    }

    std::sort (marked.begin(), marked.end());

    for (size_t first = 0; first < marked.size(); )
    {
        size_t  last = first;

        while (last + 1 < marked.size() && marked[last + 1] - marked[last] <= s_kSpliceBytes)
        {
            last++;
        }

        if (marked[last] - marked[first] + 1 > s_kSpliceBytes)
        {
            std::fill (outStretch.begin() + (ptrdiff_t) marked[first], outStretch.begin() + (ptrdiff_t) marked[last] + 1, true);
        }

        first = last + 1;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::IsAboveBase
//
//  Whether `set` marks of `read` are more than a byte with no zero bits
//  after it has, by s_kMinimumGain and by s_kDeviations standard deviations.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::IsAboveBase (double set, double read, double base)
{
    double  margin = 0.0;



    if (read < s_kMinimumReads)
    {
        return false;
    }

    margin = std::max (s_kMinimumGain, s_kDeviations * std::sqrt (base * (1.0 - base) / read));

    return set / read > base + margin;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::IsSameStream
//
//  Two revolutions, the second the first started elsewhere, aligned with
//  bytes more or fewer allowed, but for one byte in kSameShare.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::IsSameStream (std::span<const Byte> a, std::span<const Byte> b)
{
    std::vector<Byte>    twice (b.begin(), b.end());
    std::vector<size_t>  anchors;
    std::vector<int>     map;
    std::vector<bool>    stretch;
    size_t               differ = 0;



    if (a.size() < kAlignBytes || b.size() < kAlignBytes || a.size() > b.size() + s_kBand || b.size() > a.size() + s_kBand)
    {
        return false;
    }

    twice.insert (twice.end(), b.begin(), b.end());

    for (size_t from : FindAnchors (a, twice))
    {
        if (Align (a, twice, from, map, differ, stretch) && differ * kSameShare <= a.size())
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::CountZeros
//
//  The marks of a byte with no zero bits after it are measured on the
//  track's bytes that aren't FF, as a controller's timing sets them:
//  a Disk II marks such a byte about one time in a hundred at 8 cycles and
//  a //c's IWM about one time in four. The runs of s_kShortestRun or more FF
//  are counted together, as a track's sync bytes are written at once and a
//  single run has too few marks: three zero bits when nearly all are set at
//  12 cycles, two when more are set at 12 than the baseline allows, one when
//  more are set at 8, and none otherwise. Runs with no read at 12 and more
//  set at 8 are runs of sync bytes, two. Any other byte is counted as 0,
//  as is an FF among an address field's values, which a volume or checksum
//  of $FF has: a byte the latch reads again before the next one starts is
//  read twice.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::CountZeros (const std::vector<Byte> & bytes, const Marks & marks, std::vector<uint8_t> & outZeros)
{
    size_t                                  length  = bytes.size();
    double                                  base[2] = {};
    double                                  set[2]  = {};
    double                                  read[2] = {};
    size_t                                  at      = 0;
    int                                     zeros   = 0;
    std::vector<std::pair<size_t, size_t>>  runs;
    std::vector<bool>                       values (length, false);



    outZeros.assign (length, 0);

    for (size_t each = 0; each + 2 < length; each++)
    {
        Byte  first  = bytes[each]     | 0x80;
        Byte  second = bytes[each + 1] | 0x80;
        Byte  third  = bytes[each + 2] | 0x80;
        bool  disk   = first == s_kProlog0     && second == s_kProlog1 && (third == s_kAddress16 || third == s_kAddress13);
        bool  rdos   = first == s_kRdosProlog0 && second == s_kProlog1 && third == s_kRdosAddress;

        if (disk || rdos)
        {
            std::fill (values.begin() + (ptrdiff_t) each + 3, values.begin() + (ptrdiff_t) std::min (length, each + 3 + s_kAddressValues), true);
        }
    }

    for (int delay = 0; delay < 2; delay++)
    {
        double  set  = 0.0;
        double  read = 0.0;

        for (size_t each = 0; each < length; each++)
        {
            if ((bytes[each] | 0x80) != s_kSyncByte)
            {
                set  += marks.set[delay][each];
                read += marks.read[delay][each];
            }
        }

        base[delay] = (set + 1.0) / (read + 2.0);
    }

    while (at < length)
    {
        size_t  end = at;

        while (end < length && (bytes[end] | 0x80) == s_kSyncByte && !values[end])
        {
            end++;
        }

        if (end - at >= s_kShortestRun)
        {
            runs.push_back ({ at, end });

            for (size_t each = at; each < end; each++)
            {
                for (int delay = 0; delay < 2; delay++)
                {
                    set[delay]  += marks.set[delay][each];
                    read[delay] += marks.read[delay][each];
                }
            }
        }

        at = std::max (end, at + 1);
    }

    if (read[1] > 0.0 && set[1] / read[1] >= s_kThreeZeroShare)
    {
        zeros = 3;
    }
    else if (IsAboveBase (set[1], read[1], base[1]))
    {
        zeros = 2;
    }
    else if (IsAboveBase (set[0], read[0], base[0]))
    {
        zeros = (read[1] > 0.0) ? 1 : 2;
    }

    for (const auto & [first, end] : runs)
    {
        std::fill (outZeros.begin() + (ptrdiff_t) first, outZeros.begin() + (ptrdiff_t) end, (uint8_t) zeros);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::ToBits
//
//  Each byte's eight bits, two zero bits after a sync byte, and a weak byte
//  as eight zero bits. Outside a weak byte, a run of more than s_kMaxZeros
//  zero bits is cut to s_kMaxZeros: a controller reads a longer run as
//  random bits, and a byte read at a write splice, such as C0, ends in six.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::ToBits (const Stream & stream, std::vector<uint8_t> & outBits, std::vector<size_t> & outStarts)
{
    size_t  zeros = 0;



    outBits.clear();
    outStarts.clear();

    for (size_t at = 0; at < stream.bytes.size(); at++)
    {
        Byte  value = stream.bytes[at] | 0x80;

        outStarts.push_back (outBits.size());

        if (stream.weak[at])
        {
            outBits.insert (outBits.end(), 8, (uint8_t) 0);
            zeros = 0;
            continue;
        }

        for (int bit = 0; bit < 8 + stream.zeros[at]; bit++)
        {
            uint8_t  one = bit >= 8 ? 0 : (uint8_t) ((value >> (7 - bit)) & 1);

            zeros = one != 0 ? 0 : zeros + 1;

            if (zeros <= s_kMaxZeros)
            {
                outBits.push_back (one);
            }
        }
    }
}