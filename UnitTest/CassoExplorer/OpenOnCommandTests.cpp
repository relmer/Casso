#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FakeProcessLauncher.h"
#include "CassoExplorer/OpenOnCommand.h"
#include "Config/StockMachines.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeShellItem / FakeShellItemArray
//
//  The selection File Explorer passes to Invoke: items with a file system
//  path, or with none, as an item inside a ZIP folder has.
//
////////////////////////////////////////////////////////////////////////////////

class FakeShellItem : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IShellItem>
{
public:
    explicit FakeShellItem (std::wstring path) : m_path (std::move (path)) {}

    STDMETHODIMP BindToHandler  (IBindCtx *, REFGUID, REFIID, void **) override { return E_NOTIMPL; }
    STDMETHODIMP GetParent      (IShellItem **) override                     { return E_NOTIMPL; }
    STDMETHODIMP GetAttributes  (SFGAOF, SFGAOF *) override                  { return E_NOTIMPL; }
    STDMETHODIMP Compare        (IShellItem *, SICHINTF, int *) override     { return E_NOTIMPL; }

    STDMETHODIMP GetDisplayName (SIGDN form, LPWSTR * name) override
    {
        return (form == SIGDN_FILESYSPATH && !m_path.empty()) ? SHStrDupW (m_path.c_str(), name) : E_FAIL;
    }

private:
    std::wstring  m_path;
};


class FakeShellItemArray : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IShellItemArray>
{
public:
    explicit FakeShellItemArray (std::vector<std::wstring> paths)
    {
        for (const std::wstring & path : paths)
        {
            m_items.push_back (Microsoft::WRL::Make<FakeShellItem> (path));
        }
    }

    STDMETHODIMP BindToHandler              (IBindCtx *, REFGUID, REFIID, void **) override        { return E_NOTIMPL; }
    STDMETHODIMP GetPropertyStore           (GETPROPERTYSTOREFLAGS, REFIID, void **) override      { return E_NOTIMPL; }
    STDMETHODIMP GetPropertyDescriptionList (REFPROPERTYKEY, REFIID, void **) override             { return E_NOTIMPL; }
    STDMETHODIMP GetAttributes              (SIATTRIBFLAGS, SFGAOF, SFGAOF *) override             { return E_NOTIMPL; }
    STDMETHODIMP EnumItems                  (IEnumShellItems **) override                          { return E_NOTIMPL; }

    STDMETHODIMP GetCount (DWORD * count) override
    {
        *count = (DWORD) m_items.size();
        return S_OK;
    }

    STDMETHODIMP GetItemAt (DWORD index, IShellItem ** item) override
    {
        return index < m_items.size() ? m_items[index].CopyTo (item) : E_INVALIDARG;
    }

private:
    std::vector<ComPtr<FakeShellItem>>  m_items;
};





