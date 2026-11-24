#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/PreviewDecoder.h"
#include "FileTypes/DecodedFile.h"

class IPreviewRefresh;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewState
//
//  The choices made for the files previewed: the hex form and the view of the
//  file previewed now, the decode options of each file one was made for, and
//  whether a disk with a saved game previews as the saved game.
//
////////////////////////////////////////////////////////////////////////////////

class PreviewViewState
{
public:
    explicit PreviewViewState (IPreviewRefresh & refresh);

    //  The form and the view apply to the file previewed now. Another file
    //  opens in its default form and view.
    void  SetHexForm (PreviewDecoder::HexForm form);
    void  SetViewAs  (ViewChoice view);

    ViewChoice  GetViewAs () const { return m_viewAs; }

    //  The form for a file, Auto when it isn't the file previewed last.
    PreviewDecoder::HexForm  GetHexForm (const std::wstring & fileKey);

    //  The character set, the record length and the formulas choice of the
    //  file previewed now, in effect for that file until the window closes. A
    //  record length of 0 is the one found.
    void  SetCharset      (Charset charset);
    void  SetRecordLength (size_t recordLength);
    void  SetFormulas     (bool formulas);

    //  The choices made for a file, by its image's path and its path in the
    //  image; the defaults for a file with none.
    DecodeOptions  GetDecodeOptions (const std::wstring & imagePath, const std::string & entryPath) const;

    //  The options of a file whose preview hasn't set any. Setting them
    //  sets each file's options too, all but its record length.
    void                   SetDefaultDecodeOptions (const DecodeOptions & options);
    const DecodeOptions &  GetDefaultDecodeOptions () const { return m_defaultOptions; }

    //  Whether a disk with a saved game previews as the saved game or as the
    //  disk. The choice applies to every such disk until it is changed.
    void  SetSavedGameShown (bool shown);
    bool  IsSavedGameShown  () const { return m_savedGameShown; }

private:
    IPreviewRefresh  & m_refresh;

    //  The form of the file previewed now, which another file resets.
    PreviewDecoder::HexForm  m_hexForm        = PreviewDecoder::HexForm::Auto;
    std::wstring             m_hexFormKey;
    ViewChoice               m_viewAs         = ViewChoice::Default;
    bool                     m_savedGameShown = true;   // a disk with a saved game previews as the saved game, not the disk

    //  The decode choices of each file one was made for, by file key.
    std::map<std::wstring, DecodeOptions>  m_decodeOptions;
    DecodeOptions                          m_defaultOptions;
};
