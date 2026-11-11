#pragma once

#include "Pch.h"

#include "Seams/RawDevice/IRawDevice.h"

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
    ~ExplorerOpen ();

    ExplorerOpen (const ExplorerOpen &) = delete;
    ExplorerOpen & operator= (const ExplorerOpen &) = delete;

    void  OpenSelectedEntries ();
    void  OpenEachSelected    ();

    //  The file a partition on a card is copied to, which the user picks. An
    //  empty path for a partition of an image file. False when canceled.
    bool  PickPartitionImage  (const std::wstring & partition, std::wstring & outTarget);

    void  SetCardWriteConfirmed (bool confirmed);

    //  A storage card erased and given the layout the user picks, after a
    //  confirmation that lists the card and each volume erased. The write
    //  runs on a thread, and its progress appears in the card's list.
    void  InitializeCard        (const std::wstring & cardPath);

    //  For ExplorerCardProgress: the card's list refreshed, or, once the
    //  write ends, the tree and list reloaded and a failure reported.
    void  HandleCardProgress    ();

    //  Initialize card for Apple, for the root of a drive letter on a storage
    //  card. Nothing for any other folder.
    void  AddInitializeRow      (std::vector<DxuiPopupMenuItem> & items, const std::wstring & folder);

    //  InitializeCard for the card open in the list, or for the card of the
    //  drive letter open in it. Nothing for any other location.
    void  InitializeShownCard   ();

private:
    static constexpr ULONGLONG  kProgressPostMs = 250;

    void  StartInitialize  (const std::wstring & cardPath, const PhysicalDrive & drive, uint64_t layout, std::vector<WindowsVolume> volumes);
    void  FinishInitialize ();

    ExplorerContext             & m_context;
    std::thread                   m_initThread;
    std::atomic<bool>             m_initDone        = false;
    std::atomic<HRESULT>          m_initResult      = S_OK;
    std::atomic<ULONGLONG>        m_lastPostMs      = 0;
    bool                          m_navigatePending = false;
    std::wstring                  m_initCard;
    PhysicalDrive                 m_initDrive;
    uint64_t                      m_initLayout      = 0;
    std::vector<WindowsVolume>    m_initVolumes;
};