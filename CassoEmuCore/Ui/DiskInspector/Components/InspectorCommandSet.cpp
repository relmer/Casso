#include "Pch.h"

#include "Ui/DiskInspector/Components/ComparisonPart.h"
#include "Ui/DiskInspector/Components/EditingPart.h"
#include "Ui/DiskInspector/Components/FindPart.h"
#include "Ui/DiskInspector/Components/HostLink.h"
#include "Ui/DiskInspector/Components/InspectorCommandSet.h"
#include "Ui/DiskInspector/Components/InspectorDialogs.h"
#include "Ui/DiskInspector/Components/InspectorSession.h"
#include "Ui/DiskInspector/Components/InspectorToolbar.h"
#include "Ui/DiskInspector/Components/PlatterCorners.h"
#include "Ui/DiskInspector/Components/SelectionPart.h"
#include "Ui/DiskInspector/Components/TrackColumn.h"
#include "Ui/DiskInspector/PlatterCells.h"
#include "Ui/DiskInspector/PlatterView.h"
#include "Ui/DiskInspector/TrackStripView.h"
#include "Ui/DiskInspector/Views/FilesView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorCommandSet::InspectorCommandSet
//
////////////////////////////////////////////////////////////////////////////////

InspectorCommandSet::InspectorCommandSet (InspectorSession & session, IInspectorComponentHost & host, IInspectorFrame & frame, IInspectorParts & parts, InspectorCommands & commands) :
    m_session  (session),
    m_host     (host),
    m_frame    (frame),
    m_parts    (parts),
    m_commands (commands)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorCommandSet::Build
//
//  For the menus, the toolbar and the keys (FR-158).
//
////////////////////////////////////////////////////////////////////////////////

