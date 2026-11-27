#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler
//
//  File Explorer's drag-and-drop handler for a right-drag out of Casso
//  Explorer. File Explorer draws the menu for a right-drag into its windows,
//  so its Copy here writes each file in its conversion, and this handler adds
//  Copy here without conversion (raw bytes) and Copy here as hex dump beside
//  it.
//
//  Casso Explorer marks a drag whose files have a conversion with
//  kOfferedFormat, unless Options has the item turned off, and a drag of
//  files with kHexDumpOfferedFormat. When the user picks an item, the
//  handler stores the target folder's path on the drag's data object in
//  kFolderFormat or kHexDumpFolderFormat, and File Explorer copies nothing.
//  Casso Explorer reads the folder when the drag ends and copies the
//  selection there raw or as hex dumps, with its collision prompt and its
//  report, as Get does.
//
//  The class is built into CassoShellExtension.dll, which File Explorer loads.
//  A drag without the mark doesn't get the item, so other programs' drags
//  are unaffected.
//
////////////////////////////////////////////////////////////////////////////////

class RawCopyDropHandler : public IShellExtInit, public IContextMenu
{
public:
    static constexpr const wchar_t *  kOfferedFormat = L"CassoExplorer.RawCopyOffered";
    static constexpr const wchar_t *  kFolderFormat  = L"CassoExplorer.RawCopyFolder";
    static constexpr const wchar_t *  kMenuText      = L"Copy here &without conversion (raw bytes)";
    static constexpr const wchar_t *  kVerb          = L"CassoRawCopy";
    static constexpr const char *     kVerbA         = "CassoRawCopy";

    static constexpr const wchar_t *  kHexDumpOfferedFormat = L"CassoExplorer.HexDumpOffered";
    static constexpr const wchar_t *  kHexDumpFolderFormat  = L"CassoExplorer.HexDumpFolder";
    static constexpr const wchar_t *  kHexDumpMenuText      = L"Copy here as &hex dump";
    static constexpr const wchar_t *  kHexDumpVerb          = L"CassoHexDump";
    static constexpr const char *     kHexDumpVerbA         = "CassoHexDump";

    //  The commands' offsets from the menu's first command id.
    static constexpr UINT  kRawCopyCommand = 0;
    static constexpr UINT  kHexDumpCommand = 1;

    //  {8E3B6C41-2F7A-4D59-B1C8-6A0E9D24F713}
    static constexpr CLSID  kClsid    = { 0x8e3b6c41, 0x2f7a, 0x4d59, { 0xb1, 0xc8, 0x6a, 0x0e, 0x9d, 0x24, 0xf7, 0x13 } };

    //  {E79562C9-1380-4A9B-90BF-61BED8E3F1EA}: the same handler, registered
    //  from a developer build beside the release's.
    static constexpr CLSID  kDevClsid = { 0xe79562c9, 0x1380, 0x4a9b, { 0x90, 0xbf, 0x61, 0xbe, 0xd8, 0xe3, 0xf1, 0xea } };

    //  A handler with one reference the caller releases.
    static HRESULT  Create (REFIID riid, void ** ppv);

    //  The handlers and class factories alive, for DllCanUnloadNow.
    static LONG  GetObjectCount() { return s_objects.load(); }

    //  A class factory for the handler, for DllGetClassObject.
    static HRESULT  CreateClassFactory (REFIID riid, void ** ppv);

    //  The folder the handler stored, from the stored bytes.
    static std::wstring  GetFolder (const std::vector<uint8_t> & bytes);

    STDMETHODIMP          QueryInterface (REFIID riid, void ** ppv) override;
    STDMETHODIMP_(ULONG)  AddRef         () override;
    STDMETHODIMP_(ULONG)  Release        () override;

    //  IShellExtInit
    STDMETHODIMP  Initialize (PCIDLIST_ABSOLUTE folder, IDataObject * data, HKEY progId) override;

    //  IContextMenu
    STDMETHODIMP  QueryContextMenu (HMENU menu, UINT index, UINT firstCommand, UINT lastCommand, UINT flags) override;
    STDMETHODIMP  InvokeCommand    (CMINVOKECOMMANDINFO * info) override;
    STDMETHODIMP  GetCommandString (UINT_PTR command, UINT type, UINT * reserved, CHAR * name, UINT nameChars) override;

private:
    class ClassFactory;

                      RawCopyDropHandler  ();
    virtual           ~RawCopyDropHandler ();

    //  True when `data` has a mark Casso Explorer sets.
    static bool  IsOffered (IDataObject * data, const wchar_t * offered);

    //  The command `info` picks, by offset or by verb, or -1 for none of the
    //  handler's.
    static int  GetPickedCommand (const CMINVOKECOMMANDINFO * info);

    static std::atomic<LONG>  s_objects;

    std::atomic<ULONG>                     m_refCount   = 1;
    Microsoft::WRL::ComPtr<IDataObject>    m_data;
    std::wstring                           m_folder;     // the target folder's path, empty when it isn't a file system folder
    bool                                   m_rawOffered = false;
    bool                                   m_hexOffered = false;
};
