#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerClipboard
//
//  The clipboard: cut, copy and paste, on the PC and in disk images and on
//  Apple drives, and copying the selection's paths.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerClipboard
{
public:
    explicit ExplorerClipboard (ExplorerContext & context);

    void                               PasteHere                  (const std::wstring & folder);
    void                               PasteIntoVolume            ();
    std::wstring                       GetPasteFolder             () const;
    void                               CopyEntriesToClipboard     (HostFileNaming::Style style);

    //  Cut on an image's or an Apple drive's entries: they go on the
    //  clipboard as Copy puts them, and are deleted once a target reports
    //  the paste, in FinishCut.
    void                               CutEntriesToClipboard      (HostFileNaming::Style style);
    void                               FinishCut                  ();
    void                               CopySelectedPaths          ();

private:
    //  What the last cut takes away once it is pasted, read when it was cut.
    struct CutSource
    {
        bool                              valid   = false;
        bool                              onDrive = false;
        std::string                       image;
        std::vector<std::string>          catalogPaths;
        ActionsAppleDrive::DriveDeletion  drive;
    };

    void  ReportPasted (IDataObject * data, const CassoExplorerActions::Outcome & outcome);
    bool  IsCutFrom    (const std::string & inner) const;

    ExplorerContext  & m_context;
    CutSource          m_cut;
};