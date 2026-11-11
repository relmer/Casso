#include "Pch.h"

#include "CassoExplorer/Model/PreviewSteps.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewSteps::IsStop
//
//  A file, not a folder, and one of the step set when there is one.
//
////////////////////////////////////////////////////////////////////////////////

bool PreviewSteps::IsStop (const std::vector<CatalogRow> & rows, const std::vector<std::wstring> & keys, const std::vector<std::wstring> & set, size_t row)
{
    bool  inSet = set.empty() || (row < keys.size() && std::find (set.begin(), set.end(), keys[row]) != set.end());



    return !rows[row].isDirectory && inSet;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewSteps::GetTarget
//
////////////////////////////////////////////////////////////////////////////////

int PreviewSteps::GetTarget (const std::vector<CatalogRow>   & rows,
                             const std::vector<std::wstring> & keys,
                             const std::vector<std::wstring> & set,
                             int                               current,
                             bool                              forward)
{
    int  count = (int) rows.size();



    if (forward)
    {
        for (int row = (current < 0) ? 0 : current + 1; row < count; row++)
        {
            if (IsStop (rows, keys, set, (size_t) row))
            {
                return row;
            }
        }
    }
    else
    {
        for (int row = (current < 0 || current > count) ? count - 1 : current - 1; row >= 0; row--)
        {
            if (IsStop (rows, keys, set, (size_t) row))
            {
                return row;
            }
        }
    }

    return -1;
}
