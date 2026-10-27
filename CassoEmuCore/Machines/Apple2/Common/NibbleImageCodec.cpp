#include "Pch.h"

#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleImageCodec::ResolveGeometry
//
//  Which track size a file of this length must hold.
//
//  THE LENGTH IS THE ONLY THING THAT CAN ANSWER. A nibble image has no header,
//  no signature and no checksum, and the two sizes in circulation are named
//  inconsistently in the wild -- images of 6,384-byte tracks are distributed
//  under the .nib name that convention reserves for 6,656. So the extension is
//  a hint about which loader to use and nothing more, and the geometry comes
//  from arithmetic on the file size.
//
//  ERROR_BAD_LENGTH rather than E_INVALIDARG: a wrong-sized file the user
//  named is edge input, and E_INVALIDARG marks a coding error in this tree and
//  always asserts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibbleImageCodec::ResolveGeometry (size_t imageByteSize, size_t & outTrackSize)
{
    HRESULT   hr          = S_OK;
    bool      recognized  = false;



    recognized = (imageByteSize == kNibImageSize) || (imageByteSize == kNb2ImageSize);

    CBREx (recognized, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH));

    outTrackSize = (imageByteSize == kNibImageSize) ? kNibTrackSize : kNb2TrackSize;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleImageCodec::HasAnyNibble
//
//  Whether these bytes could be a nibble stream at all.
//
//  A NIBBLE IS A BYTE WITH ITS HIGH BIT SET, because that is the rule the
//  drive's shift register uses to decide a nibble is complete. A file holding
//  none anywhere cannot be read as a disk by any drive, which makes it the one
//  content failure this format permits us to detect.
//
//  IT CANNOT BE MADE STRICTER USEFULLY. Roughly half of random bytes have the
//  high bit set, so a renamed archive of the right length passes this and will
//  mount as a disk that does not boot. That is the honest outcome: with no
//  signature to check, a stricter rule would refuse odd but genuine images in
//  order to catch files that fail harmlessly anyway, and a false refusal costs
//  a user their disk where a false acceptance costs them one failed boot.
//
////////////////////////////////////////////////////////////////////////////////

