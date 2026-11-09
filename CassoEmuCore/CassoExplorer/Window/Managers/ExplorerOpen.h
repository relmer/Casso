#pragma once

#include "Pch.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerOpen
//
//  Opening the selection: files inside a disk image in the programs Windows
//  has for them, and several items one at a time, a folder or an image in a
//  new tab and a file in its program.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerOpen
{
public:
    explicit ExplorerOpen (ExplorerContext & context);

    void  OpenSelectedEntries ();
    void  OpenEachSelected    ();

private:
    ExplorerContext  & m_context;
};