void InspectorCommandSet::Build()
{
    static constexpr LPCWSTR  kpszGlyphCopy     = L"\xE8C8";
    static constexpr LPCWSTR  kpszGlyphEdit     = L"\xE70F";
    static constexpr LPCWSTR  kpszGlyphFind     = L"\xE721";
    static constexpr LPCWSTR  kpszGlyphGoTo     = L"\xE8AD";
    static constexpr LPCWSTR  kpszGlyphFollow   = L"\xE7B3";
    static constexpr LPCWSTR  kpszGlyphExport   = L"\xE72D";
    static constexpr LPCWSTR  kpszGlyphCompare  = L"\xE8AB";
    static constexpr LPCWSTR  kpszGlyphSettings = L"\xE713";
    static constexpr int      kPerTrack         = DiskImage::kQuarterTracksPerWholeTrack;



    InspectorCommands &  c          = m_commands;
    bool                 hasDrives  = m_session.host->GetDriveCount() > 0;
    auto                 hasDisk    = [this] () { return m_session.context.hasDisk; };
    auto                 isComparing = [this] () { return m_session.comparison.IsComparing(); };

    auto  make = [] (LPCWSTR label, LPCWSTR accelerator, std::function<void()> dispatch)
    {
        auto  command = std::make_shared<DxuiCommand>();

        command->label       = label;
        command->accelerator = accelerator;
        command->dispatch    = std::move (dispatch);
        return command;
    };

    auto  toolbarCommand = [] (const InspectorCommands::Command & command, LPCWSTR shortLabel, LPCWSTR glyph, LPCWSTR tip)
    {
        command->shortLabel     = shortLabel;
        command->glyph          = glyph;
        command->tip            = tip;
        command->isGlyphInMenus = true;
    };

    auto  selectTrack = [this] (int quarterTrack) { m_session.model.SelectQuarterTrack (quarterTrack); m_host.OnSelection(); };

    //  File
    c.exportFile                = make (L"&Export...", L"", [this] () { m_parts.GetDialogs().OpenExport(); });
    c.exportFile->isEnabled     = hasDisk;
    c.exportFile->glyph         = kpszGlyphExport;
    c.save                      = make (L"&Save...", L"Ctrl+S", [this] () { m_parts.GetEditing().ApplyEdits(); });
    c.save->isEnabled           = [this] () { return m_parts.GetEditing().GetState().canApply && m_session.edits.GetCounts().sectors > 0; };
    c.saveEditedCopy            = make (L"Save &as...", L"", [this] () { m_parts.GetEditing().SaveEditedCopy(); });
    c.saveEditedCopy->isEnabled = [this] () { return m_session.edits.GetCounts().sectors > 0; };
    c.close                     = make (L"&Close", L"Alt+F4", [this] () { m_frame.CloseWindow(); });

    //  Edit
    c.undoApplied               = make (L"&Undo save", L"Ctrl+Z", [this] () { m_parts.GetEditing().Undo(); });
    c.undoApplied->isEnabled    = [this] () { return m_session.edits.CanUndo() || m_parts.GetEditing().GetState().canUndoApplied; };
    c.undoApplied->labelText    = [this] () { return std::wstring (m_session.edits.CanUndo() ? L"&Undo" : L"&Undo save"); };
    c.redoApplied               = make (L"&Redo save", L"Ctrl+Y", [this] () { m_parts.GetEditing().Redo(); });
    c.redoApplied->isEnabled    = [this] () { return m_session.edits.CanRedo() || m_parts.GetEditing().GetState().canRedoApplied; };
    c.redoApplied->labelText    = [this] () { return std::wstring (m_session.edits.CanRedo() ? L"&Redo" : L"&Redo save"); };
    c.copy                      = make (L"&Copy", L"Ctrl+C", [this] () { m_parts.GetSelection().Copy(); });
    c.copy->isEnabled           = hasDisk;
    c.copySector                = make (L"Copy &sector", L"Ctrl+Shift+C", [this] () { m_parts.GetSelection().CopySector(); });
    c.copySector->isEnabled     = [this] () { return m_session.model.GetSector() != nullptr && m_session.model.GetSector()->dataField >= 0; };
    c.copySector->glyph         = kpszGlyphCopy;
    c.copySector->tip           = L"Copy the whole sector as a hex dump (Ctrl+Shift+C)";
    c.copySector->isGlyphInMenus = true;
    c.copyMap                   = make (L"Copy &map", L"", [this] () { m_parts.GetFiles().CopyFileMap(); });
    c.copyMap->isEnabled        = [this] () { return m_parts.GetFiles().HasMap(); };
    c.selectAll                 = make (L"Select &all", L"Ctrl+A", [this] () { m_parts.GetSelection().SelectAll(); });
    c.selectAll->isEnabled      = hasDisk;
    c.editSector                = make (L"&Edit sector", L"F2", [this] () { m_parts.GetEditing().ToggleEditing(); });
    c.editSector->isEnabled     = [this] () { return m_parts.GetEditing().GetState().canEdit; };
    c.editSector->isChecked     = [this] () { return m_parts.GetEditing().GetState().isEditing; };
    c.editSector->glyph         = kpszGlyphEdit;
    c.discardEdits              = make (L"&Discard edits", L"", [this] () { m_session.edits.DiscardAll(); });
    c.discardEdits->isEnabled   = [this] () { return m_session.edits.GetCounts().sectors > 0; };
    c.find                      = make (L"&Find...", L"Ctrl+F", [this] () { m_session.isFindOpen ? m_parts.GetFind().CloseFindBar() : m_parts.GetFind().OpenFindBar(); });
    c.find->isEnabled           = hasDisk;
    c.find->isChecked           = [this] () { return m_session.isFindOpen; };
    c.findNext                  = make (L"Find &next", L"F3", [this] () { m_parts.GetFind().StepFind (1); });
    c.findNext->isEnabled       = [this] () { return !m_session.find.IsEmpty(); };
    c.findPrevious              = make (L"Find pre&vious", L"Shift+F3", [this] () { m_parts.GetFind().StepFind (-1); });
    c.findPrevious->isEnabled   = [this] () { return !m_session.find.IsEmpty(); };
    toolbarCommand (c.find, L"Find", kpszGlyphFind, L"Find nibbles, bytes or text (Ctrl+F)");

    //  View
    c.structure                 = make (L"&Structure", L"", [this] () { m_parts.GetToolbar().SetTimingMode (false); });
    c.structure->isEnabled      = hasDisk;
    c.structure->isChecked      = [this] () { return !m_session.context.isTimingMode; };
    c.timing                    = make (L"&Timing", L"", [this] () { m_parts.GetToolbar().SetTimingMode (true); });
    c.timing->isEnabled         = [this] () { return m_session.context.hasDisk && m_parts.GetToolbar().HasTiming(); };
    c.timing->isChecked         = [this] () { return m_session.context.isTimingMode; };
    c.widerRange                = make (L"&Wider timing range", L"", [this] () { m_parts.GetToolbar().StepRange (1); });
    c.widerRange->isEnabled     = [this] () { return m_session.context.isTimingMode && m_parts.GetToolbar().CanStepRange (1); };
    c.narrowerRange             = make (L"&Narrower timing range", L"", [this] () { m_parts.GetToolbar().StepRange (-1); });
    c.narrowerRange->isEnabled  = [this] () { return m_session.context.isTimingMode && m_parts.GetToolbar().CanStepRange (-1); };
    c.zoomIn                    = make (L"Zoom &in", L"+", [this] () { m_session.views.platter->StepZoom (1); });
    c.zoomIn->isEnabled         = hasDisk;
    c.zoomOut                   = make (L"Zoom &out", L"-", [this] () { m_session.views.platter->StepZoom (-1); });
    c.zoomOut->isEnabled        = [this] () { return m_session.context.hasDisk && !m_session.model.IsAtFit(); };
    c.fit                       = make (L"&Fit", L"0", [this] () { m_session.model.Fit(); });
    c.fit->isEnabled            = [this] () { return m_session.context.hasDisk && !m_session.model.IsAtFit(); };
    c.flat                      = make (L"F&lat", L"", [this] () { m_session.model.SetTilt (0.0); m_session.model.RefitSelection(); });
    c.flat->isEnabled           = hasDisk;
    c.flat->isChecked           = [this] () { return !m_session.model.IsTilted(); };
    c.tilted                    = make (L"Tilte&d", L"", [this] () { m_session.model.SetTilt (m_parts.GetCorners().GetLastTilt()); m_session.model.RefitSelection(); });
    c.tilted->isEnabled         = hasDisk;
    c.tilted->isChecked         = [this] () { return m_session.model.IsTilted(); };
    c.legend                    = make (L"Le&gend", L"", [this] () { m_parts.GetCorners().ShowLegend (true); });
    c.legend->isEnabled         = hasDisk;

    auto  overlay = [this, &c, &make] (LPCWSTR label, PlatterOverlay which, std::function<bool()> isEnabled)
    {
        c.overlays.push_back (make (label, L"", [this, which] () { m_parts.GetCorners().SetOverlay (which, !m_parts.GetCorners().IsOverlayOn (which)); }));
        c.overlays.back()->isChecked = [this, which] () { return m_parts.GetCorners().IsOverlayOn (which); };
        c.overlays.back()->isEnabled = std::move (isEnabled);
    };

    overlay (L"F&iles", PlatterOverlay::Files, hasDisk);

    if (hasDrives)
    {
        overlay (L"&Reads", PlatterOverlay::Reads, [this] () { return m_session.context.hasDisk && m_session.IsWindowDiskA(); });
        overlay (L"&Writes", PlatterOverlay::Writes, [this] () { return m_session.context.hasDisk && m_session.IsWindowDiskA(); });
        overlay (L"S&cans",  PlatterOverlay::Scans,  [this] () { return m_session.context.hasDisk && m_session.IsWindowDiskA(); });
    }

    overlay (L"Track &alignment", PlatterOverlay::Alignment, hasDisk);
    overlay (L"Diff&erences", PlatterOverlay::Differences, [this] () { return m_session.context.hasDisk && m_session.comparison.IsComparing(); });

    //  Each view, available while the view selector offers it (FR-160).
    for (const TrackColumn::View & view : TrackColumn::GetAllViews())
    {
        auto  isShown = [this, view] ()
        {
            vector<TrackColumn::View>  shown = m_parts.GetTrackColumn().GetShownViews();

            return m_session.context.hasDisk && std::any_of (shown.begin(), shown.end(), [&view] (const TrackColumn::View & s) { return s.isDisk == view.isDisk && s.tab == view.tab; });
        };

        c.views.push_back (make (view.label, L"", [this, view] () { if (view.isDisk) { m_host.ShowDiskTab (view.tab); } else { m_host.ShowTrackTab (view.tab); } }));
        c.views.back()->isEnabled = isShown;
        c.views.back()->isChecked = [this, view] () { return m_session.isDiskGroup == view.isDisk && (view.isDisk ? m_session.diskTab : m_session.trackTab) == view.tab; };
    }

    //  Go
    c.back                          = make (L"&Back", L"Alt+Left", [this] () { m_parts.GetSelection().StepHistory (false); });
    c.back->isEnabled               = [this] () { return m_session.model.GetHistory().CanGoBack(); };
    c.forward                       = make (L"For&ward", L"Alt+Right", [this] () { m_parts.GetSelection().StepHistory (true); });
    c.forward->isEnabled            = [this] () { return m_session.model.GetHistory().CanGoForward(); };
    c.goTo                          = make (L"&Go to...", L"Ctrl+G", [this] () { m_parts.GetDialogs().OpenGoTo(); });
    c.goTo->isEnabled               = hasDisk;
    c.previousSector                = make (L"Previous &sector", L"Left", [this] () { m_parts.GetSelection().StepSector (-1, false); });
    c.previousSector->isEnabled     = hasDisk;
    c.nextSector                    = make (L"&Next sector", L"Right", [this] () { m_parts.GetSelection().StepSector (1, false); });
    c.nextSector->isEnabled         = hasDisk;
    c.previousQuarterTrack          = make (L"Previous &quarter track", L"Up", [selectTrack, this] () { selectTrack (m_session.model.GetQuarterTrack() - 1); });
    c.previousQuarterTrack->isEnabled = hasDisk;
    c.nextQuarterTrack              = make (L"Next q&uarter track", L"Down", [selectTrack, this] () { selectTrack (m_session.model.GetQuarterTrack() + 1); });
    c.nextQuarterTrack->isEnabled   = hasDisk;
    c.previousTrack                 = make (L"Previous &track", L"PgUp", [selectTrack, this] () { selectTrack ((m_session.model.GetQuarterTrack() - 1) / kPerTrack * kPerTrack); });
    c.previousTrack->isEnabled      = hasDisk;
    c.nextTrack                     = make (L"Next t&rack", L"PgDn", [selectTrack, this] () { selectTrack ((m_session.model.GetQuarterTrack() / kPerTrack + 1) * kPerTrack); });
    c.nextTrack->isEnabled          = hasDisk;
    c.previousDifference            = make (L"Previous &difference", L"", [this] () { m_parts.GetComparison().StepDifference (-1); });
    c.previousDifference->isEnabled = [this] () { return m_session.comparison.IsComparing() && !m_session.comparison.GetListed().empty(); };
    c.nextDifference                = make (L"Next diff&erence", L"", [this] () { m_parts.GetComparison().StepDifference (1); });
    c.nextDifference->isEnabled     = [this] () { return m_session.comparison.IsComparing() && !m_session.comparison.GetListed().empty(); };
    c.nextPane                      = make (L"Next &pane", L"F6", [this] () { m_parts.GetSelection().StepKeyTarget (1); });
    c.nextPane->isEnabled           = hasDisk;
    toolbarCommand (c.goTo, L"Go to", kpszGlyphGoTo, L"Go to a track, sector, block, nibble or cell (Ctrl+G)");

    if (hasDrives)
    {
        c.goToHead                  = make (L"Go to &head", L"", [this] () { m_parts.GetHostLink().GoToHead(); });
        c.goToHead->isEnabled       = [this] () { return m_parts.GetHostLink().IsHeadShown() && m_session.headState->hasDisk; };
        c.followHead                = make (L"&Follow head", L"", [this] () { m_parts.GetHostLink().SetFollowHead (!m_parts.GetHostLink().IsFollowingHead()); });
        c.followHead->isEnabled     = [this] () { return m_parts.GetHostLink().IsHeadShown(); };
        c.followHead->isChecked     = [this] () { return m_parts.GetHostLink().IsFollowingHead(); };
        toolbarCommand (c.followHead, L"Follow head", kpszGlyphFollow, L"Follow head: select each track the head settles on. Any other selection turns this off.");
    }

    //  Disk
    c.compareWith              = make (L"&Compare with...", L"", [this] () { m_parts.GetComparison().OpenCompare(); });
    c.compareWith->isEnabled    = [this] () { return m_session.context.hasDisk || m_session.comparison.IsComparing(); };
    c.compareWith->glyph        = kpszGlyphCompare;
    c.swapSides                 = make (L"&Swap A and B", L"", [this] () { m_parts.GetComparison().SwapSides(); });
    c.swapSides->isEnabled      = [this] () { return m_session.comparison.IsComparing() && m_session.context.hasDisk && m_session.comparison.GetB().copy != nullptr; };
    c.bSettings                 = make (L"Decode settings for &B...", L"", [this] () { m_parts.GetComparison().OpenBSettings(); });
    c.bSettings->isEnabled      = [this] () { return m_session.comparison.IsComparing() && m_session.comparison.GetB().copy != nullptr; };
    c.endComparison             = make (L"&End comparison", L"", [this] () { m_parts.GetComparison().StopComparing(); });
    c.endComparison->isEnabled  = isComparing;
    c.decodeSettings            = make (L"&Decode settings...", L"", [this] () { m_parts.GetDialogs().OpenDecodeSettings(); });
    c.decodeSettings->isEnabled = hasDisk;
    c.decodeSettings->glyph     = kpszGlyphSettings;
    c.repair                    = make (L"&Repair disk image...", L"", [this] () { m_host.OpenRepair ({}); });
    c.repair->isEnabled         = [this] () { return m_session.repairPlan.canRepair; };

    //  Window
    c.windowZoomIn           = make (L"Zoom &in", L"Ctrl++", [this] () { m_host.SetWindowZoom (DxuiZoomControl::StepZoom (m_frame.GetWindowZoom(), 1)); });
    c.windowZoomOut          = make (L"Zoom &out", L"Ctrl+-", [this] () { m_host.SetWindowZoom (DxuiZoomControl::StepZoom (m_frame.GetWindowZoom(), -1)); });
    c.actualSize             = make (L"&Actual size", L"Ctrl+0", [this] () { m_host.SetWindowZoom (1.0); });
    c.defaultSize            = make (L"&Default size", L"", [this] () { m_frame.RestoreDefaultSize(); });

    //  Help
    c.walkthrough = make (L"&Walkthrough", L"", [this] () { m_frame.HideTooltip(); m_host.OpenWalkthrough(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorCommandSet::Run
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorCommandSet::Run (const InspectorCommands::Command & command)
{
    bool  isRun = command != nullptr && command->dispatch && command->IsEnabled();



    if (isRun)
    {
        command->dispatch();
    }

    return isRun;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorCommandSet::RunKey
//
//  Ctrl with 0, + or - zooms the whole window; Ctrl+S saves; outside
//  editing, Ctrl+Z and Ctrl+Y undo and redo the last save; Alt+Left and
//  Alt+Right step through the selections; F2 edits. With a disk, the
//  platter's, the strip's and the selection's keys.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorCommandSet::RunKey (const DxuiKeyEvent & ev)
{
    static constexpr int     kPerTrack = DiskImage::kQuarterTracksPerWholeTrack;
    static constexpr double  kPanStep  = 0.1;



    bool  isHandled = false;
    int   qt        = m_session.model.GetQuarterTrack();



    if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && IsZoomKey (ev.vk))
    {
        //  Ctrl with 0, + or - zooms the whole window (FR-154).
        m_host.SetWindowZoom ((ev.vk == '0' || ev.vk == VK_NUMPAD0) ? 1.0 : DxuiZoomControl::StepZoom (m_frame.GetWindowZoom(), (ev.vk == VK_OEM_PLUS || ev.vk == VK_ADD) ? 1 : -1));
        isHandled = true;
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && !ev.shift && ev.vk == 'S')
    {
        isHandled = Run (m_commands.save);
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.ctrl && !ev.alt && !ev.shift && (ev.vk == 'Z' || ev.vk == 'Y'))
    {
        //  Outside editing, Ctrl+Z and Ctrl+Y undo and redo the last save
        //  (FR-107, FR-158).
        isHandled = Run (ev.vk == 'Z' ? m_commands.undoApplied : m_commands.redoApplied);
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.alt && !ev.ctrl && !ev.shift && (ev.vk == VK_LEFT || ev.vk == VK_RIGHT))
    {
        //  Alt+Left and Alt+Right step through the selections (FR-164).
        isHandled = Run (ev.vk == VK_LEFT ? m_commands.back : m_commands.forward);
    }
    else if (ev.kind == DxuiKeyEventKind::Down && ev.vk == VK_F2 && !ev.ctrl && !ev.alt && !ev.shift)
    {
        isHandled = Run (m_commands.editSector);
    }
    else if (ev.kind == DxuiKeyEventKind::Down && !ev.alt && m_session.context.hasDisk)
    {
        isHandled = true;

        if (ev.ctrl)
        {
            switch (ev.vk)
            {
                case VK_LEFT:  m_session.model.PanBy ({  kPanStep, 0.0 }); break;
                case VK_RIGHT: m_session.model.PanBy ({ -kPanStep, 0.0 }); break;
                case VK_UP:    m_session.model.PanBy ({ 0.0,  kPanStep }); break;
                case VK_DOWN:  m_session.model.PanBy ({ 0.0, -kPanStep }); break;
                case 'A':      m_parts.GetSelection().SelectAll();                          break;
                case 'C':      if (ev.shift) { m_parts.GetSelection().CopySector(); } else { m_parts.GetSelection().Copy(); } break;
                case 'F':      m_parts.GetFind().OpenFindBar();            break;
                case 'G':      m_parts.GetDialogs().OpenGoTo();                           break;
                default:       isHandled = false;                    break;
            }
        }
        else if (ev.vk == VK_ESCAPE && !ev.shift)
        {
            m_session.model.ClearSelection();
            m_host.OnSelection();
        }
        else if (ev.shift && (m_session.keyTarget == KeyTarget::Nibbles || m_session.keyTarget == KeyTarget::SectorData) &&
                 (ev.vk == VK_LEFT || ev.vk == VK_RIGHT || ev.vk == VK_UP || ev.vk == VK_DOWN))
        {
            m_parts.GetSelection().ExtendSelection (ev.vk);
        }
        else if (m_session.keyTarget == KeyTarget::Strip && SelectionPart::IsStripKey (ev.vk))
        {
            switch (ev.vk)
            {
                case VK_OEM_PLUS:  case VK_ADD:      m_session.views.strip->StepZoom (1);  break;
                case VK_OEM_MINUS: case VK_SUBTRACT: m_session.views.strip->StepZoom (-1); break;
                case VK_LEFT:                        m_session.views.strip->PanBy (-kPanStep);               break;
                case VK_RIGHT:                       m_session.views.strip->PanBy (kPanStep);                break;
                default:                             m_session.views.strip->ShowWholeTrack();                  break;
            }
        }
        else
        {
            switch (ev.vk)
            {
                case VK_OEM_PLUS:  case VK_ADD:      m_session.views.platter->StepZoom (1);  break;
                case VK_OEM_MINUS: case VK_SUBTRACT: m_session.views.platter->StepZoom (-1); break;
                case '0':          case VK_NUMPAD0:  m_session.model.Fit();                                    break;
                case VK_UP:    m_session.model.SelectQuarterTrack (StepRecording (qt, -1));           m_host.OnSelection(); break;
                case VK_DOWN:  m_session.model.SelectQuarterTrack (StepRecording (qt, 1));            m_host.OnSelection(); break;
                case VK_PRIOR: m_session.model.SelectQuarterTrack ((qt - 1) / kPerTrack * kPerTrack); m_host.OnSelection(); break;
                case VK_NEXT:  m_session.model.SelectQuarterTrack ((qt / kPerTrack + 1) * kPerTrack); m_host.OnSelection(); break;
                case VK_LEFT:  m_parts.GetSelection().StepSector (-1, false); break;
                case VK_RIGHT: m_parts.GetSelection().StepSector ( 1, false); break;
                case VK_HOME:  m_parts.GetSelection().StepSector (-1, true);  break;
                case VK_F3:    m_parts.GetFind().StepFind (ev.shift ? -1 : 1); break;
                case VK_F6:    m_parts.GetSelection().StepKeyTarget (ev.shift ? -1 : 1); break;
                case VK_END:   m_parts.GetSelection().StepSector ( 1, true);  break;
                default:       isHandled = false;      break;
            }
        }
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorCommandSet::IsZoomKey
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorCommandSet::IsZoomKey (WPARAM vk)
{
    return vk == '0' || vk == VK_NUMPAD0 || vk == VK_OEM_PLUS || vk == VK_ADD || vk == VK_OEM_MINUS || vk == VK_SUBTRACT;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorCommandSet::StepRecording
//
//  Up and Down go to the next recording's center ring, past rings that repeat
//  a recording and rings with none.
//
////////////////////////////////////////////////////////////////////////////////

int InspectorCommandSet::StepRecording (int quarterTrack, int step) const
{
    int  next = std::clamp (quarterTrack + step, 0, DiskImage::kQuarterTrackCount - 1);



    if (m_session.analysis.copy != nullptr)
    {
        next = PlatterCells::StepRecording (m_session.analysis.copy->playedSlot, quarterTrack, step);
    }

    return next;
}
