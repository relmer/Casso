#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshAnchor
//
//  Where a list or a tree should be looking, and what it should have selected,
//  after its rows are re-read and some have come, gone or moved.
//
//  ROWS ARE MATCHED BY KEY, NEVER BY INDEX. A file added above the view moves
//  every row below it down one, so keeping the top row's index would show a
//  different file there. Keeping the row's key keeps the file.
//
//  WHERE THE VIEW LANDS, in order of preference:
//
//    1. The focused item, if it was on screen and still exists, keeps the
//       screen row it was on -- when that is reachable. A top row outside the
//       valid range is not reachable, and falls through rather than clamping,
//       because a clamped position is not the same position.
//
//    2. A view scrolled to its end keeps its bottom item on the bottom row.
//
//    3. Otherwise the top item keeps the top row.
//
//  An anchor that no longer exists is replaced by its nearest survivor:
//  forward from the old top, backward from the old bottom. With nothing left
//  to anchor to, the old top row is kept as far as the new rows allow.
//
////////////////////////////////////////////////////////////////////////////////

class RefreshAnchor
{
public:

    //  The view as it was before the rows were re-read.
    struct Before
    {
        std::vector<std::wstring>  keys;               // row order
        int                        topRow   = 0;
        int                        capacity = 0;       // rows on screen
        std::vector<int>           selected;           // indexes into keys
        int                        focused  = -1;      // index into keys
    };

    //  The view to show over the new rows.
    struct After
    {
        std::vector<int>  selected;                    // indexes into the new keys
        int               focused = -1;
        int               topRow  = 0;
    };

    static After  Compute (const Before & before, const std::vector<std::wstring> & keys);

private:

    static int  GetMaxTopRow (int rowCount, int capacity);
};
