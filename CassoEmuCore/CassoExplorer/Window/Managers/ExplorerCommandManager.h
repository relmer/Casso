#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager
//
//  Runs a command by id, and reports whether it is enabled, checked or
//  available on a toolbar.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerCommandManager
{
public:
    explicit ExplorerCommandManager (ExplorerContext & context);

    //  Each zoom step, in percent of the theme's text size.
    static constexpr int   kPreviewZoomStep = 10;

    //  How long the list stays empty on a refresh, as File Explorer's does,
    //  so the refresh can be seen.
    static constexpr UINT  kRefreshBlankMs  = 100;

    void          Dispatch                   (int id);
    bool          IsEnabled                  (int id) const;
    bool          IsBlockedInPicker          (int id) const;
    bool          IsChecked                  (int id) const;
    std::wstring  GetCommandLabel            (int id) const;
    int           GetSortColumnCount         () const;
    bool          IsToolbarEntryAvailable    (int index) const;
    bool          IsCommandBarEntryAvailable (int index) const;
    void          ShowOptions                ();

private:
    void  ChooseCharset   (Charset charset);
    void  ChooseSheetForm (bool formulas);
    void  GetFonts        ();

    ExplorerContext  & m_context;
};
