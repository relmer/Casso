#include "Pch.h"

#include "CassoExplorer/OpenOnCommand.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "Config/FileAssociations.h"
#include "Config/StockMachineList.h"
#include "Seams/Win32ProcessLauncher.h"





std::atomic<LONG>  OpenOnCommand::s_objects = 0;





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::ClassFactory
//
//  The top command starts the Casso.exe in the DLL's folder.
//
////////////////////////////////////////////////////////////////////////////////

class OpenOnCommand::ClassFactory : public IClassFactory
{
public:
    ClassFactory()          { s_objects++; }
    virtual ~ClassFactory() { s_objects--; }

    STDMETHODIMP QueryInterface (REFIID riid, void ** ppv) override
    {
        HRESULT  hr = S_OK;



        CBREx (ppv != nullptr, E_POINTER);

        *ppv = nullptr;
        CBREx (riid == IID_IUnknown || riid == IID_IClassFactory, E_NOINTERFACE);

        *ppv = static_cast<IClassFactory *> (this);
        AddRef();

    Error:
        return hr;
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        return ++m_refCount;
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG  count = --m_refCount;



        if (count == 0)
        {
            delete this;
        }

        return count;
    }

    STDMETHODIMP CreateInstance (IUnknown * outer, REFIID riid, void ** ppv) override
    {
        HRESULT   hr               = S_OK;
        HMODULE   module           = nullptr;
        wchar_t   path[MAX_PATH]   = {};
        DWORD     length           = 0;
        BOOL      found            = FALSE;



        CBREx (outer == nullptr, CLASS_E_NOAGGREGATION);

        found = GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                    reinterpret_cast<LPCWSTR> (&OpenOnCommand::s_objects), &module);
        CWR (found);

        length = GetModuleFileNameW (module, path, ARRAYSIZE (path));
        CBREx (length > 0 && length < MAX_PATH, HRESULT_FROM_WIN32 (ERROR_INSUFFICIENT_BUFFER));

        hr = OpenOnCommand::Create (std::make_shared<Win32ProcessLauncher>(),
                                    LaunchCommand::GetSiblingPath (std::filesystem::path (path).parent_path().wstring(), LaunchCommand::kCassoExe),
                                    riid, ppv);
        CHR (hr);

    Error:
        return hr;
    }

    STDMETHODIMP LockServer (BOOL lock) override
    {
        if (lock)
        {
            s_objects++;
        }
        else
        {
            s_objects--;
        }

        return S_OK;
    }

private:
    std::atomic<ULONG>  m_refCount = 1;
};





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::Enumerator
//
//  The subcommands, in model order.
//
////////////////////////////////////////////////////////////////////////////////

class OpenOnCommand::Enumerator : public IEnumExplorerCommand
{
public:
    explicit Enumerator (std::vector<Microsoft::WRL::ComPtr<IExplorerCommand>> commands) : m_commands (std::move (commands)) { s_objects++; }
    virtual ~Enumerator() { s_objects--; }

    STDMETHODIMP QueryInterface (REFIID riid, void ** ppv) override
    {
        HRESULT  hr = S_OK;



        CBREx (ppv != nullptr, E_POINTER);

        *ppv = nullptr;
        CBREx (riid == IID_IUnknown || riid == IID_IEnumExplorerCommand, E_NOINTERFACE);

        *ppv = static_cast<IEnumExplorerCommand *> (this);
        AddRef();

    Error:
        return hr;
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        return ++m_refCount;
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG  count = --m_refCount;



        if (count == 0)
        {
            delete this;
        }

        return count;
    }

    STDMETHODIMP Next (ULONG count, IExplorerCommand ** commands, ULONG * fetched) override
    {
        ULONG  taken = 0;



        if (commands == nullptr)
        {
            return E_POINTER;
        }

        while (taken < count && m_next < m_commands.size())
        {
            commands[taken] = m_commands[m_next].Get();
            commands[taken]->AddRef();

            taken++;
            m_next++;
        }

        if (fetched != nullptr)
        {
            *fetched = taken;
        }

        return taken == count ? S_OK : S_FALSE;
    }

