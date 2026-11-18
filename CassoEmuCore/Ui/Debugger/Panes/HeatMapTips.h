#pragma once

#include "Debugger/HeatAccessJump.h"
#include "Ui/Debugger/Panes/HeatMapCells.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapTips
//
//  The text of a heat map cell's tip: what the cell contains, what touched
//  it and how much, the overlays on it, the frame of its labeled range and
//  the tag for the PC or the stack pointer. The last writer and reader the
//  machine looked up for the hovered cell are separate lines.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapTips
{
public:
    //  The first line of a cell's tip: the label of what drew the cell, and
    //  the color the cell is drawn in.
    struct TipLead
    {
        std::wstring  meaning;
        uint32_t      argb = 0;
    };

    explicit HeatMapTips (const HeatMapCells & model) : m_model (model) {}

    //  The value at level 255, whether the machine has aux RAM for
    //  the tip's location, and the last writer and reader looked up for a
    //  cell; and whether the machine looks them up, which the tip then has
    //  lines for.
    void   SetTop         (double top)                                      { m_top = top; }
    void   SetHasAux      (bool hasAux)                                     { m_hasAux = hasAux; }
    void   SetHoverAccess (std::shared_ptr<const HeatAccessHover> hover)    { m_hoverAccess = std::move (hover); }
    void   SetLookingUp   (bool isLookingUp)                                { m_isLookingUp = isLookingUp; }

    std::wstring           GetTipText      (Word address) const;
    std::wstring           GetContentsLine (Word address) const;
    void                   GetTipTag       (Word address, std::wstring & tag, uint32_t & argb) const;
    void                   GetTipFrame     (Word address, std::wstring & label, uint32_t & argb) const;
    TipLead                GetTipLead      (Word address) const;
    bool                   HasHoverAccess  (Word address) const;

private:
    std::wstring  FormatAmount        (Byte level) const;
    std::wstring  DescribeMarks       (Word address) const;
    std::wstring  DescribeInstruction (Word address) const;

    const HeatMapCells                      & m_model;
    double                                    m_top         = 0.0;
    bool                                      m_hasAux      = false;
    std::shared_ptr<const HeatAccessHover>    m_hoverAccess;
    bool                                      m_isLookingUp = false;
};
