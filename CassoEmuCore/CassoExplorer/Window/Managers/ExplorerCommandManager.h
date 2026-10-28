#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"

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

    static std::wstring    GetVerbTitle               (CassoExplorerActions::Verb verb);
    int                    ChooseFormatDefault        (const std::wstring & image);
    void                   ExtractAll                 ();
    void                   Dispatch                   (int id);
    bool                   IsEnabled                  (int id) const;
    bool                   IsChecked                  (int id) const;
    void                   CreateDisk                 (const CassoExplorerNewDiskDialog::Outcome & newDisk);
    void                   ShowOptions                ();
    bool                   IsToolbarEntryAvailable    (int index) const;
    bool                   IsCommandBarEntryAvailable (int index) const;
    HostFileNaming::Style  GetNamingStyle             () const;
    void                   RunVerb                    (CassoExplorerActions::Verb verb);
    bool                   RunRecycleBinVerb          (CassoExplorerActions::Verb verb);
    std::wstring           GetCommandLabel            (int id) const;
    void                   RunRawVerb                 (CassoExplorerActions::Verb verb);
    bool                   IsRawTargetCurrent         (const std::wstring & target);
    void                   ReportOutcome              (const CassoExplorerActions::Outcome & outcome, const wchar_t * verbName);
    void                   ExtractImageFiles          (const std::wstring & imagePath);

private:
    ExplorerContext  & m_context;
};
