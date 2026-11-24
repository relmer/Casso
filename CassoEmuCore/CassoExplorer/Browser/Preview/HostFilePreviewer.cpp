#include "Pch.h"

#include "CassoExplorer/Browser/Preview/HostFilePreviewer.h"
#include "CassoExplorer/Browser/BrowserRowPaths.h"
#include "CassoExplorer/Browser/Preview/PictureMonitorChoice.h"
#include "CassoExplorer/Browser/Preview/PreviewViewState.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "CassoExplorer/Model/HostTextPreview.h"
#include "CassoExplorer/Model/PreviewViewChoices.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "Core/AppleSingleCodec.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
#include "Seams/IShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostFilePreviewer::HostFilePreviewer
//
////////////////////////////////////////////////////////////////////////////////

HostFilePreviewer::HostFilePreviewer (const BrowserParts & parts, const PreviewParts & previewParts)
    : m_parts        (parts),
      m_previewParts (previewParts)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFilePreviewer::ReadTextPreview
//
//  The host file read, for TryPreviewText. One that cannot be read stays the
//  hex preview, which reports it.
//
////////////////////////////////////////////////////////////////////////////////

void HostFilePreviewer::ReadTextPreview (const std::wstring & path, const std::wstring & name)
{
    std::string  content;
    HRESULT      hr      = m_parts.fs.ReadAllText (path, content);



    if (SUCCEEDED (hr))
    {
        HostTextPreview::TryPreviewText (name, std::span<const Byte> ((const Byte *) content.data(), content.size()), m_previewParts.preview);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFilePreviewer::TryPreviewAppleSingle
//
//  A host file that is an AppleSingle container previews as the file it
//  holds. Only a file small enough to be one is read to find out; anything
//  larger shows its bytes as before.
//
////////////////////////////////////////////////////////////////////////////////

bool HostFilePreviewer::TryPreviewAppleSingle (const std::wstring & folder, const CatalogRow & row)
{
    static constexpr uint64_t  kLargestContainer = 16 * 1024 * 1024;
    std::string                content;
    AppleSingleFile            file;
    std::string                error;
    std::span<const Byte>      bytes;
    HRESULT                    hr                = S_OK;



    if (row.sizeBytes > kLargestContainer)
    {
        return false;
    }

    hr = m_parts.fs.ReadAllText (BrowserRowPaths::JoinPath (folder, row.name), content);

    if (FAILED (hr))
    {
        return false;
    }

    bytes = std::span<const Byte> ((const Byte *) content.data(), content.size());

    if (!AppleSingleCodec::IsAppleSingle (bytes))
    {
        return false;
    }

    hr = AppleSingleCodec::Decode (bytes, file, error);

    if (FAILED (hr))
    {
        return false;
    }

    PreviewDecoder::RenderAppleSingle (file, m_previewParts.preview);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFilePreviewer::PreviewHostFile
//
//  A file exported from an Apple disk with its type in its name previews as
//  it does in its disk. An AppleSingle file previews as its details. Any
//  other file is displayed as its bytes, which the window reads from the
//  file as it draws them, through a Windows preview handler, or as text. A
//  file read whole for View as has the other views of its bytes as well.
//
////////////////////////////////////////////////////////////////////////////////

void HostFilePreviewer::PreviewHostFile (const std::wstring & folder, const CatalogRow & row)
{
    std::wstring             path    = BrowserRowPaths::JoinPath (folder, row.name);
    FilePayload              payload;
    FileEntry                entry;
    bool                     typed   = false;
    bool                     loaded  = false;
    PreviewDecoder::HexForm  hexForm = PreviewDecoder::HexForm::Auto;
    HRESULT                  hr      = S_OK;



    if (row.isDirectory)
    {
        return;
    }

    //  A Full Send's manifest is Casso's record of the folder, not a file
    //  to read here.
    if (_wcsicmp (row.name.c_str(), TextEncoding::NarrowToWide (FullSendManifest::kFileName).c_str()) == 0)
    {
        m_previewParts.preview.kind    = PreviewContent::Kind::Error;
        m_previewParts.preview.message = L"Casso's record of the disks a Full Send received in this folder. Don't edit, rename or move it: Casso reads it to pass over disks already received.";
        return;
    }

    loaded  = TryReadHostPayload (path, row, payload, entry, typed);
    hexForm = m_previewParts.viewState.GetHexForm (path);

    if (TryPreviewAppleSingle (folder, row))
    {
        AttachHostViews (entry, payload, loaded, hexForm);
        return;
    }

    if (typed)
    {
        hr = PreviewViewChoices::Render (m_previewParts.viewState.GetViewAs(), entry, VolumeKind::ProDos, payload, hexForm, m_previewParts.bus, m_previewParts.preview, m_previewParts.monitor.GetPreviewMonitor(), m_previewParts.monitor.GetCustomArgb(), DecodeOptions());

        if (SUCCEEDED (hr))
        {
            return;
        }

        m_previewParts.preview = PreviewContent();
    }

    m_previewParts.preview.kind     = PreviewContent::Kind::Hex;
    m_previewParts.preview.hostPath = path;

    //  A Windows preview handler, as in Explorer, for a file type with one;
    //  an assembler's source and an exported Apple file are Casso Explorer's
    //  to preview.
    if (m_parts.sources.shellVerbs != nullptr && !HostTextPreview::IsSourceName (row.name) && !HostTextPreview::IsAppleExportName (row.name) && m_parts.sources.shellVerbs->HasShellPreview (m_previewParts.preview.hostPath))
    {
        m_previewParts.preview.kind = PreviewContent::Kind::Shell;
    }
    else if (row.sizeBytes <= kTextPreviewMaxBytes && !PathResolver::IsNetworkPath (m_previewParts.preview.hostPath))
    {
        //  A source file, or text the hex view cannot draw, is read whole as
        //  text when it is small and on this computer.
        ReadTextPreview (m_previewParts.preview.hostPath, row.name);
    }

    AttachHostViews (entry, payload, loaded, hexForm);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFilePreviewer::TryReadHostPayload
//
//  A local file up to kTextPreviewMaxBytes, read whole: an AppleSingle
//  file's data and resource fork, or the file's bytes. `outTyped` is set for
//  a name with a ProDOS type and aux type in it, which the entry and payload
//  take.
//
////////////////////////////////////////////////////////////////////////////////

bool HostFilePreviewer::TryReadHostPayload (const std::wstring & path, const CatalogRow & row, FilePayload & outPayload, FileEntry & outEntry, bool & outTyped)
{
    std::string            content;
    std::span<const Byte>  bytes;
    AppleSingleFile        single;
    std::string            error;
    ParsedHostName         parsed;
    HRESULT                hr      = S_OK;
    bool                   decoded = false;



    outPayload = FilePayload();
    outEntry   = FileEntry();
    outTyped   = false;

    if (row.sizeBytes > kTextPreviewMaxBytes || PathResolver::IsNetworkPath (path))
    {
        return false;
    }

    hr = m_parts.fs.ReadAllText (path, content);

    if (FAILED (hr))
    {
        return false;
    }

    bytes   = std::span<const Byte> ((const Byte *) content.data(), content.size());
    hr      = AppleSingleCodec::IsAppleSingle (bytes) ? AppleSingleCodec::Decode (bytes, single, error) : E_FAIL;
    decoded = SUCCEEDED (hr);

    if (decoded)
    {
        outPayload.bytes        = std::move (single.data);
        outPayload.resourceFork = std::move (single.resourceFork);
        return true;
    }

    outPayload.bytes.assign (bytes.begin(), bytes.end());
    outEntry.name     = TextEncoding::WideToNarrow (row.name);
    outEntry.eofBytes = (uint32_t) outPayload.bytes.size();

    if (HostFileNaming::Parse (row.name, parsed) && parsed.hasType && parsed.converted == ParsedHostName::ConvertedKind::None)
    {
        outEntry.name           = parsed.catalogName;
        outEntry.type           = parsed.type;
        outEntry.auxType        = parsed.aux;
        outEntry.hasAuxType     = parsed.hasAux;
        outEntry.loadAddress    = parsed.aux;
        outEntry.hasLoadAddress = parsed.hasAux;
        outPayload.type           = parsed.type;
        outPayload.auxType        = parsed.aux;
        outPayload.hasAuxType     = parsed.hasAux;
        outPayload.loadAddress    = parsed.aux;
        outPayload.hasLoadAddress = parsed.hasAux;
        outTyped                  = true;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFilePreviewer::AttachHostViews
//
//  The other views of a host file read whole, and the chosen one in place
//  of the default when it applies.
//
////////////////////////////////////////////////////////////////////////////////

void HostFilePreviewer::AttachHostViews (const FileEntry & entry, const FilePayload & payload, bool loaded, PreviewDecoder::HexForm hexForm)
{
    std::vector<ViewChoice>  choices;
    std::wstring             defaultName;
    PreviewContent           chosen;
    HRESULT                  hr          = E_FAIL;
    bool                     applies     = false;



    if (!loaded)
    {
        return;
    }

    PreviewViewChoices::List (payload, m_previewParts.preview, choices);
    defaultName = PreviewViewChoices::DescribeDefault (m_previewParts.preview);
    applies     = std::find (choices.begin(), choices.end(), m_previewParts.viewState.GetViewAs()) != choices.end();

    if (applies)
    {
        hr = PreviewViewChoices::RenderAs (m_previewParts.viewState.GetViewAs(), entry, VolumeKind::ProDos, payload, hexForm, m_previewParts.bus, chosen, m_previewParts.monitor.GetPreviewMonitor(), m_previewParts.monitor.GetCustomArgb());
    }

    if (SUCCEEDED (hr))
    {
        m_previewParts.preview = std::move (chosen);
    }

    m_previewParts.preview.viewChoices = std::move (choices);
    m_previewParts.preview.viewAs      = SUCCEEDED (hr) ? m_previewParts.viewState.GetViewAs() : ViewChoice::Default;
    m_previewParts.preview.defaultView = std::move (defaultName);
}
