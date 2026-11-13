#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsA2retroNet.h"
#include "Devices/Disk/A2retroNetConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesPanel
//
//  A row for each of the eight drives: its number, its image's dropdown, a
//  button that swaps it with the drive above, and its image's size or
//  problem below. Then the number of drives and the boot delay, and a
//  notice when another program changed the file.
//
////////////////////////////////////////////////////////////////////////////////

class A2retroNetDrivesPanel : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    struct Row
    {
        DxuiLabel     * name   = nullptr;
        DxuiComboBox  * image  = nullptr;
        DxuiButton    * up     = nullptr;
        DxuiLabel     * status = nullptr;
    };

    struct Parts
    {
        std::array<Row, A2retroNetConfig::kDrivesMax>  rows;
        DxuiLabel                                    * numberLabel = nullptr;
        DxuiComboBox                                 * number      = nullptr;
        DxuiLabel                                    * delayLabel  = nullptr;
        DxuiComboBox                                 * delay       = nullptr;
        DxuiLabel                                    * notice      = nullptr;
    };

    void  Init (const Parts & parts);

    void  Layout             (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse            (const DxuiMouseEvent & ev) override;
    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

private:
    static constexpr int  kNameWidthDip   = 64;
    static constexpr int  kUpWidthDip     = 76;
    static constexpr int  kSettingsGapDip = 16;

    int  Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place);

    Parts          m_parts;
    DxuiDpiScaler  m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog
//
//  A2retroNET drives for an A2Pico card: the image of each drive, chosen
//  from the images on the card, the drives' order, the number of drives and
//  the boot delay. Save writes A2retroNET.txt with its comments in place.
//  Edit as text opens the file in the user's text editor, and the file is
//  read again once it is saved there. When the dialog has unsaved changes,
//  a notice warns that Save replaces the other program's changes.
//
////////////////////////////////////////////////////////////////////////////////

class A2retroNetDrivesDialog : public DxuiDialogWindow
{
public:
    using EditAsTextFn = std::function<void (const std::wstring & path)>;

    A2retroNetDrivesDialog (ActionsA2retroNet & actions, const std::wstring & root, EditAsTextFn editAsText)
        : m_actions (actions), m_root (root), m_editAsText (std::move (editAsText)) {}

    //  False when canceled or the dialog could not open; otherwise the
    //  outcome of the save.
    static bool  Edit (HWND                        owner,
                       const IDxuiTheme          * theme,
                       ActionsA2retroNet         & actions,
                       const std::wstring        & root,
                       EditAsTextFn                editAsText,
                       ActionsTypes::Outcome     & outOutcome);

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    //  The file and the card's images read again, and the controls set from
    //  them.
    void  Load          ();
    void  Fill          ();
    void  FillDrive     (int drive);
    void  ChooseImage   (int drive, int index);
    void  MoveUp        (int drive);
    void  ChooseNumber  (int index);
    void  ChooseDelay   (int index);

    ActionsA2retroNet                                                   & m_actions;
    std::wstring                                                          m_root;
    EditAsTextFn                                                          m_editAsText;
    A2retroNetConfig                                                      m_config;
    std::vector<std::string>                                              m_images;
    std::array<std::vector<std::string>, A2retroNetConfig::kDrivesMax>    m_choices;
    FileStamp                                                             m_stamp;
    bool                                                                  m_dirty       = false;
    bool                                                                  m_saved       = false;
    ActionsTypes::Outcome                                                 m_outcome;
    std::array<DxuiLabel, A2retroNetConfig::kDrivesMax>                   m_names;
    std::array<DxuiComboBox, A2retroNetConfig::kDrivesMax>                m_imageBoxes;
    std::array<DxuiButton, A2retroNetConfig::kDrivesMax>                  m_upButtons;
    std::array<DxuiLabel, A2retroNetConfig::kDrivesMax>                   m_statuses;
    DxuiLabel                                                             m_numberLabel;
    DxuiComboBox                                                          m_numberBox;
    DxuiLabel                                                             m_delayLabel;
    DxuiComboBox                                                          m_delayBox;
    DxuiLabel                                                             m_notice;
    A2retroNetDrivesPanel                                               * m_body        = nullptr;
};
