#include "Pch.h"

#include "Ui/Debugger/Panes/SourceDocumentPanes.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/DroppedFiles.h"
#include "Ui/Debugger/Panes/DebuggerPaneFrame.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Debugger/Source/SourcePathList.h"

namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentPane::SourceDocumentPane
//
////////////////////////////////////////////////////////////////////////////////

SourceDocumentPane::SourceDocumentPane (IDebuggerPaneHost & host, SourceDocumentGroup & group, int slot) :
    IDebuggerPane (host),
    m_group       (group),
    m_slot        (slot),
    m_bar         (host, DebuggerLayout::GetSourcePaneId (slot), L"Source commands")
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentPane::~SourceDocumentPane
//
////////////////////////////////////////////////////////////////////////////////

SourceDocumentPane::~SourceDocumentPane() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentPane::GetId
//
////////////////////////////////////////////////////////////////////////////////

std::wstring SourceDocumentPane::GetId() const
{
    return DebuggerLayout::GetSourcePaneId (m_slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentPane::CreateControls
//
//  The text with its glyph margin for the breakpoints and the PC's arrow,
//  as a disassembly view has, and the banner over it.
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentPane::CreateControls()
{
    constexpr int   kBreakpointIconDip = 16;
    DxuiPanel     & parent             = m_host.GetControlParent();



    m_view   = parent.CreateChild<DxuiTextView>();
    m_banner = parent.CreateChild<DxuiActionBanner>();
    m_bar.Create();

    m_model = std::make_unique<SourcePane> (
        m_view, m_banner,
        [this] (const DebugSourceFile & record, const std::wstring & path, const std::string & key)
        {
            IDebuggerWindowHost  * host = m_host.GetWindowHost();

            return (host != nullptr) ? host->FindDebuggerSource (record, path, key) : SourceLookup();
        },
        [this] (const DebuggerActionBuilder & build) { m_host.RunAction (build (m_host.GetMode())); },
        [this] (Word address)                        { m_host.GoToCode (address); });

    m_model->SetOnToggleBody   ([this] { m_group.ToggleMacroBody(); });
    m_model->SetOnLoadSymbols  ([this] { m_group.LoadSymbolsFor (m_slot); });
    m_view->SetGutter          (DisassemblyCells::kGutterColumnDip, kBreakpointIconDip);
    m_view->SetGlyphCenterDip  (DisassemblyCells::kGlyphCenterDip);
    m_view->SetVisible         (false);
    m_banner->SetVisible       (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentPane::Configure
//
//  The toolbar has the search button.
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentPane::Configure()
{
    m_model->Configure (m_host.GetOwnerHwnd());

    m_bar.Configure();
    m_bar.GetBar()->SetEntries ({ m_host.GetFindEntry (GetId()) });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentPane::AddToDock
//
//  The toolbar, a place PlaceBar fills, and the banner over the text. Its
//  find widget floats over the text.
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentPane::AddToDock (DxuiDockSite & dock)
{
    m_frame = std::make_unique<DebuggerPaneFrame> (L"Source");
    m_frame->AddPart (m_bar.GetSlot(), PaneBarSlot::GetHeightPx);
    m_frame->AddPart (m_banner,
                      [this] (int width, const DxuiDpiScaler & scaler) { return (int) m_banner->GetPreferredHeightPx ((float) width, scaler); },
                      [this] { return m_isBannerShown; });
    m_frame->AddPart (m_view);

    dock.AddPane (GetId(), L"Source", m_frame.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentPane::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * SourceDocumentPane::GetContent() const
{
    return m_frame.get();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentPane::GetControls
//
////////////////////////////////////////////////////////////////////////////////

std::vector<IDxuiControl *> SourceDocumentPane::GetControls() const
{
    return { m_bar.GetSlot(), m_bar.GetBar(), m_banner, m_view };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentPane::GetColorKey
//
////////////////////////////////////////////////////////////////////////////////

std::optional<PaneColorKeySpec> SourceDocumentPane::GetColorKey() const
{
    return PaneColorKeySpec { m_bar.GetSlot(), ColorLegend::Pane::Source, nullptr };
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentPane::SetTextZoom
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentPane::SetTextZoom (float zoom)
{
    m_view->SetZoom (zoom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentPane::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool SourceDocumentPane::Contains (POINT atDip) const
{
    return m_isShown && m_view->IsVisible() &&
           (DxuiDockSite::Contains (m_view->GetBounds(), atDip) || DxuiDockSite::Contains (m_banner->GetBounds(), atDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentPane::ApplyDocument
//
//  The document the PC is in has the PC's own marker ahead of its title, as
//  the following disassembly view's tab does (FR-113).
//
////////////////////////////////////////////////////////////////////////////////

bool SourceDocumentPane::ApplyDocument (const DebuggerViewSnapshot & snapshot, int fileId, int macroLevel, const std::wstring & title)
{
    const std::optional<DebuggerViewSnapshot::SourceState>  & source        = snapshot.source;
    bool                                                      isShown       = false;
    bool                                                      isBannerShown = false;
    bool                                                      hasPc         = false;
    bool                                                      relayout      = false;
    std::wstring                                              bannerKey;
    DxuiTabGroup::LeadingMark                                 mark;



    m_model->SetStyle      ({ m_host.GetPcMarkerArgb(), m_host.GetPcRowArgb(), m_host.GetBreakpointIcon (true), m_host.GetBreakpointIcon (false),
                              m_host.GetSyntaxColors(), m_host.GetResultArgb(), m_host.GetPcMarker (nullptr) });
    m_model->SetFile       (fileId);
    m_model->SetMacroLevel (macroLevel);
    m_model->SetHeat       (m_host.IsCodeHeatOn() ? std::optional<HeatMapView::Palette> (m_host.GetHeatPalette()) : std::nullopt);
    m_model->Apply         (snapshot);

    if (title != m_title)
    {
        m_title = title;
        m_host.SetPaneTitle (GetId(), title);
    }

    bannerKey     = m_banner->GetText() + (m_banner->GetAction (0) != nullptr ? m_banner->GetAction (0)->GetAccessibleName() : L"");
    isShown       = m_model->IsActive();
    isBannerShown = isShown && m_model->HasBanner();

    if (isShown != m_isShown)
    {
        m_isShown       = isShown;
        m_isBannerShown = isBannerShown;
        m_bannerKey     = bannerKey;
        relayout        = true;
    }
    else if (isBannerShown != m_isBannerShown || bannerKey != m_bannerKey)
    {
        m_isBannerShown = isBannerShown;
        m_bannerKey     = bannerKey;
        m_frame->Relayout();
    }

    hasPc = isShown && source.has_value() && fileId == source->fileId;

    if (hasPc)
    {
        mark = DxuiTabGroup::LeadingMark { s_kpszTriangleRight, DxuiTheme::kMonoFace, m_host.GetPcMarkerArgb() };
    }

    m_host.SetPaneTabMark (GetId(), mark, hasPc ? L"The PC is in this file" : L"");
    return relayout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::SourceDocumentGroup
//
////////////////////////////////////////////////////////////////////////////////

SourceDocumentGroup::SourceDocumentGroup (IDebuggerPaneHost & host) :
    m_host (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::~SourceDocumentGroup
//
////////////////////////////////////////////////////////////////////////////////

SourceDocumentGroup::~SourceDocumentGroup() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::CreateDocuments
//
//  A document per source file, each visible only while it has one.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::unique_ptr<IDebuggerPane>> SourceDocumentGroup::CreateDocuments()
{
    std::vector<std::unique_ptr<IDebuggerPane>>  documents;



    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        std::unique_ptr<SourceDocumentPane>  document = std::make_unique<SourceDocumentPane> (m_host, *this, slot);

        m_documents.push_back (document.get());
        documents.push_back (std::move (document));
    }

    return documents;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::Apply
//
//  Each document's tab is titled with its file's name.
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentGroup::Apply()
{
    const DebuggerViewSnapshot                              & snapshot  = *m_host.GetLatestSnapshot();
    const std::optional<DebuggerViewSnapshot::SourceState>  & source    = snapshot.source;
    std::wstring                                              loadedFor;
    bool                                                      relayout  = false;



    if (source.has_value())
    {
        loadedFor = source->debugFilePath + SourcePathList::Utf8ToWide (source->programKey);
    }

    if (loadedFor != m_loadedFor)
    {
        m_loadedFor  = loadedFor;
        m_files.Clear();
        m_looseFiles.clear();
        m_pcPlace    = { -1, 0 };
        m_macroLevel = 0;
    }

    if (source.has_value())
    {
        m_macroLevel = SourcePane::GetMacroLevel (*source, m_macroLevel);

        Restore  (snapshot);
        FollowPc (snapshot);
    }

    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        int           fileId = m_files.GetFileId (slot);
        std::wstring  title  = L"Source";

        if (source.has_value())
        {
            for (const DebugSourceFile & record : source->files)
            {
                title = (record.id == fileId) ? SourcePathList::Utf8ToWide (record.name) : title;
            }
        }

        if (m_looseFiles.contains (fileId))
        {
            title = fs::path (m_looseFiles.at (fileId)).filename().wstring();
        }

        relayout = m_documents[(size_t) slot]->ApplyDocument (snapshot, fileId, m_macroLevel, title) || relayout;
    }

    if (relayout)
    {
        m_host.RelayoutDock();
    }

    //  A source opened before its debug file loaded, matched against it now.
    if (source.has_value() && !m_pendingLoose.empty())
    {
        std::wstring  pending = std::move (m_pendingLoose);

        m_pendingLoose.clear();
        ShowDropped (pending);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::FollowPc
//
//  At each stop at a new place, the file the PC is in comes to the front,
//  opened if it is not open yet -- the body's file while the body is on
//  screen.
//  A running machine moves nothing, so the documents do not churn under it.
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentGroup::FollowPc (const DebuggerViewSnapshot & snapshot)
{
    const DebuggerViewSnapshot::SourceState & source = *snapshot.source;
    std::pair<int, int>                       place  = SourcePane::GetPlace (source, SourcePane::GetMacroLevel (source, m_macroLevel));



    if (!snapshot.isPaused || place.first < 0 || place == m_pcPlace)
    {
        return;
    }

    m_pcPlace = place;
    Open (place.first, 0, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::Open
//
//  The PC's file is never the one closed to make room.
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentGroup::Open (int fileId, int line, bool activate)
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    int                           keep     = snapshot != nullptr && snapshot->source.has_value() ? snapshot->source->fileId : -1;
    int                           slot     = m_files.Open (fileId, keep);



    if (slot < 0)
    {
        return;
    }

    m_documents[(size_t) slot]->GetModel().SetFile (fileId);

    if (line > 0)
    {
        m_documents[(size_t) slot]->GetModel().SetTopSourceLine (line);
    }

    if (activate)
    {
        m_host.ActivatePaneTab (DebuggerLayout::GetSourcePaneId (slot));
        m_activeSlot = slot;
        m_host.RelayoutDock();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::Close
//
//  Only that file's document closes (FR-113). The PC's place is not reset,
//  so a document closed while the PC is in it stays closed until the next
//  stop somewhere else in that file opens it again.
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentGroup::Close (int slot)
{
    if (!m_files.IsOpen (slot))
    {
        return;
    }

    m_files.Close (slot);
    m_documents[(size_t) slot]->GetModel().SetFile (-1);

    if (m_host.GetLatestSnapshot() != nullptr)
    {
        Apply();
        m_host.SaveOpenViews();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::Restore
//
//  The documents open when the debugger last closed, once a debug file that
//  lists their files is loaded; until then they wait. A saved file that
//  is gone reopens with its not-found notice.
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentGroup::Restore (const DebuggerViewSnapshot & snapshot)
{
    const DebuggerViewSnapshot::SourceState & source = *snapshot.source;



    if (m_saved.empty() || source.files.empty())
    {
        return;
    }

    for (const SourceDocuments::Saved & saved : m_saved)
    {
        for (const DebugSourceFile & record : source.files)
        {
            if (record.name == saved.name)
            {
                Open (record.id, saved.line, false);
            }
        }
    }

    m_saved.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::ToggleMacroBody
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentGroup::ToggleMacroBody()
{
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();



    if (snapshot != nullptr && snapshot->source.has_value())
    {
        m_macroLevel = SourcePane::GetNextMacroLevel (m_macroLevel, SourcePane::GetMacroLevel (*snapshot->source, -1));
    }

    m_pcPlace = { -1, 0 };

    if (snapshot != nullptr && snapshot->source.has_value())
    {
        Apply();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::ShowLine
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentGroup::ShowLine (int fileId, int line)
{
    int  slot = m_files.Find (fileId);



    if (fileId < 0 || line <= 0)
    {
        return;
    }

    if (slot < 0)
    {
        Open (fileId, 0, false);
        Apply();
        slot = m_files.Find (fileId);
    }

    if (slot >= 0)
    {
        m_documents[(size_t) slot]->GetModel().ShowLine (fileId, line);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::GetSlotAt
//
//  The visible document whose text or banner is under a point, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int SourceDocumentGroup::GetSlotAt (POINT atDip) const
{
    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        if (m_documents[(size_t) slot]->Contains (atDip))
        {
            return slot;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::RouteMouse
//
//  A click moves the code pane to the line, a second click on the same
//  place within the double-click time toggles the line's breakpoint, and a
//  drag selects text as it does in any text view.
//
////////////////////////////////////////////////////////////////////////////////

bool SourceDocumentGroup::RouteMouse (const DxuiMouseEvent & ev)
{
    POINT                 at       = ev.positionDip;
    int                   slot     = GetSlotAt (at);
    uint64_t              now      = GetTickCount64();
    bool                  isDouble = false;
    bool                  inside   = false;
    bool                  onBanner = false;
    SourceDocumentPane  * document = nullptr;
    DxuiTextView        * view     = nullptr;
    DxuiActionBanner    * banner   = nullptr;



    //  A drag or a press on a scrollbar goes on to the document it began in,
    //  wherever the pointer goes.
    for (int each = 0; each < SourceDocuments::kMaxDocuments; each++)
    {
        if (m_documents[(size_t) each]->GetView()->IsInteracting())
        {
            slot = each;
        }
    }

    if (slot < 0)
    {
        return false;
    }

    document = m_documents[(size_t) slot];
    view     = document->GetView();
    banner   = document->GetBanner();

    inside   = DxuiDockSite::Contains (view->GetBounds(),   at);
    onBanner = DxuiDockSite::Contains (banner->GetBounds(), at);

    //  A document behind another tab takes no input.
    if (!document->IsShown() || !view->IsVisible() || !m_host.IsRoutable (view))
    {
        return false;
    }

    if (document->IsBannerShown() && (onBanner || ev.kind == DxuiMouseEventKind::Up || ev.kind == DxuiMouseEventKind::Move))
    {
        (void) banner->OnMouse (ev);

        //  An action changes the pane's text while the machine is paused,
        //  when no snapshot is coming with it.
        if (onBanner && ev.kind == DxuiMouseEventKind::Up && m_host.GetLatestSnapshot() != nullptr)
        {
            Apply();
        }

        if (onBanner && ev.kind == DxuiMouseEventKind::Down)
        {
            return true;
        }
    }

    if (view->IsInteracting() && ev.kind != DxuiMouseEventKind::Down)
    {
        (void) view->OnMouse (ev);
        return true;
    }

    if (!inside || (ev.kind != DxuiMouseEventKind::Down && ev.kind != DxuiMouseEventKind::Wheel))
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        isDouble = (now - m_clickMs) <= (uint64_t) GetDoubleClickTime() &&
                   std::abs (at.x - m_clickAt.x) <= GetSystemMetrics (SM_CXDOUBLECLK) &&
                   std::abs (at.y - m_clickAt.y) <= GetSystemMetrics (SM_CYDOUBLECLK);

        m_clickMs = isDouble ? 0 : now;
        m_clickAt = at;

        if (isDouble)
        {
            document->GetModel().OnDoubleClick (at);
        }
        else
        {
            document->GetModel().OnClick (at);
        }

        m_activeSlot = slot;
        m_host.FocusControl  (view);
        m_host.NoteViewFocus (true);
    }

    (void) view->OnMouse (ev);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::FollowMarkedLines
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentGroup::FollowMarkedLines()
{
    for (SourceDocumentPane * document : m_documents)
    {
        document->GetModel().FollowMarkedLine();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::FormatOpen
//
//  Each document at the line at its top (FR-113); one behind another tab,
//  with no top line, saves its last one. Documents waiting for their debug
//  file are in the text too, so a save before it loads includes them.
//
////////////////////////////////////////////////////////////////////////////////

std::string SourceDocumentGroup::FormatOpen (const std::function<std::string (int fileId)> & nameOf)
{
    for (int slot = 0; slot < SourceDocuments::kMaxDocuments; slot++)
    {
        int  line = m_documents[(size_t) slot]->GetModel().GetTopSourceLine();

        if (line > 0)
        {
            m_files.SetLine (slot, line);
        }
    }

    return m_files.Format (nameOf) + SourceDocuments::FormatSaved (m_saved);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::OpenSourcePath
//
//  With a debug file loaded, the source opens in the document for the record
//  it matches. With none, a debug or symbol file beside it is loaded first,
//  and the source is on screen meanwhile with no symbols; once that debug
//  file is in, the source is matched against it.
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentGroup::OpenSourcePath (const std::wstring & path)
{
    IDebuggerWindowHost         * host     = m_host.GetWindowHost();
    const DebuggerViewSnapshot  * snapshot = m_host.GetLatestSnapshot();
    SourceLookup                  lookup;
    int                           index    = -1;
    bool                          loaded   = snapshot != nullptr && snapshot->source.has_value();



    if (host == nullptr)
    {
        return;
    }

    if (loaded)
    {
        ShowDropped (path);
        return;
    }

    for (const std::wstring & beside : DroppedFiles::GetSymbolFilesBeside (path))
    {
        if (host->DoesDebuggerFileExist (beside))
        {
            m_host.RunAction (DebuggerActions::GetLoadSymbols (SourcePathList::WideToUtf8 (beside), m_host.GetMode()));
            m_pendingLoose = path;
            break;
        }
    }

    lookup = host->MatchDroppedDebuggerSource ({}, path, std::string(), index);
    OpenLooseFile (path, lookup.text, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::OpenLooseFile
//
//  In a document of its own titled with its name. The same file opened again
//  goes to the document it is in.
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentGroup::OpenLooseFile (const std::wstring & path, const std::string & text, bool isSource)
{
    int  fileId = -1;
    int  slot   = -1;



    for (const auto & [id, open] : m_looseFiles)
    {
        fileId = (_wcsicmp (open.c_str(), path.c_str()) == 0) ? id : fileId;
    }

    if (fileId < 0)
    {
        fileId = m_nextLooseId++;
        m_looseFiles[fileId] = path;
    }

    Open (fileId, 0, true);
    slot = m_files.Find (fileId);

    if (slot < 0)
    {
        return;
    }

    m_documents[(size_t) slot]->GetModel().ShowLoose (path, text, isSource);

    if (m_host.GetLatestSnapshot() != nullptr)
    {
        Apply();
    }

    m_host.UpdateWindowMenus();
    m_host.Repaint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::IsLoose
//
////////////////////////////////////////////////////////////////////////////////

bool SourceDocumentGroup::IsLoose (int slot) const
{
    return m_looseFiles.contains (m_files.GetFileId (slot));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::LoadSymbolsFor
//
//  The user picks the debug or symbol file, and the source is matched
//  against it once it loads.
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentGroup::LoadSymbolsFor (int slot)
{
    auto  found = m_looseFiles.find (m_files.GetFileId (slot));



    m_host.OpenSymbolFile (found != m_looseFiles.end() ? found->second : std::wstring());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocumentGroup::ShowDropped
//
//  The document for the record the file matches; a file that matches none
//  opens on its own, with a notice that it has no symbols.
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocumentGroup::ShowDropped (const std::wstring & path)
{
    const DebuggerViewSnapshot::SourceState & source = *m_host.GetLatestSnapshot()->source;
    SourceLookup                              lookup;
    int                                       index  = -1;



    lookup = m_host.GetWindowHost()->MatchDroppedDebuggerSource (source.files, path, source.programKey, index);

    if (index < 0 || index >= (int) source.files.size())
    {
        OpenLooseFile (path, lookup.text, true);
        return;
    }

    Open (source.files[(size_t) index].id, 0, true);

    m_documents[(size_t) m_activeSlot]->GetModel().ShowDropped (lookup, index);
    Apply();
}
