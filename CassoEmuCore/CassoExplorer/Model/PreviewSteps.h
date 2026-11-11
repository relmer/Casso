#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CatalogModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewSteps
//
//  Where the expanded preview's previous and next go: the file before or
//  after the current row, in row order, which is the list's sort and group
//  order, with folders skipped. A step set, the keys of files selected
//  together when the preview was expanded, limits the steps to those files.
//
//  The rows are read when each step is taken, so a file added to the folder
//  or removed from it since the last step is stepped to or past.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewSteps
{
public:
    //  The row a step from `current` goes to, or -1 at either end. `keys` are
    //  the rows' keys in row order, as CassoExplorerBrowser::GetRowKeys makes
    //  them, and `set` the step set's keys, empty for every file. With no
    //  current row, a step forward goes to the first file and a step back to
    //  the last.
    static int  GetTarget (const std::vector<CatalogRow>   & rows,
                           const std::vector<std::wstring> & keys,
                           const std::vector<std::wstring> & set,
                           int                               current,
                           bool                              forward);

private:
    static bool  IsStop (const std::vector<CatalogRow> & rows, const std::vector<std::wstring> & keys, const std::vector<std::wstring> & set, size_t row);
};
