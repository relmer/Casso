#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/TrackExport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExportForm / ExportRequest
//
//  What "Export..." writes (FR-058) and what the dialog hands back: the
//  bytes, the sectors or regions the user is told about before anything is
//  written, and the name and extension the save dialog starts with.
//
////////////////////////////////////////////////////////////////////////////////

enum class ExportForm
{
    Sectors,
    TrackNibbles,
    TrackBits,
};


struct ExportRequest
{
    vector<Byte>          bytes;
    vector<std::wstring>  notes;
    std::wstring          fileName;
    std::wstring          extension;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExportDialog
//
//  The form, and for sectors which ones (the selected sector, the selected
//  track or a range of tracks) and in which order. OK builds the export in
//  memory; an entry that cannot be read says why and stays open. The window
//  then lists what was written as decoded or as zeros, asks where to save,
//  and writes through DurableCommit.
//
////////////////////////////////////////////////////////////////////////////////

class ExportDialog : public DxuiDialogWindow
{
public:
    void  Configure (const IDxuiTheme * theme, const DiskAnalysis & analysis, int quarterTrack, int sectorIndex, const std::wstring & imageName);

    bool                    IsChosen   () const { return m_isChosen; }
    const ExportRequest &   GetRequest () const { return m_request; }

    static constexpr SIZE  kSizeDip = { 480, 420 };

protected:
    void  OnCreate () override;

private:
    void  OnOkClicked ();
    bool  BuildSectors (std::wstring & outError);
    bool  BuildTrack   (ExportForm form, std::wstring & outError);

    const IDxuiTheme *    m_theme        = nullptr;   // non-owning
    const DiskAnalysis *  m_analysis     = nullptr;   // non-owning
    int                   m_quarterTrack = 0;
    int                   m_sectorIndex  = -1;
    std::wstring          m_imageName;
    ExportRequest         m_request;
    bool                  m_isChosen     = false;

    DxuiRadioGroup        m_form;
    DxuiRadioGroup        m_scope;
    DxuiRadioGroup        m_order;
    DxuiTextInput         m_firstTrack;
    DxuiTextInput         m_lastTrack;
    DxuiLabel             m_error;
};
