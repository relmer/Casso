#include "Pch.h"

#include "CassoExplorer/RawCopyDropHandler.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RawCopyDropHandlerTests
//
//  The handler as File Explorer drives it on a right-drop: Initialize with
//  the target folder and the drag's data object, QueryContextMenu on the
//  drop menu, and InvokeCommand for the picked item. The data object is a
//  DxuiDragDropSource, which stores the handler's SetData as Casso
//  Explorer's drag does.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RawCopyDropHandlerTests)
{
public:

    struct Fixture
    {
        DxuiDragDropSource             * source  = nullptr;
        ComPtr<IShellExtInit>            init;
        ComPtr<IContextMenu>             menu;
        PIDLIST_ABSOLUTE                 folder  = nullptr;
        HMENU                            popup   = nullptr;
        std::wstring                     path;

        ~Fixture()
        {
            if (source != nullptr)
            {
                source->Release();
            }

            if (folder != nullptr)
            {
                ILFree (folder);
            }

            if (popup != nullptr)
            {
                DestroyMenu (popup);
            }
        }
    };


    //  A data object with or without Casso Explorer's mark, a handler
    //  initialized with it, and an empty menu. `target` is the folder the
    //  handler is initialized with, the temporary folder when it is null.
    static void  Build (Fixture & fixture, bool offered, PIDLIST_ABSOLUTE target = nullptr)
    {
        std::vector<DxuiDragDropSource::Format>  formats;
        wchar_t                                  temp[MAX_PATH] = {};
        wchar_t                                  full[MAX_PATH] = {};

        if (offered)
        {
            formats.push_back ({ (CLIPFORMAT) RegisterClipboardFormatW (RawCopyDropHandler::kOfferedFormat), 1, [] (int, std::vector<uint8_t> & out)
            {
                out.assign (1, 0);
                return S_OK;
            } });
        }

        Assert::AreEqual (S_OK, DxuiDragDropSource::Create (std::move (formats), &fixture.source));
        Assert::AreEqual (S_OK, RawCopyDropHandler::Create (IID_PPV_ARGS (&fixture.init)));
        Assert::AreEqual (S_OK, fixture.init.As (&fixture.menu));

        //  The long form, as SHGetPathFromIDListW returns it.
        Assert::IsTrue (GetTempPathW (MAX_PATH, temp) > 0);
        Assert::IsTrue (GetLongPathNameW (temp, full, MAX_PATH) > 0);
        fixture.path = full;

        while (!fixture.path.empty() && fixture.path.back() == L'\\')
        {
            fixture.path.pop_back();
        }

        fixture.folder = (target != nullptr) ? target : ILCreateFromPathW (fixture.path.c_str());
        Assert::IsNotNull (fixture.folder);

        fixture.popup = CreatePopupMenu();
        Assert::IsNotNull (fixture.popup);

        Assert::AreEqual (S_OK, fixture.init->Initialize (fixture.folder, static_cast<IDataObject *> (fixture.source), nullptr));
    }


    static std::wstring  GetItemText (HMENU menu, int position)
    {
        wchar_t  text[128] = {};

        GetMenuStringW (menu, (UINT) position, text, ARRAYSIZE (text), MF_BYPOSITION);
        return text;
    }


    TEST_METHOD (AMarkedDrag_GetsTheItem_AndPickingItStoresTheFolder)
    {
        Fixture               fixture;
        CMINVOKECOMMANDINFO   info    = { sizeof (info) };
        std::vector<uint8_t>  stored;
        HRESULT               hr      = S_OK;

        Build (fixture, true);

        hr = fixture.menu->QueryContextMenu (fixture.popup, 0, 100, 200, CMF_NORMAL);
        Assert::AreEqual (MAKE_HRESULT (SEVERITY_SUCCESS, FACILITY_NULL, 1), hr, L"one command id used");
        Assert::AreEqual (1, GetMenuItemCount (fixture.popup));
        Assert::AreEqual (std::wstring (RawCopyDropHandler::kMenuText), GetItemText (fixture.popup, 0));
        Assert::AreEqual (100u, GetMenuItemID (fixture.popup, 0));

        info.lpVerb = MAKEINTRESOURCEA (0);
        Assert::AreEqual (S_OK, fixture.menu->InvokeCommand (&info));

        Assert::IsTrue   (fixture.source->GetStoredBytes (RawCopyDropHandler::kFolderFormat, stored));
        Assert::AreEqual (fixture.path, RawCopyDropHandler::GetFolder (stored));
    }


    TEST_METHOD (TheItemIsPickedByItsVerb)
    {
        Fixture               fixture;
        CMINVOKECOMMANDINFO   info    = { sizeof (info) };
        std::vector<uint8_t>  stored;

        Build (fixture, true);
        fixture.menu->QueryContextMenu (fixture.popup, 0, 1, 10, CMF_NORMAL);

        info.lpVerb = RawCopyDropHandler::kVerbA;
        Assert::AreEqual (S_OK, fixture.menu->InvokeCommand (&info));
        Assert::IsTrue   (fixture.source->GetStoredBytes (RawCopyDropHandler::kFolderFormat, stored));

        info.lpVerb = "open";
        Assert::AreEqual (E_FAIL, fixture.menu->InvokeCommand (&info), L"another verb is not the handler's");
    }


    TEST_METHOD (AnUnmarkedDrag_GetsNoItem)
    {
        Fixture               fixture;
        CMINVOKECOMMANDINFO   info    = { sizeof (info) };
        std::vector<uint8_t>  stored;

        Build (fixture, false);

        Assert::AreEqual (MAKE_HRESULT (SEVERITY_SUCCESS, FACILITY_NULL, 0), fixture.menu->QueryContextMenu (fixture.popup, 0, 1, 10, CMF_NORMAL));
        Assert::AreEqual (0, GetMenuItemCount (fixture.popup));

        info.lpVerb = MAKEINTRESOURCEA (0);
        Assert::AreEqual (E_FAIL, fixture.menu->InvokeCommand (&info));
        Assert::IsFalse  (fixture.source->GetStoredBytes (RawCopyDropHandler::kFolderFormat, stored));
    }


    TEST_METHOD (ADefaultOnlyMenu_GetsNoItem)
    {
        Fixture  fixture;

        Build (fixture, true);

        Assert::AreEqual (MAKE_HRESULT (SEVERITY_SUCCESS, FACILITY_NULL, 0), fixture.menu->QueryContextMenu (fixture.popup, 0, 1, 10, CMF_DEFAULTONLY));
        Assert::AreEqual (0, GetMenuItemCount (fixture.popup));
    }


    TEST_METHOD (AFolderOutsideTheFileSystem_GetsNoItem)
    {
        Fixture           fixture;
        PIDLIST_ABSOLUTE  controlPanel = nullptr;

        Assert::AreEqual (S_OK, SHGetKnownFolderIDList (FOLDERID_ControlPanelFolder, 0, nullptr, &controlPanel));
        Build (fixture, true, controlPanel);

        fixture.menu->QueryContextMenu (fixture.popup, 0, 1, 10, CMF_NORMAL);
        Assert::AreEqual (0, GetMenuItemCount (fixture.popup));
    }


    TEST_METHOD (GetFolder_StopsAtTheNull)
    {
        const wchar_t         text[] = L"C:\\Out\0junk";
        std::vector<uint8_t>  bytes ((const uint8_t *) text, (const uint8_t *) text + sizeof (text));

        Assert::AreEqual (std::wstring (L"C:\\Out"), RawCopyDropHandler::GetFolder (bytes));
        Assert::AreEqual (std::wstring(),            RawCopyDropHandler::GetFolder ({}));
    }


    TEST_METHOD (DllGetClassObject_MakesTheHandler_AndCountsWhatIsAlive)
    {
        ComPtr<IClassFactory>  factory;
        ComPtr<IContextMenu>   menu;
        LONG                   before = RawCopyDropHandler::GetObjectCount();

        Assert::AreEqual (S_OK, DllGetClassObject (RawCopyDropHandler::kClsid, IID_PPV_ARGS (&factory)));
        Assert::AreEqual (S_OK, factory->CreateInstance (nullptr, IID_PPV_ARGS (&menu)));
        Assert::AreEqual (before + 2, RawCopyDropHandler::GetObjectCount());
        Assert::AreEqual (S_FALSE, DllCanUnloadNow());

        menu.Reset();
        factory.Reset();
        Assert::AreEqual (before, RawCopyDropHandler::GetObjectCount());

        Assert::AreEqual (CLASS_E_CLASSNOTAVAILABLE, DllGetClassObject (CLSID_DragDropHelper, IID_PPV_ARGS (&factory)));
    }
};