////////////////////////////////////////////////////////////////////////////////
//
//  OpenOnCommandTests
//
//  The package's Open in Casso on submenu as File Explorer drives it: the top
//  command's title and flags, its subcommands, and what choosing one starts.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (OpenOnCommandTests)
{
public:

    static constexpr const wchar_t *  kCasso = L"C:\\Program Files\\Casso\\Casso.exe";


    static ComPtr<IExplorerCommand> MakeTop (const std::shared_ptr<FakeProcessLauncher> & launcher)
    {
        ComPtr<IExplorerCommand>  top;

        AssertSucceeded (OpenOnCommand::Create (launcher, kCasso, IID_PPV_ARGS (&top)));
        return top;
    }


    static std::vector<ComPtr<IExplorerCommand>> GetRows (IExplorerCommand & top)
    {
        ComPtr<IEnumExplorerCommand>           enumerator;
        std::vector<ComPtr<IExplorerCommand>>  rows;
        ComPtr<IExplorerCommand>               row;
        ULONG                                  fetched = 0;

        AssertSucceeded (top.EnumSubCommands (&enumerator));

        while (enumerator->Next (1, &row, &fetched) == S_OK && fetched == 1)
        {
            rows.push_back (row);
            row.Reset();
        }

        return rows;
    }


    static std::wstring GetTitle (IExplorerCommand & command)
    {
        LPWSTR        name = nullptr;
        std::wstring  title;

        AssertSucceeded (command.GetTitle (nullptr, &name));
        title = name;
        CoTaskMemFree (name);

        return title;
    }



    TEST_METHOD (TopCommand_IsASubmenuLabeledOpenInCassoOn)
    {
        auto                      launcher = std::make_shared<FakeProcessLauncher>();
        ComPtr<IExplorerCommand>  top      = MakeTop (launcher);
        EXPCMDFLAGS               flags    = 0;
        EXPCMDSTATE               state    = 0;

        AssertSucceeded (top->GetFlags (&flags));
        AssertSucceeded (top->GetState (nullptr, FALSE, &state));

        Assert::AreEqual (std::wstring (L"Open in Casso on"), GetTitle (*top.Get()));
        Assert::IsTrue   ((flags & ECF_HASSUBCOMMANDS) != 0);
        Assert::IsTrue   (state == ECS_ENABLED);
    }



    TEST_METHOD (Subcommands_AreTheStockModelsInOrder)
    {
        auto                                   launcher = std::make_shared<FakeProcessLauncher>();
        ComPtr<IExplorerCommand>               top      = MakeTop (launcher);
        std::vector<ComPtr<IExplorerCommand>>  rows     = GetRows (*top.Get());
        std::span<const StockMachine>          machines = StockMachines::GetAll();

        Assert::AreEqual (machines.size(), rows.size());

        for (size_t i = 0; i < rows.size(); i++)
        {
            EXPCMDFLAGS  flags = ECF_HASSUBCOMMANDS;

            AssertSucceeded (rows[i]->GetFlags (&flags));
            Assert::AreEqual (std::wstring (machines[i].label), GetTitle (*rows[i].Get()));
            Assert::IsTrue   ((flags & ECF_HASSUBCOMMANDS) == 0, L"a model row has no submenu");
        }
    }



    TEST_METHOD (ChoosingAModel_StartsCassoOnItForEachImage)
    {
        auto                                   launcher = std::make_shared<FakeProcessLauncher>();
        ComPtr<IExplorerCommand>               top      = MakeTop (launcher);
        std::vector<ComPtr<IExplorerCommand>>  rows     = GetRows (*top.Get());
        ComPtr<IShellItemArray>                items    = Microsoft::WRL::Make<FakeShellItemArray> (std::vector<std::wstring> { L"C:\\disks\\game.dsk", L"", L"C:\\my disks\\hd.po" });

        launcher->present.push_back (kCasso);

        AssertSucceeded (rows[4]->Invoke (items.Get(), nullptr));

        Assert::AreEqual ((size_t) 2, launcher->launched.size(), L"the item without a path is passed over");
        Assert::AreEqual (std::wstring (kCasso), launcher->launched[0].exePath);
        Assert::AreEqual (std::wstring (L"--launch \"C:\\disks\\game.dsk\" --machine Apple2c"),    launcher->launched[0].arguments);
        Assert::AreEqual (std::wstring (L"--launch \"C:\\my disks\\hd.po\" --machine Apple2c"),    launcher->launched[1].arguments);
    }



    TEST_METHOD (WithoutCassoBesideTheDll_StartsNothing)
    {
        auto                                   launcher = std::make_shared<FakeProcessLauncher>();
        ComPtr<IExplorerCommand>               top      = MakeTop (launcher);
        std::vector<ComPtr<IExplorerCommand>>  rows     = GetRows (*top.Get());
        ComPtr<IShellItemArray>                items    = Microsoft::WRL::Make<FakeShellItemArray> (std::vector<std::wstring> { L"C:\\disks\\game.dsk" });

        HRESULT                                hr       = rows[0]->Invoke (items.Get(), nullptr);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (launcher->launched.empty());
    }



    TEST_METHOD (Commands_AreFreedWhenReleased)
    {
        LONG  before = OpenOnCommand::GetObjectCount();

        {
            auto                      launcher = std::make_shared<FakeProcessLauncher>();
            ComPtr<IExplorerCommand>  top      = MakeTop (launcher);

            GetRows (*top.Get());
        }

        Assert::AreEqual (before, OpenOnCommand::GetObjectCount(), L"File Explorer can unload the DLL");
    }
};
