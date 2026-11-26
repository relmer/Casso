#include "Pch.h"

#include "Seams/Win32ShellMenuHandlers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeContextMenu
//
//  A context menu handler whose GetCommandString returns a verb by command
//  offset from a map, and fails for an offset not in it. It is created on the
//  stack, so AddRef and Release count nothing.
//
////////////////////////////////////////////////////////////////////////////////

class FakeContextMenu : public IContextMenu
{
public:
    IFACEMETHODIMP QueryInterface (REFIID riid, void ** ppv) override
    {
        *ppv = (riid == __uuidof (IUnknown) || riid == __uuidof (IContextMenu)) ? static_cast<IContextMenu *> (this) : nullptr;

        return (*ppv != nullptr) ? S_OK : E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override  { return 1; }
    IFACEMETHODIMP_(ULONG) Release() override { return 1; }

    IFACEMETHODIMP QueryContextMenu (HMENU, UINT, UINT, UINT, UINT) override { return E_NOTIMPL; }
    IFACEMETHODIMP InvokeCommand (CMINVOKECOMMANDINFO *) override            { return E_NOTIMPL; }

    IFACEMETHODIMP GetCommandString (UINT_PTR offset, UINT type, UINT *, CHAR * name, UINT cchMax) override
    {
        auto  found = verbs.find (offset);

        requested.push_back (offset);

        if (type != GCS_VERBW || found == verbs.end() || found->second.size() >= cchMax)
        {
            return E_INVALIDARG;
        }

        wcscpy_s ((wchar_t *) name, cchMax, found->second.c_str());
        return S_OK;
    }

    std::map<UINT_PTR, std::wstring>  verbs;
    std::vector<UINT_PTR>             requested;
};





TEST_CLASS (Win32ShellMenuHandlersTests)
{
public:

    //  A row's id is its command offset plus the first id the menu was
    //  given, so GetCommandString receives the offset.
    TEST_METHOD (GetMenuVerb_PassesTheRowsOffsetFromTheFirstId)
    {
        FakeContextMenu  menu;

        menu.verbs[0] = L".txt";
        menu.verbs[3] = L"NewFolder";

        Assert::AreEqual (std::wstring (L".txt"),      Win32ShellMenuHandlers::GetMenuVerb (&menu, Win32ShellMenuHandlers::kFirstNewCommandId));
        Assert::AreEqual (std::wstring (L"NewFolder"), Win32ShellMenuHandlers::GetMenuVerb (&menu, Win32ShellMenuHandlers::kFirstNewCommandId + 3));

        Assert::AreEqual ((size_t) 2,   menu.requested.size());
        Assert::AreEqual ((UINT_PTR) 0, menu.requested[0]);
        Assert::AreEqual ((UINT_PTR) 3, menu.requested[1]);
    }



    TEST_METHOD (GetMenuVerb_IsEmptyWhenTheHandlerFails)
    {
        FakeContextMenu  menu;

        menu.verbs[0] = L".txt";

        Assert::AreEqual (std::wstring(), Win32ShellMenuHandlers::GetMenuVerb (&menu, Win32ShellMenuHandlers::kFirstNewCommandId + 1));
        Assert::AreEqual ((size_t) 1, menu.requested.size());
    }
};
