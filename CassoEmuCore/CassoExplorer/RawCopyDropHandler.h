#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandler
//
//  File Explorer's drag-and-drop handler for a right-drag out of Casso
//  Explorer. File Explorer draws the menu for a right-drag into its windows,
//  so its Copy here writes each file in its conversion, and this handler adds
//  Copy here without conversion (raw bytes) beside it.
//
//  Casso Explorer marks a drag whose files have a conversion with
//  kOfferedFormat, unless Options has the item turned off. When the user
//  picks the item, the handler stores the target folder's path on the
//  drag's data object in kFolderFormat, and File Explorer copies nothing.
//  Casso Explorer reads the folder when the drag ends and copies the
//  selection there raw, with its collision prompt and its report, as Get
//  does.
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

    //  {8E3B6C41-2F7A-4D59-B1C8-6A0E9D24F713}
    static constexpr CLSID  kClsid = { 0x8e3b6c41, 0x2f7a, 0x4d59, { 0xb1, 0xc8, 0x6a, 0x0e, 0x9d, 0x24, 0xf7, 0x13 } };

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

    //  True when `data` has the mark Casso Explorer sets.
    static bool  IsOffered (IDataObject * data);

    //  True when `info` picks the handler's command, by offset or by verb.
    static bool  IsHandlerCommand (const CMINVOKECOMMANDINFO * info);

    static std::atomic<LONG>  s_objects;

    std::atomic<ULONG>                     m_refCount = 1;
    Microsoft::WRL::ComPtr<IDataObject>    m_data;
    std::wstring                           m_folder;   // the target folder's path, empty when it isn't a file system folder
};
