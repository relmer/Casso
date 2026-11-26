#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"
#include "FileTypes/QuickDrawCanvas.h"





////////////////////////////////////////////////////////////////////////////////
//
//  QuickDrawPicture
//
//  A IIgs QuickDraw II picture, $C1/$0001, or the same picture packed with
//  PackBytes, $C0/$0003. A picture is a word of the port's SCB, its frame
//  rectangle, then word opcodes and their operands to the end of the file,
//  low byte first. A point is v then h, and a rectangle top, left, bottom,
//  right, each a signed word. The picture is drawn by running its opcodes
//  into a bitmap the size of its frame, in the standard palette for its
//  SCB's mode. Text isn't drawn, since its fonts aren't in the file; each
//  opcode that isn't drawn is listed in the notes.
//
////////////////////////////////////////////////////////////////////////////////

class QuickDrawPicture
{
public:
    static bool     MatchesPicture (const FileFacts & facts, std::span<const Byte> bytes);
    static bool     MatchesPacked  (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode         (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    static constexpr Byte    kTypePnt       = 0xC0;
    static constexpr Byte    kTypePic       = 0xC1;
    static constexpr Word    kAuxPacked     = 0x0003;
    static constexpr Word    kAuxPicture    = 0x0001;
    static constexpr size_t  kHeaderBytes   = 10;

private:
    using Canvas = QuickDrawCanvas;

    //  The state of a picture's playback: where its opcodes are read from,
    //  the line pen, the last figure of each kind for the "same" opcodes,
    //  and the opcodes found that aren't drawn.
    struct Playback
    {
        std::span<const Byte>       bytes;
        size_t                      at        = 0;
        bool                        truncated = false;
        Canvas::Point               pen;
        Canvas::Rect                lastRect;
        Canvas::Rect                lastRRect;
        Canvas::Rect                lastOval;
        Canvas::Rect                lastArc;
        std::vector<Canvas::Point>  lastPoly;
        Canvas::Region              lastRegion;
        std::set<Word>              skipped;
        std::vector<std::wstring>   notes;
    };

    static bool                        Run           (Playback & pb, Canvas & canvas);
    static bool                        RunState      (Playback & pb, Canvas & canvas, Word opcode);
    static bool                        RunLine       (Playback & pb, Canvas & canvas, Word opcode);
    static bool                        RunText       (Playback & pb, Word opcode);
    static bool                        RunFigure     (Playback & pb, Canvas & canvas, Word opcode);
    static bool                        RunBits       (Playback & pb, Canvas & canvas, Word opcode);
    static bool                        CanRead       (Playback & pb, size_t count);
    static int                         ReadWord      (Playback & pb);
    static int                         ReadSigned    (Playback & pb);
    static Canvas::Point               ReadPoint     (Playback & pb);
    static Canvas::Rect                ReadRect      (Playback & pb);
    static Canvas::Region              ReadRegion    (Playback & pb);
    static std::vector<Canvas::Point>  ReadPoly      (Playback & pb);
    static void                        ReadPattern   (Playback & pb, Canvas::Pattern & outPattern);
    static void                        Skip          (Playback & pb, size_t count);
    static const wchar_t *             GetOpcodeName (Word opcode);
};
