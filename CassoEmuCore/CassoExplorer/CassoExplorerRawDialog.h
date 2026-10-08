#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/DiskOperations.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawChoices
//
//  The rules for where a read or write of raw sectors or blocks starts and
//  how far it runs, kept apart from the dialog so they are testable without a
//  window. A 140K disk is 35 tracks of 16 sectors, or 280 blocks.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerRawChoices
{
public:
    static constexpr int  kTracks          = 35;
    static constexpr int  kSectorsPerTrack = 16;
    static constexpr int  kBlocks          = 280;

    //  Where a read starts unless the user says otherwise: what a reader most
    //  often wants first. A DOS 3.3 disk's catalog starts at track 17, sector
    //  0; a ProDOS volume directory takes blocks 2 to 5.
    static constexpr int  kDefaultTrack       = 17;
    static constexpr int  kDefaultSector      = 0;
    static constexpr int  kDefaultSectorCount = 1;
    static constexpr int  kDefaultBlock       = 2;
    static constexpr int  kDefaultBlockCount  = 4;

    //  A whole number with nothing around it but spaces.
    static bool          TryParse (const std::wstring & text, int & outValue);

    //  Why a field's text cannot be used, or empty when it can.
    static std::wstring  ValidateTrack  (const std::wstring & text);
    static std::wstring  ValidateSector (const std::wstring & text);
    static std::wstring  ValidateBlock  (const std::wstring & text);

    //  A count runs at most to the end of the disk; `available` is how far
    //  that is from the start, or zero when the start itself is not valid.
    static std::wstring  ValidateCount  (const std::wstring & text, int available);

    static int           GetSectorsFrom (int track, int sector);
    static int           GetBlocksFrom  (int block);
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawPanel
//
//  The form's rows, each a label beside its field with its error under it,
//  then a note on what the starting place holds. An error takes the room it
//  needs and pushes the rows below down.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerRawPanel : public DxuiPanel
{
public:
    struct Row
    {
        DxuiLabel       * label = nullptr;
        IDxuiControl    * field = nullptr;
        DxuiFieldError  * error = nullptr;     // none for a field that cannot be wrong
    };

    void  Init              (std::vector<Row> rows, DxuiLabel * note);
    void  SetOnChildPressed (std::function<void (IDxuiControl *)> fn)        { m_onPressed = std::move (fn); }
    void  SetMeasure        (IDxuiTextRenderer * text, const IDxuiTheme * theme) { m_text = text; m_theme = theme; }

    int   GetRequiredHeightPx () const { return ArrangeRows (m_boundsDip, m_scaler, false); }

    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kRowHeightDip     = 30;
    static constexpr int  kRowGapDip        = 8;
    static constexpr int  kLabelWidthDip    = 110;
    static constexpr int  kNoteHeightDip    = 40;
    static constexpr int  kFallbackErrorDip = 20;
    static constexpr int  kErrorTopDip      = 4;

private:
    int   ArrangeRows      (const RECT & bounds, const DxuiDpiScaler & scaler, bool place) const;
    int   GetErrorHeightPx (const DxuiFieldError & error, int widthPx, const DxuiDpiScaler & scaler) const;

    std::vector<Row>                       m_rows;
    DxuiLabel                            * m_note   = nullptr;
    std::function<void (IDxuiControl *)>   m_onPressed;
    IDxuiTextRenderer                    * m_text   = nullptr;
    const IDxuiTheme                     * m_theme  = nullptr;
    DxuiDpiScaler                          m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerRawDialog
//
//  Asks where a raw read or write starts, and for a read how far it runs:
//  track, sector and count with logical or physical numbering for sectors,
//  block and count for blocks. Each field is checked as it changes; the
//  button stays held back until all pass.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerRawDialog : public DxuiDialogWindow
{
public:
    struct Outcome
    {
        bool                         confirmed = false;
        int                          start     = 0;     // a track, or a block
        int                          sector    = 0;
        int                          count     = 1;
        DiskOperations::Numbering    numbering = DiskOperations::Numbering::Logical;
    };

    static Outcome  Ask (HWND owner, const IDxuiTheme * theme, const std::wstring & title, bool sectors, bool reading);

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    void  Revalidate   ();
    void  FitToContent ();
    int   GetAvailable () const;

    const IDxuiTheme       * m_theme      = nullptr;
    bool                     m_sectors    = true;
    bool                     m_reading    = true;
    bool                     m_fitted     = false;
    DxuiButton             * m_ok         = nullptr;
    DxuiFieldValidator       m_validator;
    Outcome                  m_outcome;
    DxuiLabel                m_startLabel;
    DxuiTextInput            m_start;
    DxuiFieldError           m_startError;
    DxuiLabel                m_sectorLabel;
    DxuiTextInput            m_sector;
    DxuiFieldError           m_sectorError;
    DxuiLabel                m_countLabel;
    DxuiTextInput            m_count;
    DxuiFieldError           m_countError;
    DxuiLabel                m_numberingLabel;
    DxuiComboBox             m_numbering;
    DxuiLabel                m_note;
    CassoExplorerRawPanel  * m_body       = nullptr;
};
