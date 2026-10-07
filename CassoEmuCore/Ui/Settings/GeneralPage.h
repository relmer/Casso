#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPage
//
//  Settings > General: app-wide options that belong to no one subsystem.
//
//      Updates
//        * Check for updates automatically  (DxuiCheckbox)
//        * When the last check ran           (DxuiLabel) + Check now
//        * The skipped release, when set     (DxuiLabel) + Cancel skip
//      Downloads
//        * Offer to download disk drive sounds  (DxuiCheckbox)
//        * Offer updated ROMs                   (DxuiCheckbox)
//      Settings folder
//        * Open settings folder              (DxuiButton)
//
//  Every control acts through its callback at once, so Cancel has nothing to
//  revert and the page never marks itself dirty.
//
////////////////////////////////////////////////////////////////////////////////

class GeneralPage : public DxuiPropertyPage
{
public:
    explicit GeneralPage (std::wstring title = L"General");

    using ToggleFn = std::function<void (bool enabled)>;
    using ActionFn = std::function<void()>;

    void  SetOnAutoUpdateToggled (ToggleFn fn) { m_onAutoUpdateToggled = std::move (fn); }
    void  SetOnAudioOfferToggled (ToggleFn fn) { m_onAudioOfferToggled = std::move (fn); }
    void  SetOnRomOfferToggled   (ToggleFn fn) { m_onRomOfferToggled   = std::move (fn); }
    void  SetOnCheckNow          (ActionFn fn) { m_onCheckNow          = std::move (fn); }
    void  SetOnStopSkipping      (ActionFn fn) { m_onStopSkipping      = std::move (fn); }
    void  SetOnOpenFolder        (ActionFn fn) { m_onOpenFolder        = std::move (fn); }

    void  SetAutoUpdateChecked   (bool checked) { m_autoUpdateCheckbox.SetChecked (checked); }
    void  SetAudioOfferChecked   (bool checked) { m_audioOfferCheckbox.SetChecked (checked); }
    void  SetRomOfferChecked     (bool checked) { m_romOfferCheckbox.SetChecked   (checked); }

    // The update status lines. An empty skipped text hides that row.
    void  SetLastCheckedText     (const std::wstring & text);
    void  SetSkippedText         (const std::wstring & text);

    void  Layout                 (const RECT & rect, const DxuiDpiScaler & scaler) override;

    // Test / wiring accessors.
    DxuiCheckbox       & GetAutoUpdateCheckbox ()       { return m_autoUpdateCheckbox; }
    const DxuiCheckbox & GetAutoUpdateCheckbox () const { return m_autoUpdateCheckbox; }
    DxuiCheckbox       & GetAudioOfferCheckbox ()       { return m_audioOfferCheckbox; }
    DxuiCheckbox       & GetRomOfferCheckbox   ()       { return m_romOfferCheckbox;   }
    DxuiLabel          & GetLastCheckedLabel   ()       { return m_lastCheckedLabel;   }
    DxuiLabel          & GetSkippedLabel       ()       { return m_skippedLabel;       }
    DxuiButton         & GetCheckNowButton     ()       { return m_checkNowButton;     }
    DxuiButton         & GetStopSkippingButton ()       { return m_stopSkipButton;     }
    DxuiButton         & GetOpenFolderButton   ()       { return m_openFolderButton;   }

private:
    static constexpr int  kRowHeightDp    = 28;
    static constexpr int  kRowGapDp       = 6;
    static constexpr int  kSectionGapDp   = 18;
    static constexpr int  kCheckWidthDp   = 360;
    static constexpr int  kTextIndentDp   = 22;
    static constexpr int  kStatusWidthDp  = 250;
    static constexpr int  kButtonWidthDp  = 130;
    static constexpr int  kFolderWidthDp  = 170;
    static constexpr int  kPagePadDp      = 16;

    static RECT  MakeRect   (int l, int t, int w, int h);
    static void  WireToggle (DxuiCheckbox & checkbox, const ToggleFn & fn);

    ToggleFn      m_onAutoUpdateToggled;
    ToggleFn      m_onAudioOfferToggled;
    ToggleFn      m_onRomOfferToggled;
    ActionFn      m_onCheckNow;
    ActionFn      m_onStopSkipping;
    ActionFn      m_onOpenFolder;

    DxuiLabel     m_updatesHeading;
    DxuiCheckbox  m_autoUpdateCheckbox;
    DxuiLabel     m_lastCheckedLabel;
    DxuiButton    m_checkNowButton;
    DxuiLabel     m_skippedLabel;
    DxuiButton    m_stopSkipButton;

    DxuiLabel     m_downloadsHeading;
    DxuiCheckbox  m_audioOfferCheckbox;
    DxuiCheckbox  m_romOfferCheckbox;

    DxuiLabel     m_folderHeading;
    DxuiButton    m_openFolderButton;

    // The last layout, so showing or hiding the skipped row can lay the page
    // out again at once.
    bool           m_hasLayout  = false;
    RECT           m_lastRect   = {};
    DxuiDpiScaler  m_lastScaler;
};
