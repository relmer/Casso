#pragma once

#include "Ui/Debugger/Panes/HeatMapBands.h"

class HeatMapCells;
class HeatMapGrid;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapPicker
//
//  The cell the mouse picks on the heat map. Small cells are hard to land
//  on, so over a cold one the pick is the nearest hot cell in the same row
//  within kSnapDip. At a zoom tall enough for an instruction's two lines, an
//  instruction is drawn as one cell across its bytes, and a pick anywhere in
//  it is its opcode.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapPicker
{
public:
    //  Below this side a cell is hard to land on, and over a cold cell the
    //  pick is the nearest hot cell in the same row within kSnapDip of the
    //  mouse.
    static constexpr int  kComfortCellDip = 8;
    static constexpr int  kSnapDip        = 4;

    //  An instruction drawn as one cell across its bytes: its opcode's place
    //  and address, and its length.
    struct Merged
    {
        HeatMapBands::Place  first;
        Word                 opcode = 0;
        int                  length = 0;
    };

    HeatMapPicker (const HeatMapGrid & grid, const HeatMapBands & bands, const HeatMapCells & model) :
        m_grid  (grid),
        m_bands (bands),
        m_model (model)
    {
    }

    std::optional<Word>                 GetPickAt      (POINT point) const;
    std::optional<HeatMapBands::Place>  GetPickPlaceAt (POINT point) const;
    std::optional<Merged>               GetMergedAt    (const HeatMapBands::Place & place) const;
    std::vector<RECT>                   GetMergedRects (const Merged & merged) const;

private:
    bool  IsMergeZoom    () const;
    int   GetMergeLength (const HeatMapBands::Place & place, Word address) const;

    const HeatMapGrid   & m_grid;
    const HeatMapBands  & m_bands;
    const HeatMapCells  & m_model;
};
