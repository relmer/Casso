#include "Pch.h"

#include "CassoLink/Session/CassoLinkCapture.h"
#include "CassoLink/Session/CaptureStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::AlignStream
//
//  The first read's revolution, from a stable start (FindReference), made the
//  consensus of every revolution of every read aligned to it, with each
//  alignment to that. False when the first read doesn't repeat.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::AlignStream (const Position & position, Stream & outStream, std::vector<Alignment> & outAlignments, bool & outUnstable)
{
    size_t             start    = 0;
    size_t             length   = 0;
    std::vector<Byte>  agreed;
    bool               repaired = false;



    outStream          = Stream();
    outStream.position = position.position;
    outUnstable        = false;

    outAlignments.clear();

    if (position.reads.empty() || !FindReference (position.reads[0], start, length))
    {
        return false;
    }

    outStream.start = start;
    outStream.bytes.assign (position.reads[0].begin() + (ptrdiff_t) start, position.reads[0].begin() + (ptrdiff_t) (start + length));

    AlignAll (position, outStream.bytes, outAlignments, outUnstable);
    MakeConsensus (position, outStream.bytes, outAlignments, agreed);

    //  Where most passes read a stretch otherwise than the first revolution
    //  did, the stream has theirs; a data field that doesn't decode is taken
    //  from a read that does (RepairFields); and everything is aligned to it
    //  again.
    repaired = RepairFields (position, agreed);
    repaired = RepairProlog (position, agreed, outStream.replaced) || repaired;

    if (repaired || agreed != outStream.bytes)
    {
        outStream.bytes = std::move (agreed);

        AlignAll (position, outStream.bytes, outAlignments, outUnstable);
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
//  The stream with each stretch most reads differ on replaced by the bytes
//  most of them read there, when more than half read the same. A read is
//  counted once however many of its revolutions align: both revolutions of
//  one read are framed alike, so a read that framed a sector otherwise than
//  two others did would tie with them by revolutions. The stretches are
//  FindDiffering's, and each alignment's version of one is TryGetVersion's.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkCapture::MakeConsensus (const Position & position, const std::vector<Byte> & revolution, const std::vector<Alignment> & alignments, std::vector<Byte> & outAgreed)
{
    std::vector<std::pair<size_t, size_t>>  stretches;
    size_t                                  copied = 0;



    outAgreed.clear();

    FindDiffering (position, revolution, alignments, stretches);

    for (const auto & [before, after] : stretches)
    {
        std::map<std::vector<Byte>, std::set<size_t>>  votes;
        std::set<size_t>                               voters;
        std::vector<Byte>                              mineBytes;
        const std::vector<Byte>                      * winner = nullptr;
        size_t                                         most   = 0;

        for (size_t at = before + 1; at < after; at++)
        {
            mineBytes.push_back (revolution[at] | 0x80);
        }

        for (const Alignment & alignment : alignments)
        {
            std::vector<Byte>  theirs;

            if (!TryGetVersion (position, revolution, alignment, before, after, theirs))
            {
                continue;
            }

            votes[theirs].insert (alignment.read);
            voters.insert (alignment.read);
        }

        for (const auto & [bytes, from] : votes)
        {
            if (from.size() > most)
            {
                most   = from.size();
                winner = &bytes;
            }
        }

        if (winner == nullptr || *winner == mineBytes || most * 2 <= voters.size())
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
//  CassoLinkCapture::FindAnchors
//
//  The places in a read a revolution may start, in order, each with room
//  for the whole revolution after it: where its first kAlignBytes bytes
//  are, and where the windows s_kWindowStep apart in it are, less their
//  offset, for a read that framed the first bytes differently; each within
//  s_kAnchorMismatch bytes. A window found elsewhere in the revolution too
//  is passed over. Places within s_kWindowDrift of one already found are
//  the same place. A place found by a later window is off by the bytes that
//  the stretches between read otherwise, so each is moved to where the
//  revolution's first bytes match exactly for longest, then with the fewest
//  of its first kAlignBytes differing, within s_kWindowDrift: a stretch
//  read a byte longer within those bytes would otherwise move the place a
//  byte past the revolution's start.
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

    for (size_t & anchor : anchors)
    {
        size_t  best    = anchor;
        size_t  longest = 0;
        size_t  fewest  = SIZE_MAX;

        for (size_t place = anchor > s_kWindowDrift ? anchor - s_kWindowDrift : 0; place <= anchor + s_kWindowDrift && place + revolution.size() + s_kBand <= read.size(); place++)
        {
            size_t  count = CountDiffer (revolution, 0, read, place, kAlignBytes);
            size_t  same  = 0;
            bool    wins  = false;

            while (same < kAlignBytes && (revolution[same] | 0x80) == (read[place + same] | 0x80))
            {
                same++;
            }

            wins    = same > longest || (same == longest && count < fewest);
            best    = wins ? place : best;
            longest = wins ? same  : longest;
            fewest  = wins ? count : fewest;
        }

        anchor = best;
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
//  s_kSpliceBytes alike between them, is marked in outStretch. So is a
//  shorter stretch that starts in the first s_kEndingBytes of an address
//  field's ending, its bytes the read has otherwise. A write starts after DOS has read those bytes, so no
//  splice falls there, and a short stretch there is weak bits: a protection
//  reads a nibble there twice and checks that the values differ. False when more than one byte in s_kAlignShare differs.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::Align (std::span<const Byte> revolution, std::span<const Byte> read, size_t from,
                              std::vector<int> & outMap, size_t & outDiffer, std::vector<bool> & outStretch)
{
    const int              band   = (int) s_kBand;
    const int              width  = 2 * band + 1;
    size_t                 n      = revolution.size();
    size_t                 m      = std::min (n + s_kBand, read.size() - from);
    std::vector<int>       costs  (2 * (size_t) width, INT_MAX / 2);   // the row before and this row, each offset by the band
    std::vector<Byte>      steps  ((n + 1) * (size_t) width, 0);
    std::vector<size_t>    marked;
    std::vector<bool>      endings;
    std::vector<bool>      misread;
    const Byte           * mine   = revolution.data();
    const Byte           * theirs = read.data() + from;
    Byte                 * step   = steps.data();
    int                  * before = costs.data();
    int                  * now    = before + width;
    int                    bestJ  = -1;
    int                    best   = INT_MAX;
    size_t                 i      = n;
    int                    j      = 0;



    outMap.assign (n, -1);
    outStretch.assign (n, false);
    misread.assign (n, false);
    outDiffer = 0;

    //  The table is indexed by offset, a column's place in its row's band: the
    //  diagonal cell is at the same offset in the row before, the cell above
    //  one further, and the cell to the left one back in the same row. Raw
    //  pointers mean the inner loop doesn't call a function in a Debug build.
    for (int column = 0; column <= band && column <= (int) m; column++)
    {
        before[column + band] = column;
        step[column + band]   = s_kStepMore;
    }

    for (size_t row = 1; row <= n; row++)
    {
        int     first   = (int) row - band > 0 ? (int) row - band : 0;
        int     last    = (int) row + band < (int) m ? (int) row + band : (int) m;
        Byte    value   = (Byte) (mine[row - 1] | 0x80);
        Byte  * stepRow = step + row * (size_t) width;

        std::fill (now, now + width, INT_MAX / 2);

        for (int column = first; column <= last; column++)
        {
            int   offset = column - (int) row + band;
            int   here   = INT_MAX / 2;
            Byte  how    = 0;

            if (column > 0)
            {
                here = before[offset] + (value == (Byte) (theirs[column - 1] | 0x80) ? 0 : 1);
                how  = s_kStepPair;
            }

            if (offset + 1 < width && before[offset + 1] + 1 < here)
            {
                here = before[offset + 1] + 1;
                how  = s_kStepFewer;
            }

            if (column > 0 && offset > 0 && now[offset - 1] + 1 < here)
            {
                here = now[offset - 1] + 1;
                how  = s_kStepMore;
            }

            now[offset]     = here;
            stepRow[offset] = how;
        }

        std::swap (before, now);
    }

    for (int column = std::max (0, (int) n - band); column <= std::min ((int) m, (int) n + band); column++)
    {
        int  offset = column - (int) n + band;

        if (before[offset] < best)
        {
            best  = before[offset];
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
        Byte  how = step[i * (size_t) width + (size_t) (j - (int) i + band)];

        if (how == s_kStepPair)
        {
            outMap[i - 1] = (int) from + j - 1;

            if ((revolution[i - 1] | 0x80) != (read[from + (size_t) j - 1] | 0x80))
            {
                marked.push_back (i - 1);
                misread[i - 1] = true;
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

    if (!marked.empty())
    {
        endings = FindAddressEndings (revolution);
    }

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
        else if (endings[marked[first]])
        {
            //  Only the bytes read otherwise: a byte the read has one more or
            //  one fewer of beside them is framing, and the bytes between
            //  them read the same.
            for (size_t at = first; at <= last; at++)
            {
                outStretch[marked[at]] = misread[marked[at]];
            }
        }

        first = last + 1;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::FindAddressEndings
//
//  For each byte of a revolution, whether it is one of the s_kEndingBytes
//  after an address field's checksum. An address field is a byte with
//  $D5's bits set, other than a sync byte, and two more bytes, then
//  s_kAddressNibbles 4-and-4 nibbles, not all sync bytes. A protected disk
//  changes the prolog, as Ultima IV's D7 AA 96 does, and its RWTS masks the
//  first byte with $D5, so the other two aren't checked. The
//  revolution wraps, so a field at its end has its ending at its start.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<bool> CassoLinkCapture::FindAddressEndings (std::span<const Byte> revolution)
{
    size_t             n        = revolution.size();
    size_t             field    = 3 + s_kAddressNibbles;
    std::vector<bool>  endings  (n, false);



    for (size_t at = 0; n > field + s_kEndingBytes && at < n; at++)
    {
        bool  isField = IsAddressFieldAt (revolution, at);

        for (size_t k = 0; isField && k < s_kEndingBytes; k++)
        {
            endings[(at + field + k) % n] = true;
        }

        //  An ending other than DE AA isn't one DOS wrote, so a write splice
        //  doesn't follow it: the bytes up to the next sync byte are the
        //  protection's.
        if (isField && ((revolution[(at + field) % n] | 0x80) != s_kEpilog0 || (revolution[(at + field + 1) % n] | 0x80) != s_kEpilog1))
        {
            for (size_t k = s_kEndingBytes; k < s_kSpliceBytes && (revolution[(at + field + k) % n] | 0x80) != s_kSyncByte; k++)
            {
                endings[(at + field + k) % n] = true;
            }
        }
    }

    return endings;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::FindAddressBodies
//
//  For each byte of a revolution, whether it is in an address field
//  (IsAddressPrologAt), from the prolog's first byte through the checksum's
//  second. A drive writes a
//  field's bytes back to back, so no zero bits follow any of them; zero bits
//  counted there come from a //c marking bytes by chance, and they make a
//  reader catch the byte before twice.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<bool> CassoLinkCapture::FindAddressBodies (std::span<const Byte> revolution)
{
    size_t             n      = revolution.size();
    size_t             field  = 3 + s_kAddressNibbles;
    std::vector<bool>  bodies (n, false);



    for (size_t at = 0; n > field && at < n; at++)
    {
        bool  isField = IsAddressPrologAt (revolution, at);

        for (size_t k = 0; isField && k < field; k++)
        {
            bodies[(at + k) % n] = true;
        }
    }

    return bodies;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::IsAddressFieldAt
//
//  Whether an address field starts at `at`: a byte with $D5's bits set,
//  other than a sync byte, two more bytes, then s_kAddressNibbles 4-and-4
//  nibbles, not all sync bytes.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::IsAddressFieldAt (std::span<const Byte> revolution, size_t at)
{
    size_t  n       = revolution.size();
    size_t  field   = 3 + s_kAddressNibbles;
    Byte    first   = (Byte) (revolution[at] | 0x80);
    bool    isField = n > field && (first & 0xD5) == 0xD5 && first != s_kSyncByte;
    bool    isSync  = true;



    for (size_t k = 3; isField && k < field; k++)
    {
        Byte  nibble = (Byte) (revolution[(at + k) % n] | 0x80);

        isField = (nibble & 0xAA) == 0xAA;
        isSync  = isSync && nibble == s_kSyncByte;
    }

    return isField && !isSync;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCapture::IsAddressPrologAt
//
//  Whether an address field starts at `at` (IsAddressFieldAt) whose prolog's
//  second byte is AA and whose third has $96's bits set, as in D5 AA 96 and
//  Ultima IV's D7 AA 96. A data field's last nibbles, its epilog and the
//  sync bytes after it can pass IsAddressFieldAt; they don't pass this.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkCapture::IsAddressPrologAt (std::span<const Byte> revolution, size_t at)
{
    size_t  n      = revolution.size();
    Byte    second = (Byte) (revolution[(at + 1) % n] | 0x80);
    Byte    third  = (Byte) (revolution[(at + 2) % n] | 0x80);



    return second == s_kProlog1 && (third & s_kAddress16) == s_kAddress16 && IsAddressFieldAt (revolution, at);
}
