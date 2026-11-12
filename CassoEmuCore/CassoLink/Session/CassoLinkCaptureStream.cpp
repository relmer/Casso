#include "Pch.h"

#include "CassoLink/Session/CassoLinkCapture.h"
#include "CassoLink/Session/CaptureStream.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





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
    size_t                  start      = 0;
    size_t                  length     = 0;
    std::vector<Alignment>  alignments;
    std::vector<Byte>       agreed;
    std::vector<int>        differs;
    bool                    repaired   = false;



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
    //  did, the stream has theirs; a data field that doesn't decode is taken
    //  from a read that does (RepairFields); and everything is aligned to it
    //  again.
    repaired = RepairFields (position, agreed);

    if (repaired || agreed != outStream.bytes)
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
//  as eight zero bits. A controller reads a run of more than s_kMaxZeros
//  zero bits as random bits. A byte read at a write splice, such as C0,
//  ends in six: the zero bit of its eight after s_kMaxZeros in a row is
//  written as a one, so the splice is as long as on the disk and every byte after
//  it on the track is the same time from the bytes before it as on the
//  disk. A sync byte's zero bits past s_kMaxZeros are left out.
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

            bool     inByte = bit < 8;

            zeros = one != 0 ? 0 : zeros + 1;
            one   = (inByte && zeros > s_kMaxZeros) ? 1 : one;
            zeros = one != 0 ? 0 : zeros;

            if (zeros <= s_kMaxZeros)
            {
                outBits.push_back (one);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::FindDataField
//
//  The first nibble of the data field after the 16-sector address field at
//  `at`: the bytes after the first prolog byte within s_kDataWithin of the
//  address field's end, marks ignored. SIZE_MAX when `at` isn't an address
//  field or the data field doesn't fit.
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoLinkCapture::FindDataField (std::span<const Byte> bytes, size_t at)
{
    size_t  data    = SIZE_MAX;
    bool    address = at + s_kMarkerBytes <= bytes.size() && (bytes[at] | 0x80) == s_kProlog0 && (bytes[at + 1] | 0x80) == s_kProlog1 && (bytes[at + 2] | 0x80) == s_kAddress16;



    for (size_t each = at + s_kMarkerBytes; address && data == SIZE_MAX && each < at + s_kMarkerBytes + s_kDataWithin; each++)
    {
        bool  fits = each + 3 + NibblizationLayer::kDataNibbleCount <= bytes.size();

        data = (fits && (bytes[each] | 0x80) == s_kProlog0) ? each + 3 : data;
    }

    return data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::RepairFields
//
//  Each 16-sector data field of the revolution whose nibbles don't decode,
//  taken from a read whose copy of the field does: the read's same address
//  field, all s_kMarkerBytes of it, and the data field after it. A field
//  across the revolution's end continues at its start. A revolution aligned
//  to a read can slip by a few bytes in a run of repeated values and put
//  another place's byte in the consensus. True when a field is taken from a
//  read.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::RepairFields (const Position & position, std::vector<Byte> & ioRevolution)
{
    std::vector<Byte>  sector;
    std::vector<Byte>  nibbles (NibblizationLayer::kDataNibbleCount);
    std::vector<Byte>  around  (ioRevolution);
    size_t             length   = ioRevolution.size();
    bool               repaired = false;



    auto  decodes = [&sector, &nibbles] (std::span<const Byte> bytes, size_t data)
    {
        std::transform (bytes.begin() + (ptrdiff_t) data, bytes.begin() + (ptrdiff_t) (data + nibbles.size()), nibbles.begin(), [] (Byte value) { return (Byte) (value | 0x80); });

        return NibblizationLayer::DecodeDataNibbles (nibbles.data(), sector);
    };

    around.insert (around.end(), ioRevolution.begin(), ioRevolution.begin() + (ptrdiff_t) std::min (length, s_kMarkerBytes + s_kDataWithin + 3 + nibbles.size()));

    for (size_t at = 0; at < length; at++)
    {
        size_t  data  = FindDataField (around, at);
        bool    fixed = data == SIZE_MAX || decodes (around, data);

        for (size_t read = 0; !fixed && read < position.reads.size(); read++)
        {
            const std::vector<Byte>  & bytes = position.reads[read];

            for (size_t other = 0; !fixed && other + s_kMarkerBytes <= bytes.size(); other++)
            {
                bool    same   = std::equal (around.begin() + (ptrdiff_t) at, around.begin() + (ptrdiff_t) (at + s_kMarkerBytes), bytes.begin() + (ptrdiff_t) other,
                                             [] (Byte mine, Byte read) { return (mine | 0x80) == (read | 0x80); });
                size_t  theirs = same ? FindDataField (bytes, other) : SIZE_MAX;

                if (theirs != SIZE_MAX && decodes (bytes, theirs))
                {
                    for (size_t each = 0; each < nibbles.size(); each++)
                    {
                        ioRevolution[(data + each) % length] = bytes[theirs + each];
                        around[(data + each) % length]       = bytes[theirs + each];
                        around[data + each]                  = bytes[theirs + each];
                    }

                    fixed    = true;
                    repaired = true;
                }
            }
        }
    }

    return repaired;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::UniteSyncZeros
//
//  The zero bits after a sync byte, one or two, the same on every track:
//  the count most tracks' marks give. A disk's sync bytes are written by one
//  formatter, but a //c's IWM marks a byte followed by one zero bit and one
//  followed by two nearly alike, so a few tracks of each capture come out
//  with the other count, and each such sync byte moves every byte after it
//  on its track by a bit. The bits are made again.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::UniteSyncZeros (std::map<int, Stream> & ioStreams)
{
    int      votes[3] = {};
    uint8_t  zeros    = 2;



    for (const auto & [position, stream] : ioStreams)
    {
        size_t  counts[3] = {};

        for (size_t at = 0; at < stream.bytes.size(); at++)
        {
            bool  sync = (stream.bytes[at] | 0x80) == s_kSyncByte && !stream.weak[at] && stream.zeros[at] >= 1 && stream.zeros[at] <= 2;

            counts[sync ? stream.zeros[at] : 0]++;
        }

        if (counts[1] + counts[2] > 0)
        {
            votes[counts[1] > counts[2] ? 1 : 2]++;
        }
    }

    zeros = votes[1] > votes[2] ? 1 : 2;

    for (auto & [position, stream] : ioStreams)
    {
        for (size_t at = 0; at < stream.bytes.size(); at++)
        {
            bool  sync = (stream.bytes[at] | 0x80) == s_kSyncByte && !stream.weak[at] && stream.zeros[at] >= 1 && stream.zeros[at] <= 2;

            stream.zeros[at] = sync ? zeros : stream.zeros[at];
        }

        ToBits (stream, stream.bits, stream.starts);
    }
}