    STDMETHODIMP Skip (ULONG count) override
    {
        m_next = std::min (m_next + count, m_commands.size());
        return m_next < m_commands.size() ? S_OK : S_FALSE;
    }

    STDMETHODIMP Reset() override
    {
        m_next = 0;
        return S_OK;
    }

    STDMETHODIMP Clone (IEnumExplorerCommand ** copy) override
    {
        HRESULT       hr    = S_OK;
        Enumerator  * clone = nullptr;



        CBREx (copy != nullptr, E_POINTER);

        *copy = nullptr;
        clone = new (std::nothrow) Enumerator (m_commands);
        CBREx (clone != nullptr, E_OUTOFMEMORY);

        clone->m_next = m_next;
        *copy         = clone;

    Error:
        return hr;
    }

private:
    std::atomic<ULONG>                                     m_refCount = 1;
    std::vector<Microsoft::WRL::ComPtr<IExplorerCommand>>  m_commands;
    size_t                                                 m_next     = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::OpenOnCommand
//
////////////////////////////////////////////////////////////////////////////////

OpenOnCommand::OpenOnCommand (std::shared_ptr<IProcessLauncher> launcher, std::wstring cassoExe, const StockMachine * machine) :
    m_launcher (std::move (launcher)),
    m_cassoExe (std::move (cassoExe)),
    m_machine  (machine)
{
    s_objects++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::~OpenOnCommand
//
////////////////////////////////////////////////////////////////////////////////

OpenOnCommand::~OpenOnCommand()
{
    s_objects--;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OpenOnCommand::Create (std::shared_ptr<IProcessLauncher> launcher, const std::wstring & cassoExe, REFIID riid, void ** ppv)
{
    HRESULT          hr      = S_OK;
    OpenOnCommand  * command = new (std::nothrow) OpenOnCommand (std::move (launcher), cassoExe, nullptr);



    CBREx (command != nullptr, E_OUTOFMEMORY);

    hr = command->QueryInterface (riid, ppv);
    CHR (hr);

Error:
    if (command != nullptr)
    {
        command->Release();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::CreateClassFactory
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OpenOnCommand::CreateClassFactory (REFIID riid, void ** ppv)
{
    HRESULT          hr      = S_OK;
    ClassFactory   * factory = new (std::nothrow) ClassFactory();



    CBREx (factory != nullptr, E_OUTOFMEMORY);

    hr = factory->QueryInterface (riid, ppv);
    CHR (hr);

Error:
    if (factory != nullptr)
    {
        factory->Release();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::OpenImages
//
//  The top command has no model, so it starts nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT OpenOnCommand::OpenImages (const std::vector<std::wstring> & paths)
{
    HRESULT       hr     = S_OK;
    std::wstring  id;
    bool          exists = m_launcher->Exists (m_cassoExe);



    CBREx (m_machine != nullptr, E_NOTIMPL);
    CBREx (exists, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    id.assign (m_machine->id, m_machine->id + strlen (m_machine->id));   // ASCII

    for (const std::wstring & path : paths)
    {
        hr = m_launcher->Launch (m_cassoExe, LaunchCommand::MakeCassoArguments (path, L"", id));
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::QueryInterface
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP OpenOnCommand::QueryInterface (REFIID riid, void ** ppv)
{
    HRESULT  hr = S_OK;



    CBREx (ppv != nullptr, E_POINTER);

    *ppv = nullptr;
    CBREx (riid == IID_IUnknown || riid == IID_IExplorerCommand, E_NOINTERFACE);

    *ppv = static_cast<IExplorerCommand *> (this);
    AddRef();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::AddRef
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP_(ULONG) OpenOnCommand::AddRef()
{
    return ++m_refCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::Release
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP_(ULONG) OpenOnCommand::Release()
{
    ULONG  count = --m_refCount;



    if (count == 0)
    {
        delete this;
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::GetTitle
//
//  The submenu's label, or the model's row label.
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP OpenOnCommand::GetTitle (IShellItemArray *, LPWSTR * name)
{
    return SHStrDupW (m_machine != nullptr ? m_machine->label : FileAssociations::kOpenOnLabel, name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::GetIcon
//
//  Casso's icon beside the submenu's label; the model rows have none.
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP OpenOnCommand::GetIcon (IShellItemArray *, LPWSTR * icon)
{
    HRESULT  hr = S_OK;



    CBREx (icon != nullptr, E_POINTER);

    *icon = nullptr;
    CBREx (m_machine == nullptr, E_NOTIMPL);

    hr = SHStrDupW ((m_cassoExe + L",0").c_str(), icon);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::GetToolTip
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP OpenOnCommand::GetToolTip (IShellItemArray *, LPWSTR * tip)
{
    if (tip != nullptr)
    {
        *tip = nullptr;
    }

    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::GetCanonicalName
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP OpenOnCommand::GetCanonicalName (GUID * guid)
{
    if (guid == nullptr)
    {
        return E_POINTER;
    }

    *guid = GUID_NULL;
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::GetState
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP OpenOnCommand::GetState (IShellItemArray *, BOOL, EXPCMDSTATE * state)
{
    if (state == nullptr)
    {
        return E_POINTER;
    }

    *state = ECS_ENABLED;
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::Invoke
//
//  The file system path of each selected image. An item with no path, such
//  as one inside a ZIP folder, is passed over.
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP OpenOnCommand::Invoke (IShellItemArray * items, IBindCtx *)
{
    HRESULT                    hr    = S_OK;
    DWORD                      count = 0;
    std::vector<std::wstring>  paths;



    CBREx (items != nullptr, E_INVALIDARG);

    hr = items->GetCount (&count);
    CHR (hr);

    for (DWORD i = 0; i < count; i++)
    {
        Microsoft::WRL::ComPtr<IShellItem>  item;
        LPWSTR                              path   = nullptr;
        HRESULT                             hrItem = items->GetItemAt (i, &item);

        if (SUCCEEDED (hrItem))
        {
            hrItem = item->GetDisplayName (SIGDN_FILESYSPATH, &path);
        }

        if (SUCCEEDED (hrItem))
        {
            paths.emplace_back (path);
            CoTaskMemFree (path);
        }
    }

    hr = OpenImages (paths);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::GetFlags
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP OpenOnCommand::GetFlags (EXPCMDFLAGS * flags)
{
    if (flags == nullptr)
    {
        return E_POINTER;
    }

    *flags = m_machine == nullptr ? ECF_HASSUBCOMMANDS : ECF_DEFAULT;
    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommand::EnumSubCommands
//
//  A subcommand per stock model, in the order of kStockMachines.
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP OpenOnCommand::EnumSubCommands (IEnumExplorerCommand ** commands)
{
    HRESULT                                                hr         = S_OK;
    std::vector<Microsoft::WRL::ComPtr<IExplorerCommand>>  rows;
    Enumerator                                           * enumerator = nullptr;



    CBREx (commands != nullptr, E_POINTER);

    *commands = nullptr;
    CBREx (m_machine == nullptr, E_NOTIMPL);

    for (const StockMachine & machine : kStockMachines)
    {
        Microsoft::WRL::ComPtr<IExplorerCommand>  row;

        row.Attach (new (std::nothrow) OpenOnCommand (m_launcher, m_cassoExe, &machine));
        CBREx (row != nullptr, E_OUTOFMEMORY);

        rows.push_back (std::move (row));
    }

    enumerator = new (std::nothrow) Enumerator (std::move (rows));
    CBREx (enumerator != nullptr, E_OUTOFMEMORY);

    *commands = enumerator;

Error:
    return hr;
}
