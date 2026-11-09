#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSelection
//
//  One selection of the inspector window: its quarter track, sector,
//  nibbles and bytes, with -1 for a part that is not selected.
//
////////////////////////////////////////////////////////////////////////////////

struct InspectorSelection
{
    int   quarterTrack = 0;
    int   sectorIndex  = -1;
    int   firstNibble  = -1;
    int   nibbleCount  = 0;
    int   firstByte    = -1;
    int   byteCount    = 0;
    bool  isTextColumn = false;

    bool  operator== (const InspectorSelection & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SelectionHistory
//
//  The user's selections on one disk, for Back and Forward (FR-164). A
//  selection added after a step back replaces the ones ahead of it, and the
//  oldest is dropped past kMaxEntries.
//
////////////////////////////////////////////////////////////////////////////////

class SelectionHistory
{
public:
    static constexpr size_t  kMaxEntries = 200;

    void  Record       (const InspectorSelection & selection);
    bool  GoBack       (InspectorSelection & outSelection);
    bool  GoForward    (InspectorSelection & outSelection);
    bool  CanGoBack    () const { return m_current > 0; }
    bool  CanGoForward () const { return m_current + 1 < m_entries.size(); }
    bool  IsEmpty      () const { return m_entries.empty(); }

private:
    vector<InspectorSelection>  m_entries;
    size_t                      m_current = 0;
};
