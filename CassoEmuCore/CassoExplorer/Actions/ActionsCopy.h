#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"
#include "CassoExplorer/Model/HostFileNaming.h"

class CassoExplorerActions;





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsCopy
//
//  Copies in and out of images: host files and folders put into an
//  image, entries copied between images or out to the host, an image's
//  files extracted, collisions resolved with numbered names, and whether a new
//  disk has room.
//
////////////////////////////////////////////////////////////////////////////////

class ActionsCopy
{
public:
    explicit ActionsCopy (CassoExplorerActions & actions);

    ActionsTypes::Outcome  PasteVirtualFiles (IDataObject * data, const std::wstring & hostFolder, const ActionsTypes::CollisionFn & collision,
                                std::vector<std::wstring> & outCreated);

    //  Whether the clipboard's data is Casso Explorer's own copy of an
    //  image's or a linked Apple's files.
    static bool  IsCassoCopy (IDataObject * data);

    ActionsTypes::Outcome  ExtractImage   (const std::wstring & imagePath, HostFileNaming::Style style, std::wstring & outFolder);
    ActionsTypes::Outcome  PutFiles       (const std::vector<std::wstring> & hostPaths, const ActionsTypes::AddressFn & askAddress = {});

    ActionsTypes::Outcome  PutInto        (const std::wstring & imagePath, VolumeKind kind, const std::string & directory,
                             const std::vector<std::wstring> & hostPaths, const ActionsTypes::AddressFn & askAddress = {},
                             ActionsTypes::Conversion conversion = ActionsTypes::Conversion::ByContent);

    std::wstring  CheckFitsNewDisk (VolumeKind kind, const std::vector<std::wstring> & hostPaths);

    ActionsTypes::Outcome  CopyEntriesInto (const std::string & sourceImage, VolumeKind sourceKind, const std::vector<std::string> & catalogPaths,
                              const std::wstring & targetImage, VolumeKind targetKind, const std::string & directory);

    //  The data and structure a file and a directory take, and a new disk's
    //  room for them, by file system. Public so the arithmetic is tested.
    static uint64_t  CountUnitsForFile   (VolumeKind kind, uint64_t sizeBytes);
    static uint64_t  CountUnitsForFolder (VolumeKind kind);
    static uint64_t  GetNewDiskFreeUnits (VolumeKind kind);

    //  Whether the catalog path `directory` is below `folder`, in any case.
    static bool          IsInsideFolder (const std::string & directory, const std::string & folder);
    static constexpr uint64_t         kProDosBlockBytes        = 512;
    static constexpr uint64_t         kProDosIndexEntries      = 256;
    static constexpr uint64_t         kProDosNewDiskFreeBlocks = 273;   // 280 less 2 boot, 4 directory, 1 bitmap
    static constexpr uint64_t         kDos33SectorBytes        = 256;
    static constexpr uint64_t         kDos33ListEntries        = 122;
    static constexpr uint64_t         kDos33NewDiskFreeSectors = 496;   // 560 less tracks 0-2 and the catalog track
    static void  AppendMessage (ActionsTypes::Outcome & inOutOutcome, HRESULT hr, const std::wstring & message);
    static void  AppendWarning (ActionsTypes::Outcome & inOutOutcome, const std::wstring & warning);

    bool  IsHostFolder    (const std::wstring & path);

    //  Where a copy out of one file goes, by the collision choice when the
    //  folder has a file of its name; empty to skip it.
    std::wstring  ResolveCopyOutPath (const std::wstring & hostFolder, const std::wstring & hostName, const ActionsTypes::CollisionFn & collision,
                                      int & inOutIndex, int count);
    int           CountCollisions    (const std::wstring & hostFolder, const std::vector<std::wstring> & hostNames);
    void  CopyEntry       (const std::string & sourceImage, VolumeKind sourceKind, const std::string & catalogPath,
                           const std::string & targetImage, VolumeKind targetKind, const std::string & directory,
                           int depth, ActionsTypes::Outcome & inOutOutcome);

    static constexpr int  kMaxCopyDepth = 16;
    void  ExtractDirectory (const std::string & image, VolumeKind kind, const std::string & directory, const std::wstring & hostFolder,
                            HostFileNaming::Style style, int depth, ActionsTypes::Outcome & inOutOutcome);

    //  One item's FILECONTENTS, from memory or a stream, cut to the
    //  descriptor's size where it gives one.
    static HRESULT  ReadFileContents (IDataObject * data, LONG index, const FILEDESCRIPTORW & item, std::vector<Byte> & outBytes);

    std::wstring  MakeFreeFolderPath (const std::wstring & parent, const std::wstring & name);
    std::wstring  MakeFreeFilePath   (const std::wstring & folder, const std::wstring & name);
    bool  TryGetHostEntry (const std::wstring & path, FileSystemEntry & outEntry);
    void  PutItems     (const std::string & image, VolumeKind kind, const std::string & directory,
                        const std::vector<std::wstring> & hostPaths, const ActionsTypes::AddressFn & askAddress, ActionsTypes::Conversion conversion, ActionsTypes::Outcome & inOutOutcome);
    void  PutFolder    (const std::string & image, VolumeKind kind, const std::string & directory,
                        const std::wstring & hostFolder, const ActionsTypes::AddressFn & askAddress, ActionsTypes::Conversion conversion, ActionsTypes::Outcome & inOutOutcome);
    static void  Record (ActionsTypes::Outcome & inOutOutcome, const DiskOperations::Result & result, const std::string & catalogPath);
    static void  KeepCreatedIn (ActionsTypes::Outcome & inOutOutcome, const std::string & directory);

private:
    friend class CassoExplorerActions;

    CassoExplorerActions  & m_actions;
};
