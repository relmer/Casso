#pragma once

#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "ScratchImages.h"
#include "CassoExplorer/CassoExplorerBrowser.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerBrowserTestHost
//
//  An in-memory host holding the two scratch images, shared by the browser
//  test classes.
//
////////////////////////////////////////////////////////////////////////////////

namespace CassoExplorerBrowserTestHost
{
    inline constexpr const wchar_t *  kDisks = L"C:\\Disks";



    struct Host
    {
        InMemoryFileSystem    fs;
        FakeDiskFileIo        io;
        CassoExplorerBrowser  browser { fs, io };

        Host()
        {
            SeedBytes (ScratchImages::GetDos33 (true),  L"C:\\Disks\\dos33.dsk");
            SeedBytes (ScratchImages::GetProDos (true), L"C:\\Disks\\prodos.po");
            AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\readme.txt", "hello"));

            browser.GetTreeModel().SetKnownFolders ({ kDisks });
            browser.GetTreeModel().SetDrives ({});
            browser.GetTreeModel().SetDirectoryProbe ([] (const std::wstring & path) { return _wcsicmp (path.c_str(), kDisks) == 0; });
        }

        void Seed (const char * fixture, const wchar_t * path)
        {
            FixtureProvider    fixtures;
            std::vector<Byte>  bytes;

            AssertSucceeded (fixtures.OpenFixture (fixture, bytes));
            SeedBytes (bytes, path);
        }

        void SeedBytes (const std::vector<Byte> & bytes, const wchar_t * path)
        {
            std::string  narrow;

            AssertSucceeded (fs.WriteAllText (path, std::string (bytes.begin(), bytes.end())));

            for (const wchar_t * p = path; *p != L'\0'; p++)
            {
                narrow.push_back ((char) *p);
            }

            io.files[narrow]  = bytes;
            io.stamps[narrow] = FileStamp { bytes.size(), 100 };
        }

        //  Expands the Casso root and returns the known folder's node id.
        std::wstring OpenDisksFolder()
        {
            std::vector<DxuiTreeNode>  roots;

            browser.GetTreeRoots (roots);

            std::vector<DxuiTreeNode>  folders = browser.GetTreeChildren (roots[1].id);

            Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual ((size_t) 1, folders.size());

            return folders[0].id;
        }

        std::wstring FindChildId (const std::wstring & parentId, const wchar_t * label)
        {
            for (const DxuiTreeNode & node : browser.GetTreeChildren (parentId))
            {
                if (node.label == label)
                {
                    return node.id;
                }
            }

            Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail (L"child not found");

            return std::wstring();
        }
    };



    inline int FindRow (const CassoExplorerBrowser & browser, const wchar_t * name)
    {
        const std::vector<CatalogRow> &  rows = browser.GetRows();

        for (size_t i = 0; i < rows.size(); i++)
        {
            if (rows[i].name == name)
            {
                return (int) i;
            }
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail (L"row not found");

        return -1;
    }
}
