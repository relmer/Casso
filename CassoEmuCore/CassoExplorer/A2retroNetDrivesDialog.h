#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsA2retroNet.h"
#include "Devices/Disk/A2retroNetConfig.h"

class IShellIcons;





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDriveRow
//
//  One drive: its number, a disk image's icon, the dropdown of the card's
//  images, and below it the image's size or problem.
//
////////////////////////////////////////////////////////////////////////////////

class A2retroNetDriveRow : public DxuiPanel
{
public:
    static constexpr int  kHeightDip = 52;

    void  Init (int number, std::shared_ptr<const DxuiIconImage> icon, DxuiHwndSource * popupHost);

    DxuiComboBox & GetImageBox () { return m_image; }
    DxuiLabel    & GetStatus   () { return m_status; }

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

private:
    static constexpr int  kNumberDip = 18;
    static constexpr int  kIconDip   = 20;
    static constexpr int  kGapDip    = 8;

    DxuiLabel                              m_number;
    DxuiComboBox                           m_image;
    DxuiLabel                              m_status;
    std::shared_ptr<const DxuiIconImage>   m_icon;
    RECT                                   m_iconRect = {};
};





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesPanel
//
//  The number of drives and the boot delay, then the eight drives in a
//  stack whose drives in use are reordered by dragging, then a notice when
//  the file has CR LF line ends or another program changed it.
//
////////////////////////////////////////////////////////////////////////////////

class A2retroNetDrivesPanel : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    struct Parts
    {
        DxuiLabel         * numberLabel = nullptr;
        DxuiComboBox      * number      = nullptr;
        DxuiLabel         * delayLabel  = nullptr;
        DxuiComboBox      * delay       = nullptr;
        DxuiReorderStack  * drives      = nullptr;
        DxuiLabel         * notice      = nullptr;
    };

    void  Init (const Parts & parts);

    //  Called after a mouse event, to lay out and repaint while the stack
    //  moves.
    void  SetOnMouseHandled (std::function<void ()> handled) { m_handled = std::move (handled); }

    void  Layout             (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse            (const DxuiMouseEvent & ev) override;
    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

private:
    static constexpr int  kLabelWidthDip = 100;
    static constexpr int  kSectionGapDip = 14;

    int  Arrange (const RECT & bounds, const DxuiDpiScaler & scaler, bool place);

    Parts                    m_parts;
    DxuiDpiScaler            m_scaler;
    std::function<void ()>   m_handled;
};





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetDrivesDialog
//
//  A2retroNET drives for an A2Pico card: the number of drives, the boot
//  delay, and the image of each drive, chosen from the images on the card.
//  An image is on one drive at a time. The drives in use are reordered by
//  dragging their grippers; the drives past the number in use are at the
//  bottom, not configured. Save writes A2retroNET.txt with its comments in
//  place. Edit as text opens the file in the user's text editor, and the
//  file is read again once it is saved there. When the dialog has unsaved
//  changes, a notice warns that Save replaces the other program's changes.
//
////////////////////////////////////////////////////////////////////////////////

class A2retroNetDrivesDialog : public DxuiDialogWindow
{
public:
    using EditAsTextFn = std::function<void (const std::wstring & path)>;

    A2retroNetDrivesDialog (ActionsA2retroNet & actions, const std::wstring & root, IShellIcons * icons, const std::wstring & editor, EditAsTextFn editAsText)
        : m_actions (actions), m_root (root), m_icons (icons), m_editor (editor), m_editAsText (std::move (editAsText)) {}

    //  False when canceled or the dialog could not open; otherwise the
    //  outcome of the save. `editor` is the program `editAsText` opens the
    //  settings file in, for the button's label, or empty when not known.
    static bool  Edit (HWND                        owner,
                       const IDxuiTheme          * theme,
                       ActionsA2retroNet         & actions,
                       const std::wstring        & root,
                       IShellIcons               * icons,
                       const std::wstring        & editor,
                       EditAsTextFn                editAsText,
                       ActionsTypes::Outcome     & outOutcome);

protected:
    void  OnCreate     () override;
    void  OnDialogTick () override;

private:
    //  The tick while the drives move, for a smooth slide, and otherwise.
    static constexpr UINT  kSlideTickMs = 16;
    static constexpr UINT  kIdleTickMs  = 250;

    static constexpr const wchar_t * kpszNotConfigured = L"<not configured>";
    static constexpr const wchar_t * kpszNoImage       = L"No image";

    //  The file and the card's images read again, and the controls set from
    //  them.
    void  Load          ();
    void  Fill          ();
    void  FillDrive     (int drive);
    void  ChooseImage   (int drive, int index);
    void  MoveDrive     (size_t from, size_t to);
    void  ChooseNumber  (int index);
    void  ChooseDelay   (int index);
    void  UpdateTick    ();

    ActionsA2retroNet                                                     & m_actions;
    std::wstring                                                            m_root;
    IShellIcons                                                           * m_icons     = nullptr;
    std::wstring                                                            m_editor;
    EditAsTextFn                                                            m_editAsText;
    A2retroNetConfig                                                        m_config;
    std::vector<std::string>                                                m_images;
    std::array<std::vector<std::string>, A2retroNetConfig::kDrivesMax>      m_choices;
    FileStamp                                                               m_stamp;
    bool                                                                    m_dirty     = false;
    bool                                                                    m_saved     = false;
    bool                                                                    m_sliding   = false;
    ActionsTypes::Outcome                                                   m_outcome;
    std::array<A2retroNetDriveRow, A2retroNetConfig::kDrivesMax>            m_rows;
    DxuiReorderStack                                                        m_drives;
    DxuiLabel                                                               m_numberLabel;
    DxuiComboBox                                                            m_numberBox;
    DxuiLabel                                                               m_delayLabel;
    DxuiComboBox                                                            m_delayBox;
    DxuiLabel                                                               m_notice;
    A2retroNetDrivesPanel                                                 * m_body      = nullptr;
};
