#pragma once

#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlockEntry
//
//  A range the disassembler shows as data, with how many items go on a line.
//
////////////////////////////////////////////////////////////////////////////////

struct DataBlockEntry
{
    std::string    name;
    Word           first   = 0;
    Word           last    = 0;
    DataBlockKind  kind    = DataBlockKind::Bytes;
    int            perLine = 8;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DataBlockTable
//
//  The data directives' ranges, kept in address order without overlap: a new
//  block takes its range from any it covers, and X removes, trims or splits
//  what it touches. A block with no given name is called after its kind and
//  address, B_0300 or T_0800.
//
////////////////////////////////////////////////////////////////////////////////

class DataBlockTable
{
public:
    void   Add       (const std::string & name, Word first, Word last, DataBlockKind kind, int perLine);
    void   Remove    (Word first, Word last);
    void   Clear     ();
    bool   TryFindAt (Word address, DataBlockEntry & entry) const;

    const std::vector<DataBlockEntry> &  GetAll () const { return m_entries; }

    static std::string  MakeName (DataBlockKind kind, Word first);

private:
    std::vector<DataBlockEntry>  m_entries;
};
