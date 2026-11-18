#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormatRows
//
//  The table's rows, in the order Identify checks them: a row with a
//  narrower match, such as Merlin's name suffix on a text file, ahead of a
//  broader one on the same type.
//
////////////////////////////////////////////////////////////////////////////////

class FileFormatRows
{
public:
    static std::vector<FileFormatTable::Row>  Make ();
};
