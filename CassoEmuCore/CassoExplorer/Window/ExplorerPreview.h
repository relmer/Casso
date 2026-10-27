#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "CassoExplorer/ICassoExplorerHost.h"
#include "CassoExplorer/Model/FocusRing.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Config/IFileSystem.h"
#include "Seams/Win32HostDialogs.h"
#include "Seams/Win32IntentChannel.h"
#include "Seams/Win32ProcessLauncher.h"
#include "CassoExplorer/CrtPictureRenderer.h"
#include "Seams/Win32ContentDetails.h"
#include "Ui/Debugger/SourceSyntax.h"
#include "Ui/ThemeLoader.h"
#include "Config/CrtResolver.h"
#include "Config/CrtPresets.h"
#include "Seams/Win32ShellIcons.h"
#include "Seams/Win32InfoTips.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "Seams/Win32PackagedCommands.h"
#include "Ui/Chrome/CassoTheme.h"
#include "CassoExplorer/Model/FolderWatch.h"
#include "CassoExplorer/Model/RefreshAnchor.h"

class CassoLinkConnections;
class FirewallMonitor;
class RunningCassoRegistry;
class ILinkAnnouncements;
struct CassoExplorerSessionWindow;
class CassoExplorerWindow;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreview
//
//  The Preview pane: its content for the selection, the picture through
//  Casso's CRT pass, the shell's preview handler, the hex and text views'
//  settings, go to and find, the pane's menus, and an archive's files.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerPreview
{
public:
    explicit ExplorerPreview (CassoExplorerWindow & window);

    static constexpr const wchar_t *  kSelectToPreview = L"Select a file to preview.";
    static constexpr float            kPromptFontDip   = 12.0f;   // Explorer's, 9 points

    //  Where DOS 3.3 on a 48K machine leaves HIMEM, against which Integer BASIC
    //  keeps its program.
    static constexpr int  kIntegerBasicHimem   = 0x9600;

    //  How long a shell preview has before the bytes replace it.
    static constexpr UINT  kShellPreviewTimeoutMs = 5000;

    //  The text view's rows for a preview; an assembler's source with its line
    //  numbers and, given colors, its syntax colored.
    static std::vector<DxuiTextView::Row>  BuildTextRows   (const PreviewContent & preview, bool lineAddresses, const SourceSyntax::Colors & colors = {});

    //  Visual Studio's code colors for the theme's background, dark or light,
    //  as Casso's debugger colors its source.
    static SourceSyntax::Colors            GetSourceColors (const IDxuiTheme * theme);

    //  Half of the largest luminance GetSourceColors weighs, 255 times 1000.
    static constexpr uint32_t              s_kDarkLumaLimit = 127500;

    ////////////////////////////////////////////////////////////////////////////
    //
    //  PreviewBytes
    //
    //  The previewed file's bytes, as a source for the hex view. The browser
    //  owns the preview content and replaces it on every selection change, so
    //  this stores a pointer rather than a copy, updated each time the preview
    //  is refilled.
    //
    ////////////////////////////////////////////////////////////////////////////
    class PreviewBytes : public IDxuiHexSource
    {
    public:
        void  SetBytes (const std::vector<Byte> * bytes) { m_bytes = bytes; }

        uint64_t  GetByteCount() const override
        {
            return (m_bytes != nullptr) ? (uint64_t) m_bytes->size() : 0;
        }

        void  ReadBytes (uint64_t offset, std::span<uint8_t> out) const override
        {
            size_t  start = (size_t) offset;

            for (size_t idx = 0; idx < out.size(); idx++)
            {
                out[idx] = (m_bytes != nullptr && (start + idx) < m_bytes->size())
                         ? (uint8_t) (*m_bytes)[start + idx]
                         : (uint8_t) 0;
            }
        }

    private:
        const std::vector<Byte> *  m_bytes = nullptr;
    };

    ////////////////////////////////////////////////////////////////////////////
    //
    //  FileBytes
    //
    //  A host file, as a source for the hex view. The view asks only for the
    //  rows it draws, and those come from a 1 MB window of the file that moves
    //  when a row falls outside it, so a file of any size costs about the
    //  same. A read larger than half the window, as a search makes, goes to
    //  the file directly.
    //
    ////////////////////////////////////////////////////////////////////////////
    class FileBytes : public IDxuiHexSource
    {
    public:
        ~FileBytes() override { Close(); }

        bool  Open  (const std::wstring & path);
        void  Close ();

        //  Which character set the start of the file is text in, if any.
        HostCharset  GetCharset () const;

        const std::wstring &  GetPath  () const { return m_path; }

        //  Whether the file at the path has the size and write time it had
        //  when opened; false once another program changed or replaced it.
        bool                  IsCurrent () const;

        //  Why the last Open failed, or S_OK.
        HRESULT               GetError () const { return m_error; }

        uint64_t  GetByteCount () const override { return m_size; }
        void      ReadBytes    (uint64_t offset, std::span<uint8_t> out) const override;

        static constexpr size_t  kWindowBytes     = 1024 * 1024;
        static constexpr size_t  kTextSampleBytes = 64 * 1024;

    private:
        void  ReadAt (uint64_t offset, std::span<uint8_t> out) const;

        HANDLE                        m_file       = INVALID_HANDLE_VALUE;
        HRESULT                       m_error      = S_OK;
        std::wstring                  m_path;
        uint64_t                      m_size       = 0;
        FILETIME                      m_written    = {};
        mutable std::vector<uint8_t>  m_window;
        mutable uint64_t              m_windowBase = 0;
    };

    //  The hex view's character column for a preview's bytes: the host
    //  file's own character set, or Apple text for a file from an image.
    static DxuiHexView::TextEncoding  GetHexEncoding (bool hostFile, HostCharset charset);
    void   FillPreview();
    void  UpdateShellPreview      (const PreviewContent & preview);
    void  OnShellPreviewResult    ();
    void  ShowShellPreviewBytes   ();
    void  ShowTextContextMenu (int x, int y);
    void  GoToTyped (const std::wstring & text);
    void  SetHexGrouping (int grouping);

    //  Whether the preview pane currently displays the hex view rather than
    //  lines, a picture or a message. Key, copy and Tab routing in the pane
    //  depend on it.
    bool  IsHexPreviewShowing() const;
    bool  IsTextPreviewShowing() const;

    //  The text view's rows for a preview: a BASIC line as its number and its
    //  statement, a detail as its label and value, anything else as one cell.
    static std::vector<Word>               GetLineAddresses (const std::vector<Byte> & program, bool integerBasic);
    void  OnSearchChanged  (const std::wstring & text);
    void  FindNext         (bool incremental = false);
    void  SetHexColumns    (int columns);
    void  SetPreviewZoom   (int percent);

    static std::wstring  FormatPreviewError (const std::wstring & message);

    void  SetHexFormat     (const char * format);
    void  SetHexShowValues (bool show);
    int   GetPreviewStopIndex (int commandId) const;

    static DxuiHexView::ValueFormat  ParseHexFormat (const std::string & name);
    static std::vector<std::wstring>       SplitLineNumber (const std::wstring & line);

    //  A picture preview on its monitor, through Casso's CRT pass.
    PicturePreview::Monitor   GetPreviewMonitor     () const;
    const ThemeCrtDefaults *  GetPreviewCrtDefaults ();
    void                      UpdateCrtPicture      (bool force);
    void                      SelectMonitor         (PicturePreview::Monitor monitor);
    void                      ShowPictureContextMenu (int x, int y);
    void                      ShowPictureForm        (bool color);

    //  The archive files selected in the preview, by the shell's names for
    //  them, and what happens to them.
    std::vector<std::wstring>  GetSelectedArchiveIds () const;
    bool                       RouteArchiveMouse     (const DxuiMouseEvent & ev, POINT point);
    void                       OpenArchiveRow        (int row);
    void                       CopyArchiveSelection  (bool cut);
    void                       DeleteArchiveSelection ();
    void                       ShowArchiveContextMenu (int x, int y);
    bool  IsArchiveLocation          () const;

private:
    friend class CassoExplorerWindow;

    CassoExplorerWindow  & m_window;

    CrtPictureRenderer  m_crtPicture;
    int                 m_crtWidth      = 0;
    int                 m_crtHeight     = 0;
    std::string         m_crtThemeName;
    LoadedTheme         m_crtTheme;
    bool                m_crtThemeValid = false;
    PreviewBytes        m_previewBytes;
    FileBytes           m_fileBytes;
    bool                m_archiveDrag   = false;   // a left press on one of them, which may become a drag
    bool                m_archiveMenu   = false;   // a right press on one, whose menu opens on release
    POINT               m_archivePress  = {};

    //  The last search, as typed and as the bytes it matches.
    std::wstring             m_findTyped;
    bool                     m_findIsText    = false;
};
