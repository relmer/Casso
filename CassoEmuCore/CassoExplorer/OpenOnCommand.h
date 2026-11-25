#pragma once

#include "Pch.h"

#include "Seams/IProcessLauncher.h"

struct StockMachine;





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand
//
//  The Open in Casso on submenu in File Explorer's menu for a disk image, for
//  the installed package. The package's manifest attaches the class to each
//  disk image type. A copy without an installer gets the same submenu from
//  registry keys FileAssociations writes, so File Explorer doesn't load the
//  DLL from a build folder and lock it.
//
//  The top command has a subcommand per stock model. Choosing one starts the
//  Casso.exe beside the DLL with `--launch "<image>" --machine <id>`, the
//  arguments Casso Explorer's Open on rows pass.
//
//  The class is built into CassoShellExtension.dll, which File Explorer loads.
//
////////////////////////////////////////////////////////////////////////////////

class OpenOnCommand : public IExplorerCommand
{
public:
    //  {146E9396-94F5-4962-A61F-DC6AE6CC30A4}
    static constexpr CLSID  kClsid = { 0x146e9396, 0x94f5, 0x4962, { 0xa6, 0x1f, 0xdc, 0x6a, 0xe6, 0xcc, 0x30, 0xa4 } };

    //  The top command, with one reference the caller releases.
    static HRESULT  Create (std::shared_ptr<IProcessLauncher> launcher, const std::wstring & cassoExe, REFIID riid, void ** ppv);

    //  A class factory for the top command, for DllGetClassObject.
    static HRESULT  CreateClassFactory (REFIID riid, void ** ppv);

    //  The commands, enumerators and class factories alive, for DllCanUnloadNow.
    static LONG  GetObjectCount() { return s_objects.load(); }

    //  Starts Casso on this command's model for each image.
    HRESULT  OpenImages (const std::vector<std::wstring> & paths);

    STDMETHODIMP          QueryInterface (REFIID riid, void ** ppv) override;
    STDMETHODIMP_(ULONG)  AddRef         () override;
    STDMETHODIMP_(ULONG)  Release        () override;

    //  IExplorerCommand
    STDMETHODIMP  GetTitle         (IShellItemArray * items, LPWSTR * name) override;
    STDMETHODIMP  GetIcon          (IShellItemArray * items, LPWSTR * icon) override;
    STDMETHODIMP  GetToolTip       (IShellItemArray * items, LPWSTR * tip) override;
    STDMETHODIMP  GetCanonicalName (GUID * guid) override;
    STDMETHODIMP  GetState         (IShellItemArray * items, BOOL okToBeSlow, EXPCMDSTATE * state) override;
    STDMETHODIMP  Invoke           (IShellItemArray * items, IBindCtx * context) override;
    STDMETHODIMP  GetFlags         (EXPCMDFLAGS * flags) override;
    STDMETHODIMP  EnumSubCommands  (IEnumExplorerCommand ** commands) override;

private:
    class ClassFactory;
    class Enumerator;

                      OpenOnCommand  (std::shared_ptr<IProcessLauncher> launcher, std::wstring cassoExe, const StockMachine * machine);
    virtual           ~OpenOnCommand ();

    static std::atomic<LONG>  s_objects;

    std::atomic<ULONG>                 m_refCount = 1;
    std::shared_ptr<IProcessLauncher>  m_launcher;
    std::wstring                       m_cassoExe;
    const StockMachine               * m_machine  = nullptr;   // null for the top command
};