bool NibbleImageCodec::HasAnyNibble (const vector<Byte> & raw)
{
    bool  found = false;



    for (Byte value : raw)
    {
        if ((value & 0x80) != 0)
        {
            found = true;
            break;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleImageCodec::Load
//
//  The file's per-track blocks become the image's per-track bit streams.
//
//  Each byte is packed in at eight bits, MSB first, and each $FF of a gap
//  is followed by two zero cells, the self-sync a drive writes there. A read
//  of the stream returns the same bytes in the same order, because the shift
//  register skips the zeros: the load and the derivation are exact inverses
//  for an untouched track.
//
//  THE ZEROS ARE WHAT LETS A GUEST WRITE INTO THE TRACK. A nibble file has
//  the bytes a drive read and none of the timing between them, so a gap of
//  eight-cell $FF bytes has no zero for the shift register to drop. A guest
//  write ends on a bit the file's bytes do not start on, and without the
//  zeros every field after it on the track was read out of step: DOS 3.3
//  could not find the next sector, reported an I/O error, and could not read
//  that sector again.
//
//  Bytes with the high bit clear are packed like any other. They are illegal
//  on real media, they appear in real images anyway, and refusing them would
//  reject files that work.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibbleImageCodec::Load (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT       hr        = S_OK;
    size_t        trackSize = 0;
    size_t        rawSize   = 0;
    size_t        bitCount  = 0;
    size_t        bitPos    = 0;
    size_t        i         = 0;
    int           bit       = 0;
    int           track     = 0;
    bool          readable  = false;
    vector<bool>  isGapSync;



    rawSize = raw.size();

    hr = ResolveGeometry (rawSize, trackSize);
    CHR (hr);

    //  The one content check the format allows, and it has to happen HERE
    //  rather than only in the failure classifier. A classifier is consulted
    //  when a load fails; a verdict no load can produce is a verdict the user
    //  never sees. Content carrying no high bit anywhere cannot be read by any
    //  drive, so refusing it is refusing a file that could never work.
    readable = HasAnyNibble (raw);
    CBREx (readable, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

    for (track = 0; track < kTrackCount; track++)
    {
        const Byte  * block = &raw[static_cast<size_t> (track) * trackSize];

        FindGapSync (block, trackSize, isGapSync);

        bitCount = trackSize * kBitsPerNibble + static_cast<size_t> (std::ranges::count (isGapSync, true)) * kSelfSyncZeros;

        out.ResizeTrack (track, bitCount);

        vector<Byte>  &  buf = out.GetTrackBitsForWrite (track);

        for (i = 0, bitPos = 0; i < trackSize; i++)
        {
            for (bit = 7; bit >= 0; bit--, bitPos++)
            {
                if (((block[i] >> bit) & 1) != 0)
                {
                    buf[bitPos >> 3] = static_cast<Byte> (buf[bitPos >> 3] | (0x80 >> (bitPos & 7)));
                }
            }

            //  ResizeTrack zero-filled the buffer, so the self-sync zeros
            //  are already there.
            if (isGapSync[i])
            {
                bitPos += kSelfSyncZeros;
            }
        }
    }

    out.SetSourceFormat (DiskFormat::Nib);
    out.ClearDirty();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleImageCodec::DeriveTrack
//
//  The bytes a drive would read off one track, in order.
//
//  Shift bits until the high bit sets; that byte is a nibble; repeat. This is
//  the sequencer's own rule and it is shared with the sector decoder rather
//  than restated, so the two can never disagree about where a nibble ends.
//
//  A self-sync byte occupies ten bit cells and yields one byte, so a track
//  derives at most trackBits/8 bytes. A loaded nibble track is longer than its
//  block by its gaps' zero cells, so after a guest write of eight-cell nibbles
//  over them a track can derive a byte or two more than its block has room
//  for, and the surplus comes off the gap in Render.
//
//  AN EMPTY RESULT IS AN ANSWER HERE, NOT A SWALLOWED FAILURE, which is worth
//  stating because the reverse is a standing hazard in this codebase. A track
//  with no bits, or none with the high bit set, is unformatted; the caller
//  fills its block with sync, and unformatted media reading back as gap is
//  exactly right. What would be a silent failure is a SHORT result for a track
//  that had data, and that cannot happen: the walk stops only at the end of the
//  revolution or at a stretch carrying no nibble at all.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibbleImageCodec::DeriveTrack (const DiskImage & img, int track, vector<Byte> & outNibbles)
{
    HRESULT   hr        = S_OK;   // vestigial, for the bail
    size_t    trackBits = 0;
    size_t    bitPos    = 0;
    Byte      nibble    = 0;
    bool      hasBits   = false;



    outNibbles.clear();

    trackBits = img.GetTrackBitCount (track);
    hasBits   = trackBits != 0;

    BAIL_OUT_IF (!hasBits, S_OK);

    outNibbles.reserve (trackBits / 8);

    while (bitPos < trackBits)
    {
        //  Zero cells left at the end of the revolution are the self-sync of
        //  the gap that wraps to the start. A read begun in them finds the
        //  track's first nibble again, so the walk ends there.
        while (bitPos < trackBits && img.ReadBit (track, bitPos) == 0)
        {
            bitPos++;
        }

        if (bitPos >= trackBits)
        {
            break;
        }

        nibble = NibblizationLayer::ReadNibbleAt (img, track, bitPos);

        if (nibble == 0)
        {
            break;
        }

        outNibbles.push_back (nibble);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleImageCodec::FindGapSync
//
//  Which bytes of a track's block are the sync of a gap: each $FF in the
//  run that ends right before a field's prologue, the track taken as a
//  circle, since a gap can wrap from the end of the block to its start.
//
//  $FF is a legal data nibble too, so a run counts only by what follows it,
//  as in RotateGapToEnd.
//
////////////////////////////////////////////////////////////////////////////////

void NibbleImageCodec::FindGapSync (const Byte * block, size_t count, vector<bool> & outIsGapSync)
{
    size_t  i       = 0;
    size_t  back    = 0;
    size_t  at      = 0;
    bool    isField = false;



    outIsGapSync.assign (count, false);

    for (i = 0; i < count; i++)
    {
        isField = block[i]               == NibblizationLayer::kProlog0 &&
                  block[(i + 1) % count] == NibblizationLayer::kProlog1 &&
                  (block[(i + 2) % count] == NibblizationLayer::kAddressProlog2 ||
                   block[(i + 2) % count] == NibblizationLayer::kDataProlog2);

        for (back = 1; isField && back < count; back++)
        {
            at = (i + count - back) % count;

            if (block[at] != kSyncNibble || outIsGapSync[at])
            {
                break;
            }

            outIsGapSync[at] = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleImageCodec::RotateGapToEnd
//
//  Turn the derived sequence so its longest run of sync bytes finishes it.
//
//  THE PADDING HAS TO LAND SOMEWHERE, and anywhere inside an address or data
//  field destroys it. Rotating is free: the track is a circle and the Disk II
//  controller has no index sensor, so no guest can tell where the file's copy
//  of it begins.
//
//  A GAP IS NOT MERELY THE LONGEST RUN OF $FF. That was the first rule here and
//  it silently destroyed sectors: $FF is a legal 6-and-2 nibble, so DATA encodes
//  to runs of it, and a sector's own data can carry a longer one than the gap
//  between sectors. Alternating $AA/$55 source bytes produce a 255-byte run
//  inside the first data field where the real address gap is twenty -- so the
//  padding went into the middle of a sector and took it out. A uniform fill
//  never shows this; it took a walking-bit pattern to surface it.
//
//  So a gap is identified by what FOLLOWS it: a run of sync immediately before
//  a field's prologue is, by definition, the space between fields. The longest
//  such run wins.
//
//  A track with no qualifying run keeps the longest sync run it has, and one
//  with no sync at all keeps the rotation it came with. Neither is a standard
//  track, so there is no structural answer to find -- but the padding still has
//  to go somewhere, and the widest quiet stretch remains the best guess.
//
//  The sequence is a circle, so a run at its end goes on into the run at its
//  start, and the two are one gap when a field follows. A track derived from
//  bit zero of a loaded nibble track ends in exactly that gap.
//
////////////////////////////////////////////////////////////////////////////////

void NibbleImageCodec::RotateGapToEnd (vector<Byte> & nibbles)
{
    size_t  count       = nibbles.size();
    size_t  runStart    = 0;
    size_t  runLength   = 0;
    size_t  bestEnd     = 0;
    size_t  bestLen     = 0;
    size_t  fallbackEnd = 0;
    size_t  fallbackLen = 0;
    size_t  end         = 0;
    size_t  leading     = 0;
    size_t  trailing    = 0;
    size_t  i           = 0;
    bool    beforeField = false;



    for (i = 0; i <= count; i++)
    {
        bool  isSync = (i < count) && (nibbles[i] == kSyncNibble);

        if (isSync)
        {
            if (runLength == 0)
            {
                runStart = i;
            }

            runLength++;
            continue;
        }

        //  The run just ended at i. Whether it was a gap depends on what
        //  starts here.
        end         = runStart + runLength;
        beforeField = (runLength != 0) && (i + 2 < count) &&
                      (nibbles[i]     == NibblizationLayer::kProlog0) &&
                      (nibbles[i + 1] == NibblizationLayer::kProlog1) &&
                      (nibbles[i + 2] == NibblizationLayer::kAddressProlog2 ||
                       nibbles[i + 2] == NibblizationLayer::kDataProlog2);

        if (beforeField && runLength > bestLen)
        {
            bestLen = runLength;
            bestEnd = end;
        }

        if (runLength > fallbackLen)
        {
            fallbackLen = runLength;
            fallbackEnd = end;
        }

        runLength = 0;
    }

    while (leading < count && nibbles[leading] == kSyncNibble)
    {
        leading++;
    }

    while (leading < count && trailing < count - leading && nibbles[count - 1 - trailing] == kSyncNibble)
    {
        trailing++;
    }

    beforeField = (trailing != 0) && (leading + 2 < count) &&
                  (nibbles[leading]     == NibblizationLayer::kProlog0) &&
                  (nibbles[leading + 1] == NibblizationLayer::kProlog1) &&
                  (nibbles[leading + 2] == NibblizationLayer::kAddressProlog2 ||
                   nibbles[leading + 2] == NibblizationLayer::kDataProlog2);

    if (beforeField && leading + trailing > bestLen)
    {
        bestLen = leading + trailing;
        bestEnd = leading;
    }

    if (bestLen == 0)
    {
        bestLen = fallbackLen;
        bestEnd = fallbackEnd;
    }

    if (bestLen != 0 && bestEnd < count)
    {
        std::rotate (nibbles.begin(), nibbles.begin() + bestEnd, nibbles.end());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleImageCodec::Build
//
//  A new nibble image at the size its name asks for.
//
//  SEPARATE FROM Serialize BECAUSE THE QUESTION IS DIFFERENT. Serialize writes
//  an image back and takes its geometry from the file it came from; there is no
//  such file here, so the size is a parameter and every track is derived. This
//  is the one place a 6,384-byte track can be chosen deliberately -- writing a
//  .nb2 that really is one, rather than 6,656-byte tracks under that name.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibbleImageCodec::Build (const DiskImage & img, size_t trackSize, vector<Byte> & out)
{
    HRESULT   hr    = S_OK;
    bool      sized = (trackSize == kNibTrackSize) || (trackSize == kNb2TrackSize);



    //  A size that is not one of the two is a caller bug, not user input: the
    //  words that reach here come from the container table.
    CBRAEx (sized, E_INVALIDARG);

    //  No source bytes, so every track is derived. Handing Render a synthetic
    //  buffer here would be worse than useless: it would look like a source,
    //  and the clean tracks -- which is all of them on a freshly built image --
    //  would be COPIED out of it instead of derived, producing a blank disk.
    hr = Render (img, vector<Byte>(), trackSize, out);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleImageCodec::Serialize
//
//  The image back to a nibble file.
//
//  A CLEAN TRACK IS COPIED, NEVER RE-DERIVED. That is what makes an untouched
//  track byte-identical after a flush, and it is not merely an optimization:
//  a track holding bytes with the high bit clear does NOT survive a round trip,
//  because such a byte is absorbed into the next one's shift. Copying sidesteps
//  that for every track the guest did not write, which is all of them on the
//  common path.
//
//  A dirty track is derived, rotated so its widest gap ends it, and padded with
//  sync bytes to the block size. Every byte written therefore has its high bit
//  set unless it was copied, so the padding reads back as ordinary gap rather
//  than as a stretch the drive can never assemble a nibble from.
//
//  Without usable source bytes every track is derived. That happens only for an
//  image this process synthesized rather than loaded; there is nothing to copy,
//  and deriving is correct because such a track was built from sync-bearing
//  nibbles in the first place.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibbleImageCodec::Serialize (
    const DiskImage     &  img,
    const vector<Byte>  &  sourceBytes,
    vector<Byte>        &  out)
{
    HRESULT   hr         = S_OK;
    HRESULT   hrSource   = S_OK;
    size_t    trackSize  = 0;
    size_t    sourceSize = 0;



    sourceSize = sourceBytes.size();

    hrSource   = ResolveGeometry (sourceSize, trackSize);

    if (FAILED (hrSource))
    {
        //  Nothing to copy from, so the block size has to be chosen rather
        //  than measured.
        //
        //  IT IS NOT THE TRACK'S OWN LENGTH. A synthesized track is trimmed to
        //  the bits its content occupies -- a freshly encoded DOS 3.3 track is
        //  50,624 bits, which is 6,328 bytes and not a nibble geometry at all.
        //  Reading the size off the track therefore produces a file length no
        //  loader would accept. The standard size is the answer here; a caller
        //  that wants the smaller one names it, through Build.
        trackSize = kNibTrackSize;
    }

    hr = Render (img, sourceBytes, trackSize, out);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleImageCodec::Render
//
//  Every track laid into a file of blocks of `trackSize`.
//
//  A CLEAN TRACK IS COPIED, NEVER RE-DERIVED, whenever there are source bytes
//  to copy from. That is what makes an untouched track byte-identical after a
//  flush, and it is not merely an optimization: a track holding bytes with the
//  high bit clear does NOT survive a round trip, because such a byte is
//  absorbed into the next one's shift. Copying sidesteps that for every track
//  the guest did not write, which is all of them on the common path.
//
//  With no source bytes every track is derived, dirty or not. That is the
//  creating case, where there is nothing to copy and the tracks were built
//  from sync-bearing nibbles in the first place.
//
//  A dirty track is derived, rotated so its widest gap ends it, and padded with
//  sync bytes. Every byte written therefore has its high bit set unless it was
//  copied, so the padding reads back as ordinary gap rather than as a stretch
//  the drive can never assemble a nibble from.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibbleImageCodec::Render (
    const DiskImage     &  img,
    const vector<Byte>  &  sourceBytes,
    size_t                 trackSize,
    vector<Byte>        &  out)
{
    HRESULT       hr          = S_OK;
    size_t        nibbleCount = 0;
    size_t        offset      = 0;
    size_t        needed      = trackSize * kTrackCount;
    int           track       = 0;
    bool          hasSource   = sourceBytes.size() == needed;
    bool          fitsInBlock = false;
    vector<Byte>  nibbles;



    out.assign (needed, kSyncNibble);

    for (track = 0; track < kTrackCount; track++)
    {
        bool  isDirty = img.IsTrackDirty (track);

        offset = static_cast<size_t> (track) * trackSize;

        if (hasSource && !isDirty)
        {
            memcpy (&out[offset], &sourceBytes[offset], trackSize);
            continue;
        }

        //  Only the shared sector writer changed this track, and it changes
        //  whole nibbles in place, so the track's bits ARE its block: every
        //  other nibble stays where it was rather than being derived again.
        if (hasSource && img.IsTrackChangedOnlyByWriter (track) && img.GetTrackBitCount (track) == trackSize * kBitsPerNibble)
        {
            memcpy (&out[offset], img.GetTrackBits (track).data(), trackSize);
            continue;
        }

        hr = DeriveTrack (img, track, nibbles);
        CHR (hr);

        //  Only when there is padding to place or a surplus to take off. A
        //  track that fills its block exactly gets neither, and rotating it
        //  anyway would move every byte for no reason -- which would also cost
        //  the guarantee that a full track derives back byte-identical.
        if (nibbles.size() != trackSize)
        {
            RotateGapToEnd (nibbles);
        }

        //  After a guest write of eight-cell nibbles over a gap's ten-cell
        //  sync, a track can derive a byte or two more than its block has room
        //  for. They come off the sync RotateGapToEnd moved to the end.
        while (nibbles.size() > trackSize && nibbles.back() == kSyncNibble)
        {
            nibbles.pop_back();
        }

        //  A track too long after that has no sync at its end: a guest wrote
        //  more nibbles than a block can store. The save fails rather than
        //  cutting a field short.
        nibbleCount = nibbles.size();
        fitsInBlock = nibbleCount <= trackSize;

        CBREx (fitsInBlock, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        memcpy (&out[offset], nibbles.data(), nibbleCount);
    }

Error:
    return hr;
}
