#include "Pch.h"

#include "DownloadDialog.h"

#include "Core/DxuiPanel.h"
#include "Widgets/DxuiButton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigureDownload
//
////////////////////////////////////////////////////////////////////////////////

void DownloadDialog::ConfigureDownload (std::unique_ptr<DxuiPanel> content, bool requiresRoms, unsigned tickMs)
{
    m_pendingContent = std::move (content);
    m_requiresRoms   = requiresRoms;
    m_tickMs         = tickMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void DownloadDialog::OnCreate()
{
    if (m_pendingContent != nullptr)
    {
        SetDialogContentOwned (std::move (m_pendingContent));
    }

    m_downloadBtn = AddDialogButton (L"Download", kIdDownload);
    m_downloadBtn->SetOnClick ([this] () { if (m_onDownloadClick) { m_onDownloadClick(); } });

    // No Skip when a ROM is missing -- the machine cannot boot without it.
    if (!m_requiresRoms)
    {
        m_skipBtn = AddDialogButton (L"Skip", kIdSkip);
    }

    m_exitBtn = AddDialogButton (L"Exit", IDCANCEL);

    SetDialogTickIntervalMs (m_tickMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDialogTick
//
////////////////////////////////////////////////////////////////////////////////

void DownloadDialog::OnDialogTick()
{
    if (m_onPoll)
    {
        m_onPoll();
    }
}
