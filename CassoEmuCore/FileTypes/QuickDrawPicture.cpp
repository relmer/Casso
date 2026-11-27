#include "Pch.h"

#include "FileTypes/QuickDrawPicture.h"
#include "FileTypes/PackBytes.h"
#include "FileTypes/IQuickDrawFonts.h"
#include "FileTypes/QuickDrawFont.h"





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::MatchesPicture
//
//  $C1/$0001, or a $C1 file of another aux type that begins with a
//  picture's header and the version opcode. Beagle Bros clip art is
//  $C1/$8000, and Beagle Draw saves pictures as $C1/$0002, the 3200-color
//  screen's aux type. A file of a screen's aux type and size is a screen.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawPicture::MatchesPicture (const FileFacts & facts, std::span<const Byte> bytes)
{
    constexpr Word    kAuxScreen       = 0x0000;
    constexpr Word    kAuxScreen3200   = 0x0002;
    constexpr size_t  kScreenBytes     = 32768;
    constexpr size_t  kScreen3200Bytes = 38400;
    constexpr Byte    kVersionLow      = 0x11;
    constexpr Byte    kVersionHigh     = 0x82;



    bool  isScreen = (facts.auxType == kAuxScreen && bytes.size() == kScreenBytes) || (facts.auxType == kAuxScreen3200 && bytes.size() == kScreen3200Bytes);



    if (facts.HasProDosType (kTypePic, kAuxPicture))
    {
        return !bytes.empty();
    }

    return facts.HasProDosType (kTypePic) && !isScreen
        && bytes.size() > kHeaderBytes + 1
        && bytes[kHeaderBytes] == kVersionLow && bytes[kHeaderBytes + 1] == kVersionHigh;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::MatchesPacked
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawPicture::MatchesPacked (const FileFacts & facts, std::span<const Byte> bytes)
{
    return facts.HasProDosType (kTypePnt, kAuxPacked) && !bytes.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::Decode
//
//  A packed picture is unpacked whole first. The frame is the bitmap's
//  size, and bit 7 of the SCB selects 640 mode.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT QuickDrawPicture::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    constexpr size_t  kMaxUnpacked = 16 * 1024 * 1024;
    constexpr int     kMaxSide     = 4096;
    constexpr int     kScb640      = 0x80;



    HRESULT                  hr       = S_OK;
    std::vector<Byte>        unpacked;
    Playback                 pb;
    Canvas::Rect             frame;
    std::unique_ptr<Canvas>  canvas;
    int                      scb      = 0;
    int                      width    = 0;
    int                      height   = 0;
    bool                     isLong   = false;
    bool                     isSized  = false;
    const wchar_t          * name     = nullptr;



    pb.bytes = bytes;
    pb.fonts = options.quickDrawFonts;

    if (facts.HasProDosType (kTypePnt, kAuxPacked))
    {
        PackBytes::Unpack (bytes, kMaxUnpacked, unpacked);
        pb.bytes = unpacked;
    }

    isLong = pb.bytes.size() >= kHeaderBytes;
    CBRFEx (isLong, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The QuickDraw II picture is too short to have a frame.");

    scb    = ReadWord (pb);
    frame  = ReadRect (pb);
    width  = frame.right - frame.left;
    height = frame.bottom - frame.top;

    isSized = width > 0 && height > 0 && width <= kMaxSide && height <= kMaxSide;
    CBRFEx (isSized, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The QuickDraw II picture's frame isn't a usable size.");

    canvas = std::make_unique<Canvas> (frame, (scb & kScb640) != 0);
    CPRA (canvas);

    Run (pb, *canvas);

    for (const auto & [family, size] : pb.missingFonts)
    {
        name = QuickDrawText::GetFamilyName (family);
        outFile.fontNotes.push_back (std::format (L"Text in {} {} isn't drawn, since the font isn't available.",
                                                        (name != nullptr) ? std::wstring (name) : std::format (L"font family ${:04X}", family), size));
    }

    if (pb.truncated)
    {
        pb.notes.push_back (L"The picture ends partway through an opcode.");
    }

    outFile.kind         = DecodedFile::Kind::Picture;
    outFile.width        = width;
    outFile.height       = height;
    outFile.notes        = std::move (pb.notes);
    canvas->WriteBgra (outFile.bgra);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::Run
//
//  Each opcode to the end of the picture. An opcode whose low byte is $11
//  is the version, and has no operands. Playback stops at an opcode that
//  isn't defined, since its operands' length is unknown.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawPicture::Run (Playback & pb, Canvas & canvas)
{
    constexpr Word  kVersionLow = 0x11;
    constexpr Word  kLowByte    = 0xFF;



    Word  opcode  = 0;
    bool  handled = false;



    while (!pb.truncated && CanRead (pb, sizeof (Word)))
    {
        opcode = (Word) ReadWord (pb);

        if ((opcode & kLowByte) == kVersionLow)
        {
            continue;
        }

        handled = RunState (pb, canvas, opcode)
               || RunLine  (pb, canvas, opcode)
               || RunText  (pb, canvas, opcode)
               || RunFigure (pb, canvas, opcode)
               || RunBits  (pb, canvas, opcode);

        if (!handled)
        {
            pb.notes.push_back (std::format (L"Opcode ${:04X} at offset ${:X} isn't defined, so drawing stopped there.", opcode, pb.at - sizeof (Word)));
            return false;
        }
    }

    return !pb.truncated;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::RunState
//
//  The opcodes that set the port's state. The font is a Font Manager font
//  ID: the family number, a style byte and the size. The extras are fixed
//  point, 16 bits of fraction. Font flag bits 1 and 2, which System
//  Software 5.0 added for pen width and color, aren't documented and have
//  no effect here.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawPicture::RunState (Playback & pb, Canvas & canvas, Word opcode)
{
    Canvas::Pattern  pattern = {};
    Canvas::PenMask  mask    = {};



    switch (opcode)
    {
        case 0x0000:                                         break;
        case 0x0001:  canvas.SetClip     (ReadRegion (pb));  break;
        case 0x0007:  canvas.SetPenSize  (ReadPoint  (pb));  break;
        case 0x0008:  canvas.SetPenMode  (ReadWord   (pb));  break;
        case 0x000B:  canvas.SetOvalSize (ReadPoint  (pb));  break;
        case 0x000C:  canvas.SetOrigin   (ReadPoint  (pb));  break;

        case 0x0002:
            ReadPattern (pb, pattern);
            canvas.SetBkPat (pattern);
            break;

        case 0x0009:
            ReadPattern (pb, pattern);
            canvas.SetPenPat (pattern);
            break;

        case 0x000A:
            ReadPattern (pb, pattern);
            canvas.SetFillPat (pattern);
            break;

        case 0x0003:
            pb.text.family = (Word) ReadWord (pb);
            Skip (pb, 1);

            if (CanRead (pb, 1))
            {
                pb.text.size = pb.bytes[pb.at];
            }

            Skip (pb, 1);
            break;

        case 0x0004:  pb.text.face      = (Word) ReadWord (pb);  break;
        case 0x0005:  pb.text.mode      = ReadWord  (pb);        break;
        case 0x0006:  pb.text.spExtra   = ReadFixed (pb);        break;
        case 0x000D:  pb.text.size      = ReadWord  (pb);        break;
        case 0x000E:  pb.text.foreColor = ReadWord  (pb);        break;
        case 0x000F:  pb.text.backColor = ReadWord  (pb);        break;
        case 0x0012:  pb.text.chExtra   = ReadFixed (pb);        break;
        case 0x0014:  canvas.SetArcRot (ReadWord (pb));          break;
        case 0x0015:  pb.text.fontFlags = (Word) ReadWord (pb);  break;

        case 0x0013:
            if (CanRead (pb, mask.size()))
            {
                std::copy_n (pb.bytes.begin() + (ptrdiff_t) pb.at, mask.size(), mask.begin());
                canvas.SetPenMask (mask);
            }

            Skip (pb, mask.size());
            break;

        default:
            return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::RunLine
//
//  A short line's offsets are signed bytes, dv then dh. Lines move the
//  line pen alone.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawPicture::RunLine (Playback & pb, Canvas & canvas, Word opcode)
{
    Canvas::Point  from = pb.pen;
    Canvas::Point  to;



    switch (opcode)
    {
        case 0x0020:
            from = ReadPoint (pb);
            to   = ReadPoint (pb);
            break;

        case 0x0021:
            to = ReadPoint (pb);
            break;

        case 0x0022:
            from = ReadPoint (pb);
            [[fallthrough]];

        case 0x0023:
            if (CanRead (pb, sizeof (Word)))
            {
                to.v = from.v + (int8_t) pb.bytes[pb.at];
                to.h = from.h + (int8_t) pb.bytes[pb.at + 1];
            }

            Skip (pb, sizeof (Word));
            break;

        default:
            return false;
    }

    if (!pb.truncated)
    {
        canvas.DrawLine (from, to);
        pb.pen = to;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::RunText
//
//  The text pen is set by LongText and RealLongText and moved by the offset
//  opcodes, and drawing text doesn't move it. The text is drawn at the pen
//  in the font of the port's family and size; text in a font that isn't
//  available is left out, and its font listed.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawPicture::RunText (Playback & pb, Canvas & canvas, Word opcode)
{
    size_t                 count = 0;
    const QuickDrawFont  * font  = nullptr;



    switch (opcode)
    {
        case 0x0028:
        case 0x002C:
            pb.textPen = ReadPoint (pb);
            break;

        case 0x0029:
            pb.textPen.h += CanRead (pb, 1) ? pb.bytes[pb.at] : 0;
            Skip (pb, 1);
            break;

        case 0x002A:
            pb.textPen.v += CanRead (pb, 1) ? pb.bytes[pb.at] : 0;
            Skip (pb, 1);
            break;

        case 0x002B:
            pb.textPen.v += CanRead (pb, 1) ? pb.bytes[pb.at] : 0;
            Skip (pb, 1);
            pb.textPen.h += CanRead (pb, 1) ? pb.bytes[pb.at] : 0;
            Skip (pb, 1);
            break;

        default:
            return false;
    }

    if (opcode == 0x002C)
    {
        count = (size_t) ReadWord (pb);
    }
    else if (CanRead (pb, 1))
    {
        count = pb.bytes[pb.at++];
    }

    if (!CanRead (pb, count))
    {
        Skip (pb, count);
        return true;
    }

    font = (pb.fonts != nullptr) ? pb.fonts->Find (pb.text.family, pb.text.size) : nullptr;

    if (font != nullptr)
    {
        QuickDrawText::Draw (canvas, *font, pb.text, pb.textPen, pb.bytes.subspan (pb.at, count));
    }
    else
    {
        pb.missingFonts.insert ({ pb.text.family, pb.text.size });
    }

    Skip (pb, count);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::RunFigure
//
//  $0030 to $008C: the high nibble the figure (rectangle, round rectangle,
//  oval, arc, polygon, region) and the low nibble the verb (frame, paint,
//  erase, invert, fill), plus 8 for the last figure of that kind. An arc's
//  angles follow even the "same" opcode.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawPicture::RunFigure (Playback & pb, Canvas & canvas, Word opcode)
{
    constexpr Word  kFirst    = 0x0030;
    constexpr Word  kLast     = 0x008C;
    constexpr Word  kSameBit  = 0x0008;
    constexpr int   kVerbs    = 5;



    int           figure     = (opcode >> 4) - (kFirst >> 4);
    int           verbIndex  = opcode & (kSameBit - 1);
    bool          same       = (opcode & kSameBit) != 0;
    Canvas::Verb  verb       = (Canvas::Verb) verbIndex;
    int           startAngle = 0;
    int           arcAngle   = 0;



    if (opcode < kFirst || opcode > kLast || verbIndex >= kVerbs)
    {
        return false;
    }

    switch (figure)
    {
        case 0:  pb.lastRect   = same ? pb.lastRect   : ReadRect   (pb);  break;
        case 1:  pb.lastRRect  = same ? pb.lastRRect  : ReadRect   (pb);  break;
        case 2:  pb.lastOval   = same ? pb.lastOval   : ReadRect   (pb);  break;
        case 3:  pb.lastArc    = same ? pb.lastArc    : ReadRect   (pb);  break;
        case 4:  pb.lastPoly   = same ? pb.lastPoly   : ReadPoly   (pb);  break;
        default: pb.lastRegion = same ? pb.lastRegion : ReadRegion (pb);  break;
    }

    if (figure == 3)
    {
        startAngle = ReadSigned (pb);
        arcAngle   = ReadSigned (pb);
    }

    if (pb.truncated)
    {
        return true;
    }

    switch (figure)
    {
        case 0:  canvas.DrawRect   (verb, pb.lastRect);                         break;
        case 1:  canvas.DrawRRect  (verb, pb.lastRRect);                        break;
        case 2:  canvas.DrawOval   (verb, pb.lastOval);                         break;
        case 3:  canvas.DrawArc    (verb, pb.lastArc, startAngle, arcAngle);    break;
        case 4:  canvas.DrawPoly   (verb, pb.lastPoly);                         break;
        default: canvas.DrawRegion (verb, pb.lastRegion);                       break;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::RunBits
//
//  A pixel image: its SCB, a reserved word, its bytes a row, its bounds,
//  the source and destination rectangles and the transfer mode, a mask
//  region for BitsRgn alone, then the rows of the bounds. A long comment is
//  passed over.
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawPicture::RunBits (Playback & pb, Canvas & canvas, Word opcode)
{
    constexpr int   kScb640       = 0x80;
    constexpr Word  kBitsRect     = 0x0090;
    constexpr Word  kBitsRgn      = 0x0091;
    constexpr Word  kLongComment  = 0x00A1;



    int                    scb      = 0;
    int                    rowBytes = 0;
    Canvas::Rect           bounds;
    Canvas::Rect           source;
    Canvas::Rect           dest;
    int                    mode     = 0;
    size_t                 size     = 0;
    std::span<const Byte>  pixels;
    Canvas::Region         mask;



    if (opcode == kLongComment)
    {
        Skip (pb, sizeof (Word));
        Skip (pb, (size_t) ReadWord (pb));
        return true;
    }

    if (opcode != kBitsRect && opcode != kBitsRgn)
    {
        return false;
    }

    scb      = ReadWord (pb);
    Skip (pb, sizeof (Word));
    rowBytes = ReadWord (pb);
    bounds   = ReadRect (pb);
    source   = ReadRect (pb);
    dest     = ReadRect (pb);
    mode     = ReadWord (pb);

    if (opcode == kBitsRgn)
    {
        mask = ReadRegion (pb);
    }

    size = (size_t) rowBytes * (size_t) (std::max) (bounds.bottom - bounds.top, 0);

    if (!pb.truncated && CanRead (pb, size))
    {
        pixels = pb.bytes.subspan (pb.at, size);
        canvas.DrawPixels (pixels, rowBytes, (scb & kScb640) != 0, bounds, source, dest, mode, (opcode == kBitsRgn) ? &mask : nullptr);
    }

    Skip (pb, size);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::CanRead
//
////////////////////////////////////////////////////////////////////////////////

bool QuickDrawPicture::CanRead (Playback & pb, size_t count)
{
    return pb.at <= pb.bytes.size() && pb.bytes.size() - pb.at >= count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::Skip
//
//  Moves past bytes; past the end, the picture is truncated.
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawPicture::Skip (Playback & pb, size_t count)
{
    if (!CanRead (pb, count))
    {
        pb.truncated = true;
        pb.at        = pb.bytes.size();
        return;
    }

    pb.at += count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::ReadWord
//
//  A word, low byte first, or 0 past the end.
//
////////////////////////////////////////////////////////////////////////////////

int QuickDrawPicture::ReadWord (Playback & pb)
{
    int  value = 0;



    if (CanRead (pb, sizeof (Word)))
    {
        value = pb.bytes[pb.at] | (pb.bytes[pb.at + 1] << 8);
    }

    Skip (pb, sizeof (Word));

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::ReadSigned
//
////////////////////////////////////////////////////////////////////////////////

int QuickDrawPicture::ReadSigned (Playback & pb)
{
    return (int) (int16_t) ReadWord (pb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::ReadFixed
//
//  A fixed point number: a long, low word first, with 16 bits of fraction.
//
////////////////////////////////////////////////////////////////////////////////

double QuickDrawPicture::ReadFixed (Playback & pb)
{
    constexpr double  kOne = 65536.0;



    int  low  = ReadWord (pb);
    int  high = ReadSigned (pb);



    return high + low / kOne;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::ReadPoint
//
////////////////////////////////////////////////////////////////////////////////

QuickDrawCanvas::Point QuickDrawPicture::ReadPoint (Playback & pb)
{
    Canvas::Point  point;



    point.v = ReadSigned (pb);
    point.h = ReadSigned (pb);

    return point;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::ReadRect
//
////////////////////////////////////////////////////////////////////////////////

QuickDrawCanvas::Rect QuickDrawPicture::ReadRect (Playback & pb)
{
    Canvas::Rect  rect;



    rect.top    = ReadSigned (pb);
    rect.left   = ReadSigned (pb);
    rect.bottom = ReadSigned (pb);
    rect.right  = ReadSigned (pb);

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::ReadPattern
//
////////////////////////////////////////////////////////////////////////////////

void QuickDrawPicture::ReadPattern (Playback & pb, Canvas::Pattern & outPattern)
{
    if (CanRead (pb, outPattern.size()))
    {
        std::copy_n (pb.bytes.begin() + (ptrdiff_t) pb.at, outPattern.size(), outPattern.begin());
    }

    Skip (pb, outPattern.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::ReadPoly
//
//  The polygon's size in bytes, its bounds, then its points.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<QuickDrawCanvas::Point> QuickDrawPicture::ReadPoly (Playback & pb)
{
    constexpr size_t  kPolyHeader = sizeof (Word) + 4 * sizeof (Word);
    constexpr size_t  kPointBytes = 2 * sizeof (Word);



    std::vector<Canvas::Point>  points;
    size_t                      size   = (size_t) ReadWord (pb);



    ReadRect (pb);

    for (size_t at = kPolyHeader; at + kPointBytes <= size && !pb.truncated; at += kPointBytes)
    {
        points.push_back (ReadPoint (pb));
    }

    return points;
}





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture::ReadRegion
//
//  The region's size in bytes and its bounds. A region larger than that is
//  followed by its rows where inside and outside change: each a v, the h
//  values where they change, and $7FFF, with $7FFF after the last row. A
//  row's changes apply from that row down, so each row of the region is
//  the changes so far.
//
////////////////////////////////////////////////////////////////////////////////

QuickDrawCanvas::Region QuickDrawPicture::ReadRegion (Playback & pb)
{
    constexpr size_t  kRectRegion = sizeof (Word) + 4 * sizeof (Word);
    constexpr int     kEnd        = 0x7FFF;



    Canvas::Region                region;
    size_t                        size    = (size_t) ReadWord (pb);
    size_t                        end     = pb.at - sizeof (Word) + size;
    std::map<int, std::set<int>>  changes;
    std::set<int>                 current;
    int                           v       = 0;
    int                           h       = 0;



    region.bounds = ReadRect (pb);

    if (size <= kRectRegion)
    {
        return region;
    }

    while (!pb.truncated && pb.at < end && (v = ReadSigned (pb)) != kEnd)
    {
        while (!pb.truncated && (h = ReadSigned (pb)) != kEnd)
        {
            if (!changes[v].insert (h).second)
            {
                changes[v].erase (h);
            }
        }
    }

    pb.at = (std::max) (pb.at, (std::min) (end, pb.bytes.size()));

    for (v = region.bounds.top; v < region.bounds.bottom; v++)
    {
        for (int change : changes[v])
        {
            if (!current.insert (change).second)
            {
                current.erase (change);
            }
        }

        region.rows.emplace_back (current.begin(), current.end());
    }

    return region;
}





