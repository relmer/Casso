#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/PreviewSaveChoices.h"
#include "FileTypes/FileFormatTable.h"
#include "Machines/Apple2/Common/VolumeImage.h"

struct ExplorerContext;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewOutput
//
//  The preview's Save as and Copy.
//
//  Save as offers PreviewSaveChoices' types for a file in a disk image; for
//  any other file, the current view's types only, since the others read the
//  file through its image. The type is read back from the extension of the
//  name picked.
//
//  Copy puts the current view on the clipboard as text, a document as RTF as
//  well, and a picture as a bitmap and a PNG as well.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPreviewOutput
{
public:
    static bool  CanSaveAs (const ExplorerContext & context);
    static void  SaveAs    (ExplorerContext & context);
    static bool  CanCopy   (const ExplorerContext & context);
    static void  Copy      (ExplorerContext & context);

private:
    //  The single file selected in a disk image, with what reading it takes.
    struct Source
    {
        std::string  image;
        std::string  catalogPath;
        FileEntry    entry;
        VolumeKind   kind   = VolumeKind::Unknown;
        FileFormat   format = FileFormat::None;
    };

    static bool     TryGetSource   (ExplorerContext & context, Source & outSource);
    static HRESULT  WriteChoice    (ExplorerContext & context, PreviewSaveChoices::Kind kind, const Source & source, const std::filesystem::path & path);
    static HRESULT  SetClipboard   (HWND owner, const std::wstring & text, const std::vector<Byte> & rtf, const PreviewContent & preview);
    static HRESULT  PutGlobal      (UINT format, const void * data, size_t bytes);

    static constexpr GUID  s_kSaveAsGuid = { 0x5f0b6b2e, 0x3c41, 0x4f7a, { 0x9e, 0x2d, 0x61, 0x8a, 0x77, 0x14, 0xc3, 0x0d } };
};
