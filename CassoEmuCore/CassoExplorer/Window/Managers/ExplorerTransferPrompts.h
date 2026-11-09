#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTransferPrompts
//
//  The questions put to the user during a copy into a disk image or a folder,
//  for a paste, a drop or an undone deletion: a binary's load address, and
//  what to do with a file of the same name.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerTransferPrompts
{
public:
    explicit ExplorerTransferPrompts (ExplorerContext & context);

    CassoExplorerActions::AddressFn    MakeAddressPrompt   ();
    CassoExplorerActions::CollisionFn  MakeCollisionPrompt ();

private:
    ExplorerContext  & m_context;
};