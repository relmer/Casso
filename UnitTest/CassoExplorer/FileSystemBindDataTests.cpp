#include "Pch.h"

#include "Seams/ShellVerbs/FileSystemBindData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemBindDataTests
//
//  The find data a delete's items are parsed with, so the shell's
//  confirmation has a card file's real size.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FileSystemBindDataTests)
{
public:

    static WIN32_FIND_DATAW  MakeFound()
    {
        static constexpr DWORD  kSize  = 143360;
        WIN32_FIND_DATAW        found  = {};

        found.dwFileAttributes = FILE_ATTRIBUTE_ARCHIVE;
        found.nFileSizeLow     = kSize;
        wcscpy_s (found.cFileName, L"CLEAN.PO");

        return found;
    }

    TEST_METHOD (TheBindContextHasTheFindData)
    {
        ComPtr<IBindCtx>             context;
        ComPtr<IUnknown>             param;
        ComPtr<IFileSystemBindData>  data;
        WIN32_FIND_DATAW             found = MakeFound();
        WIN32_FIND_DATAW             read  = {};

        Assert::AreEqual (S_OK, FileSystemBindData::CreateBindContext (found, &context));
        Assert::AreEqual (S_OK, context->GetObjectParam (const_cast<LPOLESTR> (STR_FILE_SYS_BIND_DATA), &param));
        Assert::AreEqual (S_OK, param.As (&data));
        Assert::AreEqual (S_OK, data->GetFindData (&read));

        Assert::AreEqual (found.nFileSizeLow,     read.nFileSizeLow);
        Assert::AreEqual (found.dwFileAttributes, read.dwFileAttributes);
        Assert::AreEqual (std::wstring (L"CLEAN.PO"), std::wstring (read.cFileName));
    }

    TEST_METHOD (SetFindDataReplacesTheData)
    {
        WIN32_FIND_DATAW             found = MakeFound();
        WIN32_FIND_DATAW             other = {};
        WIN32_FIND_DATAW             read  = {};
        ComPtr<IFileSystemBindData>  data;

        data.Attach (new FileSystemBindData (found));

        other.nFileSizeLow = 1;

        Assert::AreEqual (S_OK, data->SetFindData (&other));
        Assert::AreEqual (S_OK, data->GetFindData (&read));
        Assert::AreEqual ((DWORD) 1, read.nFileSizeLow);
        Assert::AreEqual (E_POINTER, data->GetFindData (nullptr));
    }

    TEST_METHOD (QueryInterfaceReturnsOnlyTheBindDataInterface)
    {
        ComPtr<IFileSystemBindData>  data;
        ComPtr<IUnknown>             unknown;
        ComPtr<IBindCtx>             other;

        data.Attach (new FileSystemBindData (MakeFound()));

        Assert::AreEqual (S_OK,          data.As (&unknown));
        Assert::AreEqual (E_NOINTERFACE, data.As (&other));
    }
};
