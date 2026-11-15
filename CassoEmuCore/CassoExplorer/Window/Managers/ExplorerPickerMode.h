#pragma once

#include "Pch.h"

#include "CassoExplorer/Picker/PickerScope.h"

struct ExplorerContext;
struct PickerRequest;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMode
//
//  A window opened as a picker: its row scope, which row can be confirmed,
//  and the confirm and the cancel. In a window that is not a picker every
//  call does nothing.
//
//  A confirm in a drive's picker inserts the image through the request and
//  closes the window when the insert was taken. In a modal picker it puts
//  the path in the outcome and closes the window.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPickerMode
{
public:
    explicit ExplorerPickerMode (ExplorerContext & context);

    //  Before the first listing: the browser lists only rows in scope.
    void  Start ();

    //  The path of the one file row selected, in a folder or in the Casso
    //  node's list.
    bool  TryGetSelectedFile (std::wstring & outPath) const;
    bool  CanConfirm         () const;
    void  Confirm            ();
    void  Cancel             ();

    //  Creates a disk or a tape for the drive and confirms it.
    bool  CanCreateNew       () const;
    void  CreateNew          ();

    //  A row opened by Enter or a double-click. True when the row is a file,
    //  which is confirmed rather than opened.
    bool  HandleActivateRow (int row);

    //  Esc cancels.
    bool  HandleKey (const DxuiKeyEvent & ev);

    size_t  GetFilterCount  () const { return m_scope.GetFilterCount(); }
    size_t  GetActiveFilter () const { return m_scope.GetActiveFilter(); }
    void    SetActiveFilter (size_t index);

    //  "Apple 3.5 Drive, slot 5 drive 1: 400K and 800K disk images" for a
    //  drive, and empty for a request with none.
    std::wstring  GetHeaderText   () const;
    std::wstring  GetConfirmLabel () const;
    std::wstring  GetFilterLabel  (size_t index) const;

    static std::wstring  MakeHeaderText (const PickerRequest & request);

private:
    bool  TryGetRowFile      (int row, std::wstring & outPath) const;
    bool  IsStockDiskToFetch (int row) const;
    void  ConfirmPath        (const std::wstring & path);

    ExplorerContext  & m_context;
    PickerScope        m_scope;
};
