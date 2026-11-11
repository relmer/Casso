#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerPreviewOutput.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Model/CopyOutConversion.h"
#include "CassoExplorer/Model/DragPayload.h"
#include "CassoExplorer/Model/PicturePreview.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "Core/AppleSingleCodec.h"
#include "Core/TextEncoding.h"
#include "Seams/IHostDialogs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewOutput::TryGetSource
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewOutput::TryGetSource (ExplorerContext & context, Source & outSource)
{
    std::vector<FileEntry>  entries;



    outSource = Source();

    if (!context.browser.IsImageLocation())
    {
        return false;
    }

    context.browser.GetSelectedEntries (entries);

    if (entries.size() != 1 || entries[0].isDirectory)
    {
        return false;
    }

    outSource.image       = TextEncoding::WideToNarrow (context.browser.GetLocation().path);
    outSource.catalogPath = context.browser.GetEntryPath (entries[0]);
    outSource.entry       = entries[0];
    outSource.kind        = context.browser.GetVolumeKind();
    outSource.format      = CopyOutConversion::Find (context.browser.GetOperations(), outSource.image, outSource.catalogPath, outSource.entry, outSource.kind);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewOutput::CanSaveAs
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewOutput::CanSaveAs (const ExplorerContext & context)
{
    const PreviewContent &  preview  = context.browser.GetPreview();
    std::vector<FileEntry>  entries;
    bool                    viewable = (preview.kind == PreviewContent::Kind::Picture && !preview.bgra.empty())
                                    || !PreviewSaveChoices::GetViewText (preview).empty();



    context.browser.GetSelectedEntries (entries);

    return context.app.fs != nullptr && (viewable || (entries.size() == 1 && !entries[0].isDirectory));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewOutput::SaveAs
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewOutput::SaveAs (ExplorerContext & context)
{
    HRESULT                                   hr        = S_OK;
    Source                                    source;
    bool                                      inImage   = TryGetSource (context, source);
    bool                                      altered   = false;
    std::wstring                              converted;
    std::wstring                              stem      = L"File";
    std::vector<PreviewSaveChoices::Choice>   choices;
    FileDialogSpec                            spec;
    std::filesystem::path                     picked;
    bool                                      chosen    = false;
    const std::vector<int> &                  selected  = context.browser.GetSelectedRows();



    BAIL_OUT_IF (context.app.fs == nullptr, S_OK);

    if (inImage)
    {
        converted = DragPayload::GetHostName (source.entry, source.kind, context.commandManager.GetNamingStyle(), altered, source.format);
        stem      = std::filesystem::path (converted).stem().wstring();
    }
    else if (selected.size() == 1 && selected[0] >= 0 && (size_t) selected[0] < context.browser.GetRows().size())
    {
        stem = std::filesystem::path (context.browser.GetRows()[(size_t) selected[0]].name).stem().wstring();
    }

    PreviewSaveChoices::Build (context.browser.GetPreview(), converted, choices);

    //  A file not in an image has the view's types only.
    if (!inImage)
    {
        std::erase_if (choices, [] (const PreviewSaveChoices::Choice & choice)
        {
            return choice.kind != PreviewSaveChoices::Kind::ViewPicture && choice.kind != PreviewSaveChoices::Kind::ViewText;
        });
    }

    BAIL_OUT_IF (choices.empty(), S_OK);

    for (const PreviewSaveChoices::Choice & choice : choices)
    {
        spec.filters.push_back (FileDialogFilter { std::format (L"{} (*.{})", choice.label, choice.extension), L"*." + choice.extension });
    }

    spec.clientGuid       = s_kSaveAsGuid;
    spec.defaultFileName  = stem + L"." + choices[0].extension;
    spec.defaultExtension = choices[0].extension;
    spec.title            = L"Save as";

    hr = context.dialogs.PickFileToSave (context.window.GetHwnd(), spec, picked, chosen);
    CHR (hr);
    BAIL_OUT_IF (!chosen, S_OK);

    hr = WriteChoice (context, PreviewSaveChoices::Match (choices, picked), source, picked);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        context.window.ShowMessage (L"The file could not be saved.", MB_ICONERROR);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewOutput::WriteChoice
//
//  A picture at its original size, whatever the zoom; text as UTF-8; and the
//  others read from the image as Get reads them.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerPreviewOutput::WriteChoice (ExplorerContext & context, PreviewSaveChoices::Kind kind, const Source & source, const std::filesystem::path & path)
{
    HRESULT                   hr      = S_OK;
    const PreviewContent    & preview = context.browser.GetPreview();
    std::vector<Byte>         bytes;
    std::string               text;
    DiskOperations::Result    result;
    AppleSingleFile           single;
    bool                      read    = false;
    bool                      written = false;



    switch (kind)
    {
        case PreviewSaveChoices::Kind::ViewPicture:
            hr = PicturePreview::EncodePng (preview.bgra, preview.width, preview.height, bytes);
            CHR (hr);
            break;

        case PreviewSaveChoices::Kind::ViewText:
            text = TextEncoding::WideToUtf8 (PreviewSaveChoices::GetViewText (preview));
            bytes.assign (text.begin(), text.end());
            break;

        case PreviewSaveChoices::Kind::Converted:
            result = CopyOutConversion::GetToFile (context.browser.GetOperations(), *context.app.fs,
                                                   context.browser.GetDecodeOptions (TextEncoding::NarrowToWide (source.image), source.catalogPath),
                                                   source.image, source.catalogPath, source.entry, source.kind, source.format,
                                                   CassoExplorerActions::GetEncoding (source.entry, source.kind), path.wstring());
            read    = result.Succeeded();
            written = true;
            CBR (read);
            break;

        case PreviewSaveChoices::Kind::Original:
        case PreviewSaveChoices::Kind::AppleSingle:
            result = context.browser.GetOperations().Get (source.image, source.catalogPath, ActionsTypes::Encoding::Verbatim, "", source.entry.catalogIndex);
            read   = result.Succeeded();
            CBR (read);

            if (kind == PreviewSaveChoices::Kind::Original)
            {
                bytes = std::move (result.payload);
                break;
            }

            single                 = DragPayload::MakeAppleSingle (source.entry, source.kind);
            single.data            = std::move (result.payload);
            single.resourceFork    = std::move (result.resourceFork);
            single.finderInfo      = std::move (result.finderInfo);
            single.hasResourceFork = result.hasResourceFork;
            AppleSingleCodec::Encode (single, bytes);
            break;
    }

    BAIL_OUT_IF (written, S_OK);

    hr = context.app.fs->WriteAllText (path.wstring(), std::string (bytes.begin(), bytes.end()));
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewOutput::CanCopy
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerPreviewOutput::CanCopy (const ExplorerContext & context)
{
    const PreviewContent &  preview = context.browser.GetPreview();



    return (preview.kind == PreviewContent::Kind::Picture && !preview.bgra.empty()) || !PreviewSaveChoices::GetViewText (preview).empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewOutput::Copy
//
//  A document's RTF is Get's conversion of it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPreviewOutput::Copy (ExplorerContext & context)
{
    HRESULT                   hr      = S_OK;
    const PreviewContent    & preview = context.browser.GetPreview();
    Source                    source;
    std::vector<Byte>         rtf;
    std::wstring              failure;
    DiskOperations::Result    result;



    if (preview.kind == PreviewContent::Kind::Document && TryGetSource (context, source) && source.format != FileFormat::None)
    {
        result = CopyOutConversion::Get (context.browser.GetOperations(),
                                         context.browser.GetDecodeOptions (TextEncoding::NarrowToWide (source.image), source.catalogPath),
                                         source.image, source.catalogPath, source.entry, source.kind, source.format,
                                         CassoExplorerActions::GetEncoding (source.entry, source.kind), failure);

        if (result.Succeeded())
        {
            rtf = std::move (result.payload);
        }
    }

    hr = SetClipboard (context.window.GetHwnd(), PreviewSaveChoices::GetViewText (preview), rtf, preview);

    if (FAILED (hr))
    {
        context.window.ShowMessage (L"The preview could not be copied.", MB_ICONERROR);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewOutput::SetClipboard
//
//  Text, RTF when there is some, and a picture as a 32-bit DIB and a PNG.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerPreviewOutput::SetClipboard (HWND owner, const std::wstring & text, const std::vector<Byte> & rtf, const PreviewContent & preview)
{
    HRESULT             hr      = S_OK;
    BOOL                opened  = OpenClipboard (owner);
    BOOL                emptied = FALSE;
    bool                picture = preview.kind == PreviewContent::Kind::Picture && !preview.bgra.empty();
    std::vector<Byte>   dib;
    std::vector<Byte>   png;
    std::vector<Byte>   rtfText = rtf;



    CBREx (opened, HRESULT_FROM_WIN32 (GetLastError()));

    emptied = EmptyClipboard();
    CBREx (emptied, HRESULT_FROM_WIN32 (GetLastError()));

    if (!text.empty())
    {
        hr = PutGlobal (CF_UNICODETEXT, text.c_str(), (text.size() + 1) * sizeof (wchar_t));
        CHR (hr);
    }

    if (!rtfText.empty())
    {
        rtfText.push_back (0);

        hr = PutGlobal (RegisterClipboardFormatW (L"Rich Text Format"), rtfText.data(), rtfText.size());
        CHR (hr);
    }

    if (picture)
    {
        BITMAPINFOHEADER  header = {};
        size_t            pixels = preview.bgra.size() * sizeof (uint32_t);
        size_t            row    = (size_t) preview.width;

        header.biSize        = sizeof (header);
        header.biWidth       = preview.width;
        header.biHeight      = preview.height;    // bottom-up
        header.biPlanes      = 1;
        header.biBitCount    = 32;
        header.biCompression = BI_RGB;

        dib.resize (sizeof (header) + pixels);
        std::memcpy (dib.data(), &header, sizeof (header));

        for (int y = 0; y < preview.height; y++)
        {
            std::memcpy (dib.data() + sizeof (header) + (size_t) y * row * 4, preview.bgra.data() + (size_t) (preview.height - 1 - y) * row, row * 4);
        }

        hr = PutGlobal (CF_DIB, dib.data(), dib.size());
        CHR (hr);

        hr = PicturePreview::EncodePng (preview.bgra, preview.width, preview.height, png);
        CHR (hr);

        hr = PutGlobal (RegisterClipboardFormatW (L"PNG"), png.data(), png.size());
        CHR (hr);
    }

Error:
    if (opened)
    {
        CloseClipboard();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewOutput::PutGlobal
//
//  The clipboard owns the memory once SetClipboardData succeeds; until then
//  it is freed here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerPreviewOutput::PutGlobal (UINT format, const void * data, size_t bytes)
{
    HRESULT  hr     = S_OK;
    HGLOBAL  global = GlobalAlloc (GMEM_MOVEABLE, bytes);
    void   * locked = nullptr;
    HANDLE   placed = nullptr;



    CBREx (global != nullptr, E_OUTOFMEMORY);

    locked = GlobalLock (global);
    CBREx (locked != nullptr, E_OUTOFMEMORY);

    std::memcpy (locked, data, bytes);
    GlobalUnlock (global);

    placed = SetClipboardData (format, global);
    CBREx (placed != nullptr, HRESULT_FROM_WIN32 (GetLastError()));

    global = nullptr;

Error:
    if (global != nullptr)
    {
        GlobalFree (global);
    }

    return hr;
}
