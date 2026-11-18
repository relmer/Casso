#pragma once

#include "Ui/Debugger/BranchArrow.h"

class DisassemblyGroup;
class IDebuggerPaneHost;
struct DebuggerPaneParts;
class DebuggerPaneLook;
class LatestSnapshot;
class IDxuiPainter;
class SourceDocumentGroup;
class SourcePane;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyBranchArrows
//
//  The PC's branch arrow over the disassembly views and the source
//  documents: where it runs, its painting, a click on it to go where it
//  points and back, and its tip.
//
////////////////////////////////////////////////////////////////////////////////

class DisassemblyBranchArrows
{
public:
    DisassemblyBranchArrows (const DebuggerPaneParts & parts, DisassemblyGroup & group, SourceDocumentGroup & sources);

    //  The arrow in a view, a view past the code views being a source
    //  document's; drawn, clicked, and its tip.
    bool  GetBranchArrow   (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken, bool isBranchedFrom = false) const;
    void  PaintBranchArrow (IDxuiPainter & painter, int view);
    bool  ClickBranchArrow (POINT pointPx);
    bool  TryGetBranchTip  (POINT clientPx, RECT & anchor, std::wstring & text) const;

    //  Whether a point is on a code view's glyph margin or the lanes its
    //  branch arrows run in, left of the code, in a view or a source
    //  document, where the pointer is an arrow rather than the I-beam.
    bool  IsOverArrowsOrGutter (POINT clientPx) const;

private:
    bool  GetOneBranchArrow (int view, BranchArrow::Input & input, Word & goesTo, bool & isTaken, bool isBranchedFrom) const;
    void  ClickSourceArrow  (SourcePane & document);

    IDebuggerPaneHost       & m_host;
    const LatestSnapshot    & m_latest;
    const DebuggerPaneLook  & m_look;
    DisassemblyGroup        & m_group;
    SourceDocumentGroup     & m_sources;
    bool                      m_isArrowAtTarget = false;
};
