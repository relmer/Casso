#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/SectorSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapWriter
//
//  What every map reader shares: it gives each cell its result, takes the
//  files and the sectors they own and the volume's own structures, and at
//  the end settles each cell's role (FR-086), each file's states (FR-089)
//  and deleted counts (FR-088), and the file system findings (FR-093). A
//  volume with a bitmap gives it, so allocated-but-unowned and
//  owned-but-free apply only there. Chain walks go through Step, which stops
//  at a loop, a pointer outside the volume, a bad or missing sector, or a
//  chain longer than the volume (FR-092).
//
////////////////////////////////////////////////////////////////////////////////

class FileMapWriter
{
public:
    FileMapWriter (const SectorSource & source, FileMap & map, MapFileSystem fileSystem);

    int   AddFile       (MappedFile file);
    void  Own           (int file, int cell, SectorRole role);
    void  AddHole       (int file);
    void  SetStructure  (int cell, SectorRole role);
    void  SetMarkedUsed (vector<bool> markedUsed);
    void  BreakChain    (int file, ChainReason reason, int cell);
    void  AddFinding    (FindingKind kind, int cell, const std::wstring & text);

    //  A whole chain whose sector count differs from the one its catalog or
    //  directory entry records (FR-093).
    void  CheckCount    (int file, const std::wstring & source, LPCWSTR unit);
    void  Finalize      ();

    //  One step of a chain onto a cell: false, with the reason, when the
    //  chain must stop there.
    bool  Step (int cell, std::set<int> & visited, ChainReason & outReason) const;

    bool           IsOwned      (int cell) const { return cell >= 0 && cell < static_cast<int> (m_map.cells.size()) && !m_map.cells[cell].owners.empty(); }
    SectorResult   GetResult    (int cell) const { return m_map.cells[cell].result; }
    int            GetCellCount () const         { return static_cast<int> (m_map.cells.size()); }
    std::wstring   DescribeCell (int cell) const;
    int            GetPhysical  (int cell) const;

    static std::wstring  DescribeReason (ChainReason reason);

private:
    void  SettleCell  (int cell);
    void  SettleFiles ();

    const SectorSource &  m_source;
    FileMap &             m_map;
    vector<SectorRole>    m_structure;
    vector<SectorRole>    m_ownerRole;
    vector<bool>          m_markedUsed;
    bool                  m_hasBitmap = false;
};
