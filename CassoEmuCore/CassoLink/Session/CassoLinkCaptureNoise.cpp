#include "Pch.h"

#include "CassoLink/Session/CassoLinkCapture.h"
#include "CassoLink/Session/CaptureStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::AlignNoisy
//
//  A track with a stretch that has no flux, which the drive reads as noise:
//  a different count of bytes on each pass, so a revolution's bytes don't
//  repeat at one length and the bytes after the stretch are framed anew.
//  The revolution is the first read's from a start in the bytes that do
//  repeat (FindNoisyStart), and every revolution of every read is aligned
//  to it by the windows of kAlignBytes found once in it (AnchorRead), so a
//  stretch of any length between two of them is a stretch that differs. A
//  read with no whole revolution aligned makes the stream unstable. False
//  when the first read doesn't repeat.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::AlignNoisy (const Position & position, Stream & outStream, std::vector<Alignment> & outAlignments, bool & outUnstable)
{
    size_t                                start  = 0;
    size_t                                length = 0;
    std::unordered_map<uint64_t, size_t>  places;   // each window's place in the revolution, SIZE_MAX for one found twice
    std::vector<Byte>                     ring;     // the revolution, marks ignored, and its first bytes again
    bool                                  found  = !position.reads.empty() && FindNoisyStart (position.reads[0], start, length);



    outStream          = Stream();
    outStream.position = position.position;
    outUnstable        = false;

    outAlignments.clear();

    if (!found)
    {
        return false;
    }

    outStream.start = start;
    outStream.bytes.assign (position.reads[0].begin() + (ptrdiff_t) start, position.reads[0].begin() + (ptrdiff_t) (start + length));

    for (size_t at = 0; at < length + kAlignBytes - 1; at++)
    {
        ring.push_back (outStream.bytes[at % length] | 0x80);
    }

    for (size_t at = 0; at < length; at++)
    {
        uint64_t  hash  = HashWindow (ring, at);
        auto      known = places.find (hash);

        if (known == places.end())
        {
            places.emplace (hash, at);
        }
        else
        {
            known->second = SIZE_MAX;
        }
    }

    for (size_t read = 0; read < position.reads.size(); read++)
    {
        size_t  before = outAlignments.size();

        AnchorRead (ring, places, read, position.reads[read], outAlignments);

        outUnstable = outUnstable || outAlignments.size() == before;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::MarkRandom
//
//  Each stretch most reads differ on (FindDiffering) that every revolution
//  read otherwise, from s_kRandomPasses revolutions or more, made weak: a
//  few bits with no flux, which the drive reads as random bits. The bytes
//  a write splice frames differently are framed one of a few ways, so some
//  revolutions read them alike.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::MarkRandom (const Position & position, const std::vector<Byte> & revolution, const std::vector<Alignment> & alignments, std::vector<bool> & ioWeak)
{
    std::vector<std::pair<size_t, size_t>>  stretches;



    FindDiffering (position, revolution, alignments, stretches);

    for (const auto & [before, after] : stretches)
    {
        std::set<std::vector<Byte>>  versions;
        size_t                       voters = 0;

        for (const Alignment & alignment : alignments)
        {
            std::vector<Byte>  theirs;

            if (TryGetVersion (position, revolution, alignment, before, after, theirs))
            {
                versions.insert (std::move (theirs));
                voters++;
            }
        }

        if (voters >= s_kRandomPasses && versions.size() == voters)
        {
            std::fill (ioWeak.begin() + (ptrdiff_t) before + 1, ioWeak.begin() + (ptrdiff_t) after, true);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::TimeZeros
//
//  The zero bits after each byte of a stream from the moments the reads
//  caught their bytes. The capture loop catches a byte up to a poll's
//  s_kPollCycles after it is whole, and marks it when the latch has it at
//  the second read too, 8 or 12 cycles later; a byte stays in the
//  latch s_kHoldCycles and s_kBitCycles more for each zero bit after it. The
//  next poll is a fixed number of cycles after the catch, so the zero bits
//  after a byte move the moment its next byte is caught by a known amount.
//  Each revolution's possible moments are followed byte by byte, and each
//  byte is given the zero bits that fit the most revolutions' marks, looking
//  one byte ahead. -1 for a byte that fits too few revolutions.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::TimeZeros (const Position & position, size_t length, const std::vector<Alignment> & alignments, std::vector<int> & outZeros)
{
    std::vector<uint8_t>  phases (alignments.size(), s_kAllPhases);
    int                   weight = (int) alignments.size() + 1;
    auto                  delay  = [&] (const Alignment & alignment) { return s_kWaitCycles[(alignment.read < position.delays.size() && position.delays[alignment.read] == 1) ? 1 : 0]; };
    auto                  marked = [&] (const Alignment & alignment, size_t at) { return CaptureStream::IsSync (position.reads[alignment.read][(size_t) alignment.map[at]]); };
    auto                  fits   = [] (int phase, int zeros, int wait, bool mark) { return (phase + wait < s_kHoldCycles + s_kZeroCycles * zeros) == mark; };
    auto                  next   = [&] (const Alignment & alignment, size_t at, uint8_t from, int zeros) -> uint8_t
    {
        int      wait  = delay (alignment);
        bool     mark  = marked (alignment, at);
        int      gap   = wait + s_kLoopTail + (mark ? 1 : 0) - ((alignment.map[at] % s_kPageBytes == s_kPageBytes - 1) ? 1 : 0);
        uint8_t  to    = 0;

        for (int phase = 0; phase < s_kPollCycles; phase++)
        {
            int  moved = phase + gap - s_kByteCycles - s_kZeroCycles * zeros;

            if ((from & (1 << phase)) == 0 || !fits (phase, zeros, wait, mark))
            {
                continue;
            }

            while (moved < 0)
            {
                moved += s_kPollCycles;
            }

            to |= (uint8_t) (moved < s_kPollCycles ? (1 << moved) : 0);
        }

        return to;
    };
    auto                  count  = [&] (size_t at, int zeros, const std::vector<uint8_t> & from) -> int
    {
        int  count = 0;

        for (size_t each = 0; each < alignments.size(); each++)
        {
            const Alignment  & alignment = alignments[each];
            bool               mark      = alignment.map[at] >= 0 && marked (alignment, at);
            bool               any       = false;

            for (int phase = 0; alignment.map[at] >= 0 && phase < s_kPollCycles && !any; phase++)
            {
                any = (from[each] & (1 << phase)) != 0 && fits (phase, zeros, delay (alignment), mark);
            }

            count += any ? 1 : 0;
        }

        return count;
    };



    outZeros.assign (length, -1);

    for (size_t at = 0; at < length; at++)
    {
        int   best      = -1;
        int   bestScore = -1;
        int   present   = 0;
        bool  tied      = false;
        int   fit[s_kMaxSyncZeros + 1] = {};

        for (const Alignment & alignment : alignments)
        {
            present += alignment.map[at] >= 0 ? 1 : 0;
        }

        for (int zeros = 0; zeros <= s_kMaxSyncZeros; zeros++)
        {
            std::vector<uint8_t>  moved (alignments.size(), 0);
            int                   score = 0;
            int                   ahead = 0;

            fit[zeros] = count (at, zeros, phases);
            score      = fit[zeros] * weight;

            for (size_t each = 0; each < alignments.size(); each++)
            {
                moved[each] = alignments[each].map[at] >= 0 ? next (alignments[each], at, phases[each], zeros) : s_kAllPhases;
                moved[each] = moved[each] == 0 ? s_kAllPhases : moved[each];
            }

            for (int later = 0; at + 1 < length && later <= s_kMaxSyncZeros; later++)
            {
                ahead = std::max (ahead, count (at + 1, later, moved));
            }

            score += ahead;

            if (score > bestScore)
            {
                bestScore = score;
                best      = zeros;
            }
        }

        //  A count that fits more revolutions' marks than any other is taken;
        //  on a tie, as for revolutions whose moments aren't known yet, the
        //  moments of each count in the tie are followed.
        for (int zeros = 0; zeros <= s_kMaxSyncZeros; zeros++)
        {
            tied = tied || (zeros != best && fit[zeros] >= fit[best]);
        }

        outZeros[at] = (!tied && present > 0 && fit[best] * s_kFitOf >= present * s_kFitNeeded) ? best : -1;

        for (size_t each = 0; each < alignments.size(); each++)
        {
            const Alignment  & alignment = alignments[each];
            bool               broken    = alignment.map[at] < 0 || at + 1 >= length || alignment.map[at + 1] != alignment.map[at] + 1;
            uint8_t            moved     = 0;

            for (int zeros = 0; !broken && zeros <= s_kMaxSyncZeros; zeros++)
            {
                moved |= (zeros == best || (tied && fit[zeros] == fit[best])) ? next (alignment, at, phases[each], zeros) : 0;
            }

            phases[each] = moved == 0 ? s_kAllPhases : moved;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::FindNoisyStart
//
//  The first start, from the read's first byte framed as on the disk
//  (FindFramed) and kAlignBytes on, in steps of s_kStartStep, whose
//  kAlignBytes bytes have several values and repeat within
//  s_kAnchorMismatch bytes between s_kShortestNoisy and
//  s_kLongestRevolution later, with the kAlignBytes before it repeating at
//  the same length, and that are nowhere else in that length. The
//  revolution so starts and ends in bytes every pass reads alike.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::FindNoisyStart (std::span<const Byte> read, size_t & outStart, size_t & outLength)
{
    bool  found = false;



    outStart  = 0;
    outLength = 0;

    for (size_t start = FindFramed (read, read.size()) + kAlignBytes; !found && start + s_kLongestRevolution + kAlignBytes <= read.size(); start += s_kStartStep)
    {
        std::set<Byte>  values;
        size_t          length = 0;

        for (size_t at = 0; at < kAlignBytes; at++)
        {
            values.insert (read[start + at] | 0x80);
        }

        if (values.size() <= 1)
        {
            continue;
        }

        for (length = s_kShortestNoisy; length <= s_kLongestRevolution; length++)
        {
            if (CountDiffer (read, start, read, start + length, 0) == 0 &&
                CountDiffer (read, start - kAlignBytes, read, start + length - kAlignBytes, 0) == 0)
            {
                break;
            }
        }

        found     = length <= s_kLongestRevolution && IsUnique (read, start, length);
        outStart  = start;
        outLength = length;
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
//  CassoLinkCapture::AnchorRead
//
//  Each whole revolution of a read aligned to the revolution in `ring`: the
//  read's windows found in `places` put each byte of the read at its place
//  in the revolution, and a window more than half a revolution from the
//  place of the one before it begins another revolution (MapAnchored).
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::AnchorRead (std::span<const Byte> ring, const std::unordered_map<uint64_t, size_t> & places, size_t read,
                                   std::span<const Byte> bytes, std::vector<Alignment> & ioAlignments)
{
    ptrdiff_t                               length = (ptrdiff_t) (ring.size() + 1 - kAlignBytes);
    ptrdiff_t                               offset = 0;
    std::vector<std::pair<size_t, size_t>>  hits;     // a window's place in the read and in the revolution
    auto                                    flush  = [&] ()
    {
        Alignment  alignment;

        alignment.read = read;

        if (!hits.empty() && MapAnchored (ring, bytes, hits, alignment))
        {
            ioAlignments.push_back (std::move (alignment));
        }

        hits.clear();
    };



    for (size_t at = 0; at + kAlignBytes <= bytes.size(); at++)
    {
        auto       known = places.find (HashWindow (bytes, at));
        ptrdiff_t  here  = 0;

        if (known == places.end() || known->second == SIZE_MAX || CountDiffer (ring, known->second, bytes, at, 0) > 0)
        {
            continue;
        }

        here = (ptrdiff_t) at - (ptrdiff_t) known->second;

        if (!hits.empty() && std::abs (here - offset) > length / 2)
        {
            flush();
        }

        offset = here;
        hits.push_back ({ at, known->second });
    }

    flush();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::MapAnchored
//
//  A revolution of a read aligned by its windows found in the revolution:
//  each window's bytes mapped to the read's, and each gap between them
//  mapped from both sides (MapGap). False for a part of a revolution, one
//  whose windows end more than kAlignBytes from the revolution's start or
//  end.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::MapAnchored (std::span<const Byte> ring, std::span<const Byte> bytes, const std::vector<std::pair<size_t, size_t>> & hits, Alignment & outAlignment)
{
    size_t  length = ring.size() + 1 - kAlignBytes;
    size_t  first  = SIZE_MAX;
    size_t  last   = 0;



    outAlignment.map.assign (length, -1);
    outAlignment.stretch.assign (length, false);
    outAlignment.from   = hits.front().first - std::min (hits.front().first, hits.front().second);
    outAlignment.differ = 0;

    for (const auto & [at, place] : hits)
    {
        for (size_t each = 0; each < kAlignBytes && place + each < length; each++)
        {
            outAlignment.map[place + each] = (int) (at + each);
        }

        first = std::min (first, place);
        last  = std::max (last, std::min (place + kAlignBytes, length) - 1);
    }

    for (size_t gap = first; gap <= last; gap++)
    {
        size_t  end = gap;

        if (outAlignment.map[gap] >= 0)
        {
            continue;
        }

        while (end <= last && outAlignment.map[end] < 0)
        {
            end++;
        }

        MapGap (ring, bytes, gap, end, outAlignment);

        gap = end;
    }

    for (size_t at = 0; at < length; at++)
    {
        int   mine = outAlignment.map[at];
        bool  odd  = outAlignment.stretch[at] || (mine >= 0 && (bytes[(size_t) mine] | 0x80) != ring[at]);

        outAlignment.differ += odd ? 1 : 0;
    }

    return first < kAlignBytes && last + kAlignBytes >= length;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::MapGap
//
//  The revolution's bytes from `first` to `end`, which no window of the read
//  matched, between two that did. From each side, each byte the read has the
//  same is mapped. A gap with as many bytes in the read as in the
//  revolution is mapped byte for byte; one with more or fewer has its
//  middle unmapped. Either way, a middle longer than s_kSpliceBytes is a
//  stretch that differs.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::MapGap (std::span<const Byte> ring, std::span<const Byte> bytes, size_t first, size_t end, Alignment & ioAlignment)
{
    std::vector<int>  & map   = ioAlignment.map;
    int                 low   = map[first - 1];
    int                 high  = map[end];
    bool                alike = high - low == (int) (end - first) + 1;
    size_t              from  = first;
    size_t              to    = end;
    size_t              split = 0;



    while (from < to && low + (int) (from - first) + 1 < high && (bytes[(size_t) (low + (int) (from - first) + 1)] | 0x80) == ring[from])
    {
        map[from] = low + (int) (from - first) + 1;
        from++;
    }

    while (to > from && high - (int) (end - to) - 1 > map[from - 1] && (bytes[(size_t) (high - (int) (end - to) - 1)] | 0x80) == ring[to - 1])
    {
        map[to - 1] = high - (int) (end - to) - 1;
        to--;
    }

    for (size_t at = from; alike && at < to; at++)
    {
        map[at] = low + (int) (at - first) + 1;
    }

    //  The bytes left that the read has as the same bits, framed otherwise,
    //  as a run of one value the latch frames from wherever the noise before
    //  it ends, are mapped byte for byte from the side they match, and aren't
    //  a stretch that differs.
    if (!alike && to > from)
    {
        int                    before = map[from - 1];
        int                    after  = to < end ? map[to] : high;
        std::span<const Byte>  mine   = ring.subspan (from, to - from);
        std::span<const Byte>  theirs = after > before + 1 ? bytes.subspan ((size_t) before + 1, (size_t) (after - before - 1)) : std::span<const Byte>();
        size_t                 tail   = CountSameBits (mine, theirs, true) / 8;
        size_t                 head   = CountSameBits (mine.first (mine.size() - tail), theirs, false) / 8;

        for (size_t each = 0; each < tail && after - 1 - (int) each > before; each++)
        {
            map[to - 1 - each] = after - 1 - (int) each;
        }

        for (size_t each = 0; each < head && before + 1 + (int) each < after; each++)
        {
            map[from + each] = before + 1 + (int) each;
        }

        to   -= tail;
        from += head;
    }

    //  Bytes found only once in what is left of the gap, in the revolution
    //  and in the read, split it in two, as data between two stretches of
    //  noise can be like data elsewhere on the track.
    if (!alike && to > from + 2 * kAlignBytes && TryMapInside (ring, bytes, from, to, to < end ? map[to] : high, ioAlignment, split))
    {
        if (split > from)
        {
            MapGap (ring, bytes, from, split, ioAlignment);
        }

        if (to > split + kAlignBytes)
        {
            MapGap (ring, bytes, split + kAlignBytes, to, ioAlignment);
        }

        return;
    }

    if (to - from > s_kSpliceBytes)
    {
        std::fill (ioAlignment.stretch.begin() + (ptrdiff_t) from, ioAlignment.stretch.begin() + (ptrdiff_t) to, true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::TryMapInside
//
//  The first window of kAlignBytes, s_kInsideStep apart, in the revolution's
//  bytes from `from` to `to` that has two values or more and is once within
//  s_kInsideDrift bytes of itself there, and once in the read's bytes
//  between the one mapped before `from` and `after`, within s_kInsideDrift
//  bytes of its place counted from the nearer end: mapped, with outSplit its
//  place. A track can have the same data more than once, and noise is read
//  in a different count of bytes on each pass.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::TryMapInside (std::span<const Byte> ring, std::span<const Byte> bytes, size_t from, size_t to, int after, Alignment & ioAlignment, size_t & outSplit)
{
    int     before = ioAlignment.map[from - 1];
    bool    found  = false;



    outSplit = 0;

    for (size_t at = from; !found && at + kAlignBytes <= to; at += s_kInsideStep)
    {
        std::set<Byte>  values;
        size_t          mine     = 0;
        size_t          theirs   = 0;
        size_t          place    = 0;
        int             estimate = (at - from < to - at) ? before + 1 + (int) (at - from) : after - (int) (to - at);
        size_t          low      = std::max (from, at > s_kInsideDrift ? at - s_kInsideDrift : 0);
        size_t          high     = std::min (to, at + s_kInsideDrift + kAlignBytes);

        for (size_t each = 0; each < kAlignBytes; each++)
        {
            values.insert (ring[at + each]);
        }

        for (size_t other = low; values.size() > 1 && mine <= 1 && other + kAlignBytes <= high; other++)
        {
            mine += CountDiffer (ring, at, ring, other, 0) == 0 ? 1 : 0;
        }

        for (int other = std::max (before + 1, estimate - (int) s_kInsideDrift); values.size() > 1 && mine == 1 && theirs <= 1 && other + (int) kAlignBytes <= std::min (after, estimate + (int) (s_kInsideDrift + kAlignBytes)); other++)
        {
            bool  same = CountDiffer (ring, at, bytes, (size_t) other, 0) == 0;

            theirs += same ? 1 : 0;
            place   = same ? (size_t) other : place;
        }

        found = mine == 1 && theirs == 1;

        for (size_t each = 0; found && each < kAlignBytes; each++)
        {
            ioAlignment.map[at + each] = (int) (place + each);
        }

        outSplit = found ? at : 0;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::CountSameBits
//
//  The bits of `a`, eight to a byte, marks ignored, that `b` has the same
//  from their starts, or from their ends with fromEnd. A zero bit either one
//  has and the other doesn't is passed over when the s_kSkipLook bits after
//  it are the same, as a latch that frames a byte otherwise passes over the
//  zero bits before it.
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoLinkCapture::CountSameBits (std::span<const Byte> a, std::span<const Byte> b, bool fromEnd)
{
    size_t  aBits = a.size() * 8;
    size_t  bBits = b.size() * 8;
    size_t  i     = 0;
    size_t  j     = 0;
    auto    get   = [fromEnd] (std::span<const Byte> bytes, size_t bit) -> int
    {
        size_t  at = fromEnd ? bytes.size() * 8 - 1 - bit : bit;

        return ((bytes[at / 8] | 0x80) >> (7 - at % 8)) & 1;
    };
    auto    same  = [&] (size_t aAt, size_t bAt) -> bool
    {
        bool  alike = aAt + s_kSkipLook <= aBits && bAt + s_kSkipLook <= bBits;

        for (size_t each = 0; alike && each < s_kSkipLook; each++)
        {
            alike = get (a, aAt + each) == get (b, bAt + each);
        }

        return alike;
    };



    while (i < aBits && j < bBits)
    {
        int  aBit = get (a, i);
        int  bBit = get (b, j);

        if (aBit == bBit)
        {
            i++;
            j++;
        }
        else if (aBit == 0 && same (i + 1, j))
        {
            i++;
        }
        else if (bBit == 0 && same (i, j + 1))
        {
            j++;
        }
        else
        {
            break;
        }
    }

    return i;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::HashWindow
//
//  FNV-1a of the kAlignBytes bytes at `at`, marks ignored.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t CassoLinkCapture::HashWindow (std::span<const Byte> bytes, size_t at)
{
    uint64_t  hash = s_kFnvBasis;



    for (size_t each = 0; each < kAlignBytes; each++)
    {
        hash = (hash ^ (Byte) (bytes[at + each] | 0x80)) * s_kFnvPrime;
    }

    return hash;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::FitNoise
//
//  Each weak stretch of a track aligned as noisy made as long in bits as the
//  passes read it: the median of the bytes each alignment has between the
//  bytes on either side, at the bits a byte of noise takes. A byte of noise
//  is a one bit, seven bits at random, and the zero bits before the next one
//  bit, so with p the share of ones among the seven, it takes 8 + (1 - p) / p
//  bits on average. A byte the loop stored twice, as a byte followed by
//  more zero bits than a loop's length is, is counted once. The bits past a
//  weak byte's eight are its weakZeros.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::FitNoise (const Position & position, const std::vector<Alignment> & alignments, Stream & ioStream)
{
    size_t  length = ioStream.bytes.size();
    size_t  end    = 0;



    ioStream.weakZeros.assign (length, 0);

    for (size_t first = 1; first < length; first = std::max (end, first + 1))
    {
        std::vector<double>  counts;
        double               ones   = 0.0;
        double               bits   = 0.0;
        double               share  = 0.0;
        double               median = 0.0;
        size_t               count  = 0;
        size_t               extra  = 0;

        end = first;

        if (!ioStream.weak[first] || ioStream.weak[first - 1])
        {
            continue;
        }

        while (end < length && ioStream.weak[end])
        {
            end++;
        }

        for (const Alignment & alignment : alignments)
        {
            const std::vector<Byte>  & bytes = position.reads[alignment.read];
            int                        low   = alignment.map[first - 1];
            int                        high  = end < length ? alignment.map[end] : -1;

            if (low < 0 || high <= low)
            {
                continue;
            }

            counts.push_back (0.0);

            for (int at = low + 1; at < high; at++)
            {
                bool  again = at > low + 1 && CaptureStream::IsSync (bytes[(size_t) at - 1]) && (bytes[(size_t) at] | 0x80) == (bytes[(size_t) at - 1] | 0x80);

                if (again)
                {
                    continue;
                }

                counts.back() += 1.0;

                for (int bit = 0; bit < s_kNoiseBits; bit++)
                {
                    ones += (bytes[(size_t) at] >> bit) & 1;
                }

                bits += s_kNoiseBits;
            }
        }

        if (counts.empty() || ones == 0.0)
        {
            continue;
        }

        std::sort (counts.begin(), counts.end());

        median = counts[counts.size() / 2];
        share  = ones / bits;
        count = end - first;
        extra = (size_t) std::max (0.0, std::round (median * (8.0 + (1.0 - share) / share)) - 8.0 * (double) count);

        for (size_t at = 0; at < count; at++)
        {
            ioStream.weakZeros[first + at] = (uint8_t) std::min<size_t> (UINT8_MAX, extra / count + (at < extra % count ? 1 : 0));
        }
    }
}
