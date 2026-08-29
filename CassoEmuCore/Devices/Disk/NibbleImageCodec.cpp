#include "Pch.h"

#include "NibbleImageCodec.h"
#include "NibblizationLayer.h"





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
//  Each block is packed straight in at eight bits per byte, MSB first, so a
//  track of N bytes becomes a track of N*8 bits. That is what makes the load
//  and the derivation exact inverses for an untouched track: eight shifts
//  assemble byte zero and set the high bit on the eighth, so reading the
//  stream back gives the same bytes in the same order.
//
//  Bytes with the high bit clear are packed like any other. They are illegal
//  on real media, they appear in real images anyway, and refusing them would
//  reject files that work.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NibbleImageCodec::Load (const vector<Byte> & raw, DiskImage & out)
{
    HRESULT   hr        = S_OK;
    size_t    trackSize = 0;
    size_t    rawSize   = 0;
    int       track     = 0;
    bool      readable  = false;



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
        vector<Byte>  &  buf    = out.GetTrackBitsForWrite (track);
        size_t           offset = static_cast<size_t> (track) * trackSize;

        out.ResizeTrack (track, trackSize * 8);

        memcpy (buf.data(), &raw[offset], trackSize);

        out.SetTrackBitCount (track, trackSize * 8);
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
//  THE COUNT CAN ONLY FALL, NEVER RISE. A self-sync byte occupies ten bit
//  cells and still yields one byte, so a track carrying any sync derives fewer
//  bytes than its block holds. The ceiling is trackBits/8, which IS the block
//  size, so overflow is arithmetic rather than something to guard against.
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
//  NibbleImageCodec::RotateGapToEnd
//
//  Turn the derived sequence so its longest run of sync bytes finishes it.
//
//  THE PADDING HAS TO LAND SOMEWHERE, and anywhere inside an address or data
//  field destroys it. The longest run of $FF is the widest gap on the track,
//  which is where a real drive's slack sits and the only place bytes can be
//  inserted harmlessly. Rotating is free: the track is a circle and the Disk II
//  controller has no index sensor, so no guest can tell where the file's copy
//  of it begins.
//
//  A track with no sync run at all keeps the rotation it came with. There is no
//  safe place on such a track, so the padding goes to the seam the derivation
//  already made by breaking the circle at bit zero.
//
////////////////////////////////////////////////////////////////////////////////

void NibbleImageCodec::RotateGapToEnd (vector<Byte> & nibbles)
{
    size_t  count     = nibbles.size();
    size_t  runStart  = 0;
    size_t  runLength = 0;
    size_t  bestEnd   = 0;
    size_t  bestLen   = 0;
    size_t  i         = 0;



    for (i = 0; i < count; i++)
    {
        if (nibbles[i] != kSyncNibble)
        {
            runLength = 0;
            continue;
        }

        if (runLength == 0)
        {
            runStart = i;
        }

        runLength++;

        if (runLength > bestLen)
        {
            bestLen = runLength;
            bestEnd = runStart + runLength;
        }
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

        hr = DeriveTrack (img, track, nibbles);
        CHR (hr);

        //  More derived bytes than the block holds is arithmetically
        //  impossible, so this asserts rather than truncating: reaching it
        //  means the derivation or the track length is wrong.
        nibbleCount = nibbles.size();
        fitsInBlock = nibbleCount <= trackSize;

        CBRA (fitsInBlock);

        //  Only when there is padding to place. A track that fills its block
        //  exactly needs none, and rotating it anyway would move every byte for
        //  no reason -- which would also cost the guarantee that a full track
        //  derives back byte-identical.
        if (nibbleCount < trackSize)
        {
            RotateGapToEnd (nibbles);
        }

        memcpy (&out[offset], nibbles.data(), nibbleCount);
    }

Error:
    return hr;
}
