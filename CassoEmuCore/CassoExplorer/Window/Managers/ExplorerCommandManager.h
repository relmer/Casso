#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "FileTypes/DecodedFile.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerCommandManager
//
//  The commands: running one by id, whether it is enabled or checked,
//  and the verbs it runs, raw sector and block verbs included, with
//  their outcomes reported.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerCommandManager
{
public:
    explicit ExplorerCommandManager (ExplorerContext & context);

    static constexpr uint64_t  kDos33ImageBytes = 35 * 16 * 256;

    //  Each zoom step, in percent of the theme's text size.
    static constexpr int    kPreviewZoomStep     = 10;

    //  How long the list stays empty on a refresh, as File Explorer's does,
    //  so the refresh can be seen.
    static constexpr UINT  kRefreshBlankMs        = 100;

    //  The raw read and write pickers' own history, apart from other pickers.
    static constexpr GUID  s_kRawPickerGuid = { 0xe347dd39, 0x4a18, 0x4a0d, { 0x93, 0x4a, 0xfd, 0x63, 0xb7, 0x9c, 0xba, 0x3e } };

    //  The longest record length that can be typed, and its digits.
    static constexpr size_t  kRecordLengthMax    = 65535;
    static constexpr int     kRecordLengthDigits = 5;

    static bool            TryParseRecordLength       (const std::wstring & text, size_t & outLength);
    static std::wstring    GetVerbTitle               (CassoExplorerActions::Verb verb);
    int                    ChooseFormatDefault        (const std::wstring & image);
    void                   ChooseFilesystem           (VolumeKind kind);
    void                   ExtractAll                 ();
    void                   MakeArchive                (bool shrinkItDisk);
    void                   TestImage                  ();
    void                   CopyPartitionToImage       ();
    void                   ReplacePartition           ();
    void                   Dispatch                   (int id);
    bool                   IsEnabled                  (int id) const;
    bool                   IsChecked                  (int id) const;

    //  The count of list columns with a Sort by command: the fixed ones and
    //  the shell properties added, up to the ids set aside for them.
    int                        GetSortColumnCount         () const;
    void                       CreateDisk                 (const CassoExplorerNewDiskDialog::Outcome & newDisk);
    void                       ShowOptions                ();
    bool                       IsToolbarEntryAvailable    (int index) const;
    bool                       IsCommandBarEntryAvailable (int index) const;
    HostFileNaming::Style      GetNamingStyle             () const;
    void                       RunVerb                    (CassoExplorerActions::Verb verb);
    bool                       RunRecycleBinVerb          (CassoExplorerActions::Verb verb);
    std::vector<std::wstring>  GetRecycledPaths           (const std::vector<std::wstring> & ids);
    std::wstring               GetCommandLabel            (int id) const;
    void                       RunRawVerb                 (CassoExplorerActions::Verb verb);
    bool                       IsRawTargetCurrent         (const std::wstring & target);
    void                       ReportOutcome              (const CassoExplorerActions::Outcome & outcome, const wchar_t * verbName);
    bool                       TryPrepareBootDisk         (const DiskOperations::NewDiskRequest & request);
    void                       ExtractImageFiles          (const std::wstring & imagePath);

private:
    void  ChooseCharset      (Charset charset);
    void  ChooseRecordLength ();
    void  ChooseSheetForm    (bool formulas);

    ExplorerContext  & m_context;
};
