#include "Pch.h"

#include "CassoExplorer/Browser/Preview/PreviewViewState.h"
#include "CassoExplorer/Browser/Preview/IPreviewRefresh.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewState::PreviewViewState
//
////////////////////////////////////////////////////////////////////////////////

PreviewViewState::PreviewViewState (IPreviewRefresh & refresh)
    : m_refresh (refresh)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewState::SetHexForm
//
//  The form applies to the file previewed now. Another file opens in its
//  default form.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewViewState::SetHexForm (PreviewDecoder::HexForm form)
{
    m_hexForm = form;
    m_refresh.UpdatePreview();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewState::SetViewAs
//
//  The view applies to the file previewed now. Another file opens in its
//  default view.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewViewState::SetViewAs (ViewChoice view)
{
    m_viewAs = view;
    m_refresh.UpdatePreview();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewState::GetHexForm
//
////////////////////////////////////////////////////////////////////////////////

PreviewDecoder::HexForm PreviewViewState::GetHexForm (const std::wstring & fileKey)
{
    if (fileKey != m_hexFormKey)
    {
        m_hexFormKey = fileKey;
        m_hexForm    = PreviewDecoder::HexForm::Auto;
        m_viewAs     = ViewChoice::Default;
    }

    return m_hexForm;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewState::SetCharset
//
////////////////////////////////////////////////////////////////////////////////

void PreviewViewState::SetCharset (Charset charset)
{
    if (!m_hexFormKey.empty())
    {
        m_decodeOptions.try_emplace (m_hexFormKey, m_defaultOptions).first->second.charset = charset;
        m_refresh.UpdatePreview();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewState::SetRecordLength
//
////////////////////////////////////////////////////////////////////////////////

void PreviewViewState::SetRecordLength (size_t recordLength)
{
    if (!m_hexFormKey.empty())
    {
        m_decodeOptions.try_emplace (m_hexFormKey, m_defaultOptions).first->second.recordLength = recordLength;
        m_refresh.UpdatePreview();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewState::SetFormulas
//
////////////////////////////////////////////////////////////////////////////////

void PreviewViewState::SetFormulas (bool formulas)
{
    if (!m_hexFormKey.empty())
    {
        m_decodeOptions.try_emplace (m_hexFormKey, m_defaultOptions).first->second.formulas = formulas;
        m_refresh.UpdatePreview();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewState::GetDecodeOptions
//
////////////////////////////////////////////////////////////////////////////////

DecodeOptions PreviewViewState::GetDecodeOptions (const std::wstring & imagePath, const std::string & entryPath) const
{
    auto           found   = m_decodeOptions.find (imagePath + L"|" + TextEncoding::NarrowToWide (entryPath));
    DecodeOptions  options = (found != m_decodeOptions.end()) ? found->second : m_defaultOptions;



    options.quickDrawFonts = m_fonts.get();
    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewState::SetQuickDrawFonts
//
////////////////////////////////////////////////////////////////////////////////

void PreviewViewState::SetQuickDrawFonts (std::unique_ptr<QuickDrawFontLibrary> fonts)
{
    if (m_fonts == nullptr)
    {
        m_fonts = std::move (fonts);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewState::SetDefaultDecodeOptions
//
////////////////////////////////////////////////////////////////////////////////

void PreviewViewState::SetDefaultDecodeOptions (const DecodeOptions & options)
{
    m_defaultOptions              = options;
    m_defaultOptions.recordLength = 0;

    for (auto & [key, fileOptions] : m_decodeOptions)
    {
        size_t  recordLength = fileOptions.recordLength;

        fileOptions              = m_defaultOptions;
        fileOptions.recordLength = recordLength;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewState::SetSavedGameShown
//
//  Whether a disk with a saved game previews as the saved game or as the
//  disk. The choice applies to every such disk until it is changed.
//
////////////////////////////////////////////////////////////////////////////////

void PreviewViewState::SetSavedGameShown (bool shown)
{
    m_savedGameShown = shown;
    m_refresh.UpdatePreview();
}
