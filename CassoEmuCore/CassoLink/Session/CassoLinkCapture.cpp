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
    std::map<int, Stream>  wholes;     // each whole track's stream, made once for pass 2
    int                    last      = std::clamp (lastTrack, 0, kTracks - 1) * 4;
    Run                    run       { host, drive, module, progress, outResult };
    auto                   whole     = [&positions, &wholes] (int position) -> const Stream &
    {
        auto  found    = wholes.find (position);
        bool  unstable = false;

        if (found == wholes.end())
        {
            found = wholes.emplace (position, Stream()).first;
            MakeStream (positions[(size_t) position / 4], found->second, unstable);
        }

        return found->second;
    };



    outResult   = Result();
    run.started = GetTickCount64();
    run.planned = (last / 4 + 1) * kWholeReads;

    for (int position = 0; position <= last; position += 4)
    {
        Position  read;
        Stream    previous;
        bool      unstable = false;

        //  Each whole track after the first waits on a marker on the one
        //  before, so the tracks can be placed relative to each other. The
        //  first waits on a marker on itself, so the drive's revolution can
        //  be timed (TimeRevolution).
        if (positions.empty())
        {
            read.syncFrom = position;
        }
        else if (MakeStream (positions.back(), previous, unstable))
        {
            FindMarker (previous.bytes, read.syncMarker);
            read.syncFrom = read.syncMarker.empty() ? -1 : positions.back().position;
            wholes.emplace (positions.back().position, std::move (previous));
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

        //  A position with a separate stream is read again.
        if (MakeStream (read, stream, unstable) && !IsSameStream (stream.bytes, whole (lower).bytes) && !IsSameStream (stream.bytes, whole (upper).bytes))
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
        put (position.writeProtected ? 1 : 0, 1);

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
    valid   = valid && version >= 1 && version <= s_kReadsVersion;
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

        if (version >= 3)
        {
            position.writeProtected = get (1) != 0;
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

    run.writeProtected = run.writeProtected || status.done == CassoLinkHost::kCaptureProtected;

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
        //  A capture's first read takes the arm to track 0 first, as the
        //  program's record of its place can be wrong.
        std::vector<Byte>  bytes;
        Byte               delay   = kWholeDelays[read % kWholeReads];
        Byte               request = (Byte) (delay | ((position == 0 && read == 0) ? CassoLinkHost::kRecalibrate : 0));
        bool               self    = outPosition.syncFrom == position;
        bool               sync    = outPosition.syncFrom >= 0 && read == (self ? 1 : 0) && !outPosition.syncMarker.empty();
        bool               missed  = false;

        hr = ReadOnce (run, position, request, read + 1, reads, sync ? (Byte) outPosition.syncFrom : CassoLinkHost::kNoSync,
                       sync ? std::span<const Byte> (outPosition.syncMarker) : std::span<const Byte>(), bytes, missed);
        CHR (hr);

        if (missed)
        {
            outPosition.syncFrom = -1;

            hr = ReadOnce (run, position, delay, read + 1, reads, CassoLinkHost::kNoSync, {}, bytes, missed);
            CHR (hr);
        }

        //  For a track that waits on a marker on itself, the marker is taken
        //  from its first read.
        if (self && read == 0)
        {
            size_t             start  = 0;
            size_t             length = 0;
            std::vector<Byte>  revolution;

            if (FindReference (bytes, start, length))
            {
                revolution.assign (bytes.begin() + (ptrdiff_t) start, bytes.begin() + (ptrdiff_t) (start + length));
                FindGapMarker (revolution, outPosition.syncMarker);
            }

            outPosition.syncFrom = outPosition.syncMarker.empty() ? -1 : position;
        }

        outPosition.reads.push_back (std::move (bytes));
        outPosition.delays.push_back (delay);
        outPosition.writeProtected = run.writeProtected;
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
//  field must also decode, and its prolog can be D5 BB, as Archon's. A field across the revolution's end is found in
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
            bool  standard = bytes[data + 1] == s_kProlog1 && bytes[data + 2] == s_kData;
            bool  other    = is16 && bytes[data + 1] == s_kOtherProlog1;

            hasData = bytes[data] == s_kProlog0 && (standard || other);
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
//  CassoLinkCapture::TimeRevolution
//
//  The cycles a revolution of the drive takes, from the track whose second
//  read waited on a marker on that track: the read started s_kSelfSyncCycles
//  after the marker passed, and the bits from the read's first byte to the
//  marker passing again took s_kBitCycles each. The marker is the address
//  field before the track's gap (FindGapMarker), so the gap passes during
//  the delay and every bit counted is data: a gap's length in the stream
//  isn't its time on the disk. 0 when no track waited on itself or the
//  result is more than s_kRevolutionSpread from the track's bits.
//
////////////////////////////////////////////////////////////////////////////////

double CassoLinkCapture::TimeRevolution (const std::vector<Position> & positions, const std::map<int, Stream> & streams)
{
    double  cycles = 0.0;



    for (const Position & position : positions)
    {
        auto                         mine = streams.find (position.position);
        std::vector<Byte>            bytes;
        std::vector<Byte>            read;
        std::vector<Byte>::iterator  inStream;
        std::vector<Byte>::iterator  inRead;

        if (cycles > 0.0 || position.syncFrom != position.position || position.reads.size() < 2 || position.syncMarker.empty() || mine == streams.end())
        {
            continue;
        }

        for (Byte value : mine->second.bytes)
        {
            bytes.push_back (value | 0x80);
        }

        for (Byte value : position.reads[1])
        {
            read.push_back (value | 0x80);
        }

        inStream = std::search (bytes.begin(), bytes.end(), position.syncMarker.begin(), position.syncMarker.end());
        inRead   = std::search (read.begin(),  read.end(),  position.syncMarker.begin(), position.syncMarker.end());

        if (inStream == bytes.end() || inRead == read.end())
        {
            continue;
        }

        const Stream  & stream   = mine->second;
        size_t          length   = stream.bytes.size();
        size_t          end      = ((size_t) (inStream - bytes.begin()) + position.syncMarker.size()) % length;
        size_t          before   = ((size_t) (inRead - read.begin()) + position.syncMarker.size()) % length;
        size_t          first    = (end + length - before) % length;
        double          bits     = (double) stream.bits.size();
        double          ahead    = (double) GetBitOffset (stream, end) - (double) GetBitOffset (stream, first);
        double          expected = s_kBitCycles * bits;
        double          measured = 0.0;

        ahead    = ahead <= 0.0 ? ahead + bits : ahead;
        measured = s_kSelfSyncCycles + s_kBitCycles * ahead;

        if (std::abs (measured - expected) <= s_kRevolutionSpread * expected)
        {
            cycles = measured;
        }
    }

    return cycles;
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
//  drive's measured cycles (TimeRevolution), or the track's bits at
//  s_kBitCycles each when it wasn't measured, and the stream's first byte
//  the bits of the read's bytes before it after that. A track without one starts at angle 0, and the
//  tracks aren't synchronized.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::PlaceTracks (const std::vector<Position> & positions, const std::map<int, Stream> & streams, double revolution, std::map<int, size_t> & outRotations, bool & outSynchronized)
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

        if (position.syncFrom >= 0 && position.syncFrom != position.position && previous != streams.end() && placed != angles.end())
        {
            const Stream                 & before = previous->second;
            const Stream                 & stream = mine->second;
            std::vector<Byte>              bytes;
            std::vector<Byte>::iterator    found;
            double                         marker = 0.0;
            double                         bits   = (double) stream.bits.size();
            double                         cycles = revolution > 0.0 ? revolution : s_kBitCycles * bits;
            size_t                         lead   = (stream.bytes.size() - stream.start % std::max<size_t> (stream.bytes.size(), 1)) % std::max<size_t> (stream.bytes.size(), 1);

            for (Byte value : before.bytes)
            {
                bytes.push_back (value | 0x80);
            }

            found = std::search (bytes.begin(), bytes.end(), position.syncMarker.begin(), position.syncMarker.end());

            if (found != bytes.end())
            {
                size_t  last = (size_t) (found - bytes.begin()) + position.syncMarker.size();

                marker = placed->second + (double) (GetBitOffset (before, last - 1) + 8) / (double) before.bits.size();
                angle  = marker + s_kSyncCycles / cycles + (bits - (double) GetBitOffset (stream, lead)) / bits;
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

    UniteSyncZeros (streams);

    outResult.revolutionCycles = TimeRevolution (positions, streams);

    for (auto & [position, stream] : streams)
    {
        FitGap (stream, outResult.revolutionCycles / s_kBitCycles);
    }

    PlaceTracks (positions, streams, outResult.revolutionCycles, rotations, outResult.synchronized);

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

    //  The INFO chunk's synchronized and write-protected flags, which
    //  Serialize writes as 0, and the header's CRC of everything after the
    //  header again. A program that writes to the disk it runs from takes
    //  another path when the write fails, as Wizardry I's does after its title.
    outResult.writeProtected = std::any_of (positions.begin(), positions.end(), [] (const Position & position) { return position.writeProtected; });

    if (outResult.woz.size() > s_kInfoSynchronized)
    {
        outResult.woz[s_kInfoSynchronized] = outResult.synchronized   ? 1 : 0;
        outResult.woz[s_kInfoProtected]    = outResult.writeProtected ? 1 : 0;
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
