#pragma once

#include "Pch.h"
#include "Window/DxuiDialogWindow.h"


class DxuiButton;
class DxuiPanel;




////////////////////////////////////////////////////////////////////////////////
//
//  DownloadDialog
//
//  DxuiDialogWindow hosting StartupDownloadDialog's intro + asset-row
//  content and the Download / Skip / Exit buttons. Download carries a
//  custom click handler (start workers, relabel -- it must NOT close);
//  Skip auto-closes, leaving the default Skipped result; Exit is the
//  IDCANCEL button (Escape / close-box) with a custom handler that records
//  the Exit result. A fast tick polls the workers via the OnPoll hook.
//
////////////////////////////////////////////////////////////////////////////////

class DownloadDialog : public DxuiDialogWindow
{
public:
    // Command ids for the two non-cancel buttons. Public because the owner
    // names kIdDownload when it picks the default button for ShowModalDialog.
    static constexpr int  kIdDownload = 100;
    static constexpr int  kIdSkip     = 101;

    void  ConfigureDownload (std::unique_ptr<DxuiPanel> content, bool requiresRoms, unsigned tickMs);

    void  SetOnDownloadClick (std::function<void()> fn) { m_onDownloadClick = std::move (fn); }
    void  SetOnPoll          (std::function<void()> fn) { m_onPoll          = std::move (fn); }

    DxuiButton *  DownloadButton() const { return m_downloadBtn; }
    DxuiButton *  SkipButton() const { return m_skipBtn; }
    DxuiButton *  ExitButton() const { return m_exitBtn; }

protected:
    void  OnCreate() override;
    void  OnDialogTick() override;

private:
    std::unique_ptr<DxuiPanel>  m_pendingContent;
    bool                        m_requiresRoms    = false;
    unsigned                    m_tickMs          = 100;
    std::function<void()>       m_onDownloadClick;
    std::function<void()>       m_onPoll;
    DxuiButton  *               m_downloadBtn     = nullptr;
    DxuiButton  *               m_skipBtn         = nullptr;
    DxuiButton  *               m_exitBtn         = nullptr;
};
