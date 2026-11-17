#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/PreviewDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewChoices
//
//  The preview's View as list: a file's default view, which PreviewDecoder
//  chooses, and the other views of its bytes that apply to it. Text and hex
//  apply to any file with data, a disassembly to any file with data, a
//  picture mode to a file the size of that mode's screen, and the resource
//  fork to a file that has one. A view the default already is isn't
//  listed again.
//
//  A chosen view that doesn't apply to the file, as when another file is
//  selected, is the default.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewViewChoices
{
public:
    //  The file's default view rendered, its other views listed in
    //  `viewChoices`, and `view` rendered in place of the default when it
    //  applies.
    static HRESULT  Render (ViewChoice                 view,
                            const FileEntry          & entry,
                            VolumeKind                 kind,
                            const FilePayload        & payload,
                            PreviewDecoder::HexForm    hexForm,
                            MemoryBus                & bus,
                            PreviewContent           & outContent,
                            PicturePreview::Monitor    monitor,
                            uint32_t                   customArgb,
                            const DecodeOptions      & options);

    //  The views other than the default that apply to the payload.
    static void  List (const FilePayload & payload, const PreviewContent & defaultView, std::vector<ViewChoice> & outChoices);

    //  The default view's name in the list: "BASIC listing", "Text",
    //  "Hi-res picture" and so on.
    static std::wstring  DescribeDefault (const PreviewContent & content);

    //  One view of the payload, not the default.
    static HRESULT  RenderAs (ViewChoice                view,
                              const FileEntry         & entry,
                              VolumeKind                kind,
                              const FilePayload       & payload,
                              PreviewDecoder::HexForm   hexForm,
                              MemoryBus               & bus,
                              PreviewContent          & outContent,
                              PicturePreview::Monitor   monitor,
                              uint32_t                  customArgb);

private:
    //  The byte counts of a screen saved by each picture mode: the 8,192 or
    //  8,184 of a hi-res page, double that for double hi-res, and the 1,024
    //  or 1,016 of a lo-res page.
    static constexpr size_t  kHiResMin       = 8184;
    static constexpr size_t  kHiResMax       = 8192;
    static constexpr size_t  kDoubleHiResMin = 16368;
    static constexpr size_t  kDoubleHiResMax = 16384;
    static constexpr size_t  kLoResMin       = 1016;
    static constexpr size_t  kLoResMax       = 1024;
    static constexpr Word    kHiResPage      = 0x2000;
    static constexpr Word    kLoResPage      = 0x0400;

    static bool  IsPictureSize (size_t bytes, size_t minimum, size_t maximum) { return bytes >= minimum && bytes <= maximum; }
    static void  SplitLines    (const std::string & text, std::vector<std::wstring> & outLines);
};
