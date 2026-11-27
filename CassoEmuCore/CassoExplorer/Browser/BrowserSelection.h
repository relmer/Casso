#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/Location.h"

class BrowserModel;
class IBrowserSelectionSource;
struct CatalogRow;
struct FileEntry;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserSelection
//
//  The list's selected rows: selecting by row and by key, the keys a tab
//  stores its selection by, and the catalog entries, host paths and shell
//  ids behind the rows selected.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserSelection
{
public:
    BrowserSelection (BrowserModel            & model,
                      std::vector<CatalogRow> & rows,
                      std::vector<int>        & selectedRows,
                      IBrowserSelectionSource & source);

    void  SetSelectedRows  (const std::vector<int> & rows);

    //  Selects the rows whose keys are in `keys`, in one pass over the rows.
    void  SelectRowsByKeys (const std::vector<std::wstring> & keys);

    //  The row at a place in the list, or the last row when the list ends
    //  before it; nothing for a negative place or an empty list.
    void  SelectRowAt      (int row);

    //  Every row's key, in row order, as a selection stores them: a name and
    //  its occurrence among rows with that name. A refresh matches rows by
    //  key, since an index moves whenever a row is added or removed above it.
    void          GetRowKeys      (std::vector<std::wstring> & outKeys) const;

    //  The key a selection stores for a row: its name and its occurrence among
    //  rows with that name, since a DOS 3.3 catalog can repeat a name.
    std::wstring  GetSelectionKey (size_t row) const;

    //  The catalog entry behind the one selected row, when the location is a
    //  volume.
    bool  TryGetSelectedEntry (const FileEntry *& outEntry) const;

    //  The catalog entries behind the selected rows, in row order, when the
    //  location is an image; empty otherwise.
    void  GetSelectedEntries   (std::vector<FileEntry> & outEntries) const;

    //  The host paths behind the selected rows, when the location is a host
    //  folder; empty otherwise.
    void  GetSelectedHostPaths (std::vector<std::wstring> & outPaths) const;

    //  The selection by the shell's ids for it, when every selected row is a
    //  host file or an item the shell lists; false when any is inside a disk
    //  image. Copy and the shell's commands access a selected row by its place
    //  in the shell, which a shell folder's and Home's rows have.
    bool         TryGetSelectedShellIds (std::vector<std::wstring> & outIds) const;
    static bool  HasShellRows           (Location::Kind kind, bool home) { return kind == Location::Kind::ShellFolder || home; }

    //  Whether the selected rows are all files or folders on a disk that a
    //  search found, which act as they do in their folders.
    bool  AreSelectedRowsSearchFiles () const;

    //  Whether the selected rows are all image files the Casso node lists from
    //  its folders, which act as they do in their folders. A stock disk is not one.
    bool  AreSelectedRowsCassoFiles  () const;

    //  Whether the selected rows are all disk image files, in a host folder,
    //  a search or the Casso node. A stock disk not downloaded is not a file.
    bool  AreSelectedRowsImages      () const;

private:
    static bool  IsCassoNodeLocation (const Location & location);

    BrowserModel             & m_model;
    std::vector<CatalogRow>  & m_rows;
    std::vector<int>         & m_selectedRows;
    IBrowserSelectionSource  & m_source;
};
