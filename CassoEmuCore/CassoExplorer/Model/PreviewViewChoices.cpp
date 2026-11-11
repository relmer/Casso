#include "Pch.h"

#include "CassoExplorer/Model/PreviewViewChoices.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewChoices::Render
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PreviewViewChoices::Render (ViewChoice                 view,
                                    const FileEntry          & entry,
                                    VolumeKind                 kind,
                                    const FilePayload        & payload,
                                    PreviewDecoder::HexForm    hexForm,
                                    MemoryBus                & bus,
                                    PreviewContent           & outContent,
                                    PicturePreview::Monitor    monitor,
                                    uint32_t                   customArgb,
                                    const DecodeOptions      & options)
{
    HRESULT                  hr          = S_OK;
    std::vector<ViewChoice>  choices;
    std::wstring             defaultName;
    PreviewContent           chosen;
    bool                     applies     = false;
    HRESULT                  hrChosen    = S_OK;



    hr = PreviewDecoder::Render (entry, kind, payload, hexForm, bus, outContent, monitor, customArgb, options);
    CHR (hr);

    List (payload, outContent, choices);
    defaultName = DescribeDefault (outContent);
    applies     = std::find (choices.begin(), choices.end(), view) != choices.end();
    hrChosen    = applies ? RenderAs (view, entry, kind, payload, hexForm, bus, chosen, monitor, customArgb) : E_FAIL;

    if (SUCCEEDED (hrChosen))
    {
        outContent = std::move (chosen);
    }
    else
    {
        view = ViewChoice::Default;
    }

    outContent.viewChoices = std::move (choices);
    outContent.viewAs      = view;
    outContent.defaultView = std::move (defaultName);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewChoices::List
//
////////////////////////////////////////////////////////////////////////////////

void PreviewViewChoices::List (const FilePayload & payload, const PreviewContent & defaultView, std::vector<ViewChoice> & outChoices)
{
    size_t                bytes   = payload.bytes.size();
    bool                  hasData = bytes > 0;
    bool                  isHex   = defaultView.kind == PreviewContent::Kind::Hex;
    PicturePreview::Mode  mode    = (defaultView.kind == PreviewContent::Kind::Picture) ? defaultView.pictureMode : PicturePreview::Mode::None;
    bool                  isDhr   = mode == PicturePreview::Mode::DoubleHiRes || mode == PicturePreview::Mode::DoubleHiResMono;



    outChoices.clear();

    if (hasData && defaultView.kind != PreviewContent::Kind::Text)
    {
        outChoices.push_back (ViewChoice::Text);
    }

    if (hasData && !(isHex && !defaultView.disassembly))
    {
        outChoices.push_back (ViewChoice::Hex);
    }

    if (hasData && !(isHex && defaultView.disassembly))
    {
        outChoices.push_back (ViewChoice::Disassembly);
    }

    if (IsPictureSize (bytes, kHiResMin, kHiResMax) && mode != PicturePreview::Mode::HiRes)
    {
        outChoices.push_back (ViewChoice::HiRes);
    }

    if (IsPictureSize (bytes, kDoubleHiResMin, kDoubleHiResMax) && !isDhr)
    {
        outChoices.push_back (ViewChoice::DoubleHiRes);
    }

    if (IsPictureSize (bytes, kLoResMin, kLoResMax) && mode != PicturePreview::Mode::LoRes)
    {
        outChoices.push_back (ViewChoice::LoRes);
    }

    if (!payload.resourceFork.empty())
    {
        outChoices.push_back (ViewChoice::ResourceFork);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewChoices::DescribeDefault
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PreviewViewChoices::DescribeDefault (const PreviewContent & content)
{
    switch (content.kind)
    {
        case PreviewContent::Kind::Listing:   return L"BASIC listing";
        case PreviewContent::Kind::Text:      return L"Text";
        case PreviewContent::Kind::Hex:       return content.disassembly ? L"Disassembly" : L"Hex";
        case PreviewContent::Kind::Document:  return L"Document";
        case PreviewContent::Kind::Grid:      return L"Table";
        case PreviewContent::Kind::Variables: return L"Variables";
        case PreviewContent::Kind::Catalog:   return L"Catalog";

        case PreviewContent::Kind::Picture:
            return (content.pictureMode == PicturePreview::Mode::LoRes) ? L"Lo-res picture"
                 : (content.pictureMode == PicturePreview::Mode::HiRes) ? L"Hi-res picture"
                 : (content.pictureMode == PicturePreview::Mode::None)  ? L"Picture"
                                                                        : L"Double hi-res picture";

        default:
            return L"Preview";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewChoices::RenderAs
//
//  Hex and disassembly are the two forms of the hex view, so the form chosen
//  on its toolbar switches between them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PreviewViewChoices::RenderAs (ViewChoice                view,
                                      const FileEntry         & entry,
                                      VolumeKind                kind,
                                      const FilePayload       & payload,
                                      PreviewDecoder::HexForm   hexForm,
                                      MemoryBus               & bus,
                                      PreviewContent          & outContent,
                                      PicturePreview::Monitor   monitor,
                                      uint32_t                  customArgb)
{
    HRESULT               hr          = S_OK;
    Word                  origin      = payload.hasLoadAddress ? payload.loadAddress : 0;
    PicturePreview::Mode  mode        = PicturePreview::Mode::None;
    bool                  relocatable = kind == VolumeKind::ProDos && entry.type == ProDosVolume::kTypeRelocatable;
    bool                  disassemble = false;
    std::string           text;



    outContent = PreviewContent();

    switch (view)
    {
        case ViewChoice::Text:
            outContent.kind = PreviewContent::Kind::Text;

            if (payload.textForm != PayloadTextForm::AppleHighAscii)
            {
                PreviewDecoder::DecodeVolumeText (payload, text);
                SplitLines (text, outContent.lines);
            }
            else
            {
                PreviewDecoder::SplitAppleText (payload.bytes, outContent.lines);
            }

            break;

        case ViewChoice::Hex:
        case ViewChoice::Disassembly:
            disassemble = (view == ViewChoice::Disassembly) ? hexForm != PreviewDecoder::HexForm::Bytes
                                                            : hexForm == PreviewDecoder::HexForm::Disassembly;

            outContent.kind       = PreviewContent::Kind::Hex;
            outContent.executable = true;

            if (disassemble)
            {
                PreviewDecoder::RenderDisassembly (payload.bytes, origin, kind, relocatable, outContent.lines);
                outContent.disassembly = true;
            }
            else
            {
                outContent.bytes    = payload.bytes;
                outContent.origin   = origin;
                outContent.textFile = PreviewDecoder::LooksLikeText (payload.bytes);
            }

            break;

        case ViewChoice::HiRes:
        case ViewChoice::DoubleHiRes:
        case ViewChoice::LoRes:
            mode   = (view == ViewChoice::HiRes) ? PicturePreview::Mode::HiRes : (view == ViewChoice::LoRes) ? PicturePreview::Mode::LoRes : PicturePreview::Mode::DoubleHiRes;
            origin = (view == ViewChoice::LoRes) ? kLoResPage : kHiResPage;

            hr = PicturePreview::Render (mode, payload.bytes, origin, bus, outContent.bgra, outContent.width, outContent.height, monitor, customArgb);
            CHR (hr);

            outContent.kind          = PreviewContent::Kind::Picture;
            outContent.bytes         = payload.bytes;
            outContent.origin        = origin;
            outContent.pictureMode   = mode;
            outContent.pictureChosen = mode;
            break;

        case ViewChoice::ResourceFork:
            outContent.kind  = PreviewContent::Kind::Hex;
            outContent.bytes = payload.resourceFork;
            break;

        default:
            hr = HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
            break;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewChoices::SplitLines
//
//  Host text's lines, at CR, LF or CR LF.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewViewChoices::SplitLines (const std::string & text, std::vector<std::wstring> & outLines)
{
    std::string  line;



    outLines.clear();

    for (size_t at = 0; at < text.size(); at++)
    {
        char  ch = text[at];

        if (ch == '\r' || ch == '\n')
        {
            outLines.push_back (TextEncoding::NarrowToWide (line));
            line.clear();

            if (ch == '\r' && at + 1 < text.size() && text[at + 1] == '\n')
            {
                at++;
            }
        }
        else
        {
            line.push_back (ch);
        }
    }

    outLines.push_back (TextEncoding::NarrowToWide (line));
}
