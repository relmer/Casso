#include "Pch.h"
#include "CassoExplorer/CassoExplorerShell.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerDropTests
//
//  A drop that would put the dragged files back where they are, or onto one
//  of themselves, is turned away before any target is called: a folder dragged
//  onto its own row, or a disk image, archive or program dropped on itself.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerDropTests)
{
public:

    //  A drag with the paths as a file list, as File Explorer's drags have.
    static Microsoft::WRL::ComPtr<IDataObject> MakeDrag (const std::vector<std::wstring> & paths)
    {
        Microsoft::WRL::ComPtr<IDataObject>  data;
        FORMATETC                            format = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM                            medium = { TYMED_HGLOBAL };
        size_t                               chars  = 1;
        DROPFILES                          * drop   = nullptr;
        wchar_t                            * next   = nullptr;
        HRESULT                              hr     = S_OK;



        for (const std::wstring & path : paths)
        {
            chars += path.size() + 1;
        }

        hr = SHCreateDataObject (nullptr, 0, nullptr, nullptr, IID_PPV_ARGS (&data));
        Assert::IsTrue (SUCCEEDED (hr));

        medium.hGlobal = GlobalAlloc (GHND, sizeof (DROPFILES) + chars * sizeof (wchar_t));
        Assert::IsNotNull (medium.hGlobal);

        drop = (DROPFILES *) GlobalLock (medium.hGlobal);
        Assert::IsNotNull (drop);

        drop->pFiles = sizeof (DROPFILES);
        drop->fWide  = TRUE;
        next         = (wchar_t *) ((BYTE *) drop + sizeof (DROPFILES));

        for (const std::wstring & path : paths)
        {
            memcpy (next, path.c_str(), path.size() * sizeof (wchar_t));
            next += path.size() + 1;
        }

        GlobalUnlock (medium.hGlobal);

        hr = data->SetData (&format, &medium, TRUE);
        Assert::IsTrue (SUCCEEDED (hr));

        return data;
    }


    TEST_METHOD (DraggedPathsAreTheFileList)
    {
        auto  data  = MakeDrag ({ L"C:\\Disks\\a.dsk", L"C:\\Disks\\Games" });
        auto  paths = ExplorerDragDrop::GetDraggedPaths (data.Get());

        Assert::AreEqual ((size_t) 2, paths.size());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\Games"), paths[1]);
        Assert::IsTrue   (ExplorerDragDrop::GetDraggedPaths (nullptr).empty(), L"No drag holds no files");
    }


    TEST_METHOD (ADropOntoADraggedItemIsTurnedAway)
    {
        auto  data = MakeDrag ({ L"C:\\Disks\\a.dsk", L"C:\\Disks\\Games" });

        Assert::IsTrue  (ExplorerDragDrop::IsDraggedItem (data.Get(), L"C:\\Disks\\Games"),  L"A folder onto its own row");
        Assert::IsTrue  (ExplorerDragDrop::IsDraggedItem (data.Get(), L"c:\\disks\\A.DSK"),  L"A disk image onto itself, in any case");
        Assert::IsFalse (ExplorerDragDrop::IsDraggedItem (data.Get(), L"C:\\Disks\\b.dsk"),  L"Onto another image");
        Assert::IsFalse (ExplorerDragDrop::IsDraggedItem (data.Get(), L"C:\\Disks\\Games2"), L"Onto a folder whose path only begins with one dragged");
        Assert::IsFalse (ExplorerDragDrop::IsDraggedItem (data.Get(), L""),                  L"Onto no target");
    }


    TEST_METHOD (ADropBackIntoTheSourceFolderIsTurnedAway)
    {
        auto  data  = MakeDrag ({ L"C:\\Disks\\a.dsk", L"C:\\Disks\\Games" });
        auto  mixed = MakeDrag ({ L"C:\\Disks\\a.dsk", L"C:\\Other\\b.dsk" });

        Assert::IsTrue  (ExplorerDragDrop::IsDraggedFromFolder (data.Get(),  L"C:\\Disks"), L"Every file is already there");
        Assert::IsFalse (ExplorerDragDrop::IsDraggedFromFolder (mixed.Get(), L"C:\\Disks"), L"One file comes from elsewhere");
    }
};
