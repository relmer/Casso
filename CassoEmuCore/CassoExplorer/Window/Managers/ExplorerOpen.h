#pragma once

#include "Pch.h"

struct ExplorerContext;
struct InitializeCardChoice;





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

    //  The file a partition on a card is copied to, which the user picks. An
    //  empty path for a partition of an image file. False when canceled.
    bool  PickPartitionImage  (const std::wstring & partition, std::wstring & outTarget);

    void  SetCardWriteConfirmed (bool confirmed);

    //  A storage card erased and given the layout the user picks, after a
    //  confirmation that lists the card and each volume erased.
    void  InitializeCard        (const std::wstring & cardPath);

private:
    static constexpr uint64_t  kSliceBytes = 32ull * 1024 * 1024;

    static void  BuildInitializeChoices (uint64_t bytes, int maxVolumes, std::vector<InitializeCardChoice> & outChoices);

    ExplorerContext  & m_context;
};