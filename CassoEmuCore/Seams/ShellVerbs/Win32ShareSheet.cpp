#include "Pch.h"

#include "Seams/ShellVerbs/Win32ShareSheet.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::FtmBase;
using Microsoft::WRL::Implements;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::Wrappers::Event;
using Microsoft::WRL::Wrappers::HStringReference;

namespace AbiTransfer    = ABI::Windows::ApplicationModel::DataTransfer;
namespace AbiStorage     = ABI::Windows::Storage;
namespace AbiFoundation  = ABI::Windows::Foundation;
namespace AbiCollections = ABI::Windows::Foundation::Collections;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShareSheet::~Win32ShareSheet
//
////////////////////////////////////////////////////////////////////////////////

Win32ShareSheet::~Win32ShareSheet()
{
    Unhook();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShareSheet::Show
//
//  The files are opened as storage items before the window opens, as its
//  DataRequested event needs them at once.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShareSheet::Show (HWND owner, const std::vector<std::wstring> & paths)
{
    HRESULT                              hr = S_OK;
    ComPtr<IDataTransferManagerInterop>  interop;
    StorageItems                         items;



    CBRAEx (owner != nullptr && !paths.empty(), E_INVALIDARG);

    for (const std::wstring & path : paths)
    {
        ComPtr<StorageItem>  item;

        hr = OpenItem (path, item);
        CHR (hr);

        items.push_back (std::move (item));
    }

    m_items = Make<ItemList> (std::move (items));
    CPR (m_items);

    m_title = GetTitle (paths);

    hr = RoGetActivationFactory (HStringReference (RuntimeClass_Windows_ApplicationModel_DataTransfer_DataTransferManager).Get(), IID_PPV_ARGS (&interop));
    CHR (hr);

    //  A window has one manager, which raises DataRequested each time the
    //  share window opens.
    if (m_manager == nullptr || m_owner != owner)
    {
        Unhook();

        hr = interop->GetForWindow (owner, IID_PPV_ARGS (&m_manager));
        CHR (hr);

        hr = m_manager->add_DataRequested (Callback<AbiFoundation::ITypedEventHandler<AbiTransfer::DataTransferManager *, AbiTransfer::DataRequestedEventArgs *>> (
                 [this] (AbiTransfer::IDataTransferManager *, AbiTransfer::IDataRequestedEventArgs * args) { return OnDataRequested (args); }).Get(),
             &m_token);
        CHR (hr);

        m_owner = owner;
    }

    hr = interop->ShowShareUIForWindow (owner);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShareSheet::GetTitle
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32ShareSheet::GetTitle (const std::vector<std::wstring> & paths)
{
    if (paths.size() == 1)
    {
        return std::filesystem::path (paths[0]).filename().wstring();
    }

    return std::format (L"{} items", paths.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShareSheet::OpenItem
//
//  A folder as a storage folder and anything else as a storage file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShareSheet::OpenItem (const std::wstring & path, ComPtr<StorageItem> & outItem)
{
    HRESULT                                                              hr         = S_OK;
    DWORD                                                                attributes = GetFileAttributesW (path.c_str());
    ComPtr<AbiStorage::IStorageFileStatics>                              files;
    ComPtr<AbiStorage::IStorageFolderStatics>                            folders;
    ComPtr<AbiFoundation::IAsyncOperation<AbiStorage::StorageFile *>>    fileOpen;
    ComPtr<AbiFoundation::IAsyncOperation<AbiStorage::StorageFolder *>>  folderOpen;
    ComPtr<AbiStorage::IStorageFile>                                     file;
    ComPtr<AbiStorage::IStorageFolder>                                   folder;



    CWR (attributes != INVALID_FILE_ATTRIBUTES);

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        hr = RoGetActivationFactory (HStringReference (RuntimeClass_Windows_Storage_StorageFolder).Get(), IID_PPV_ARGS (&folders));
        CHR (hr);

        hr = folders->GetFolderFromPathAsync (HStringReference (path.c_str()).Get(), &folderOpen);
        CHR (hr);

        hr = Await (folderOpen.Get(), folder.GetAddressOf());
        CHR (hr);

        hr = folder.As (&outItem);
        CHR (hr);
    }
    else
    {
        hr = RoGetActivationFactory (HStringReference (RuntimeClass_Windows_Storage_StorageFile).Get(), IID_PPV_ARGS (&files));
        CHR (hr);

        hr = files->GetFileFromPathAsync (HStringReference (path.c_str()).Get(), &fileOpen);
        CHR (hr);

        hr = Await (fileOpen.Get(), file.GetAddressOf());
        CHR (hr);

        hr = file.As (&outItem);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShareSheet::Await
//
//  The operation's result, waiting up to kOpenTimeoutMs while the thread's
//  messages are dispatched.
//
////////////////////////////////////////////////////////////////////////////////

template <typename Result, typename Abi>
HRESULT Win32ShareSheet::Await (AbiFoundation::IAsyncOperation<Result *> * operation, Abi ** outResult)
{
    HRESULT  hr     = S_OK;
    Event    done   (CreateEventExW (nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS));
    DWORD    signal = 0;
    HANDLE   handle = done.Get();



    CWR (done.IsValid());

    hr = operation->put_Completed (Callback<Implements<RuntimeClassFlags<ClassicCom>, AbiFoundation::IAsyncOperationCompletedHandler<Result *>, FtmBase>> (
             [handle] (AbiFoundation::IAsyncOperation<Result *> *, AbiFoundation::AsyncStatus) { SetEvent (handle); return S_OK; }).Get());
    CHR (hr);

    hr = CoWaitForMultipleHandles (COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES, kOpenTimeoutMs, 1, &handle, &signal);
    CHR (hr);

    hr = operation->GetResults (outResult);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShareSheet::OnDataRequested
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ShareSheet::OnDataRequested (AbiTransfer::IDataRequestedEventArgs * args)
{
    HRESULT                                       hr = S_OK;
    ComPtr<AbiTransfer::IDataRequest>             request;
    ComPtr<AbiTransfer::IDataPackage>             package;
    ComPtr<AbiTransfer::IDataPackagePropertySet>  properties;



    hr = args->get_Request (&request);
    CHR (hr);

    hr = request->get_Data (&package);
    CHR (hr);

    hr = package->get_Properties (&properties);
    CHR (hr);

    hr = properties->put_Title (HStringReference (m_title.c_str()).Get());
    CHR (hr);

    hr = package->SetStorageItemsReadOnly (m_items.Get());
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShareSheet::Unhook
//
////////////////////////////////////////////////////////////////////////////////

void Win32ShareSheet::Unhook()
{
    HRESULT  hr = S_OK;



    if (m_manager != nullptr)
    {
        hr = m_manager->remove_DataRequested (m_token);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    m_manager.Reset();
    m_owner = nullptr;
    m_token = {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShareSheet::ItemIterator::get_Current
//
////////////////////////////////////////////////////////////////////////////////

IFACEMETHODIMP Win32ShareSheet::ItemIterator::get_Current (StorageItem ** current)
{
    HRESULT  hr = S_OK;



    CBRAEx (current != nullptr, E_POINTER);
    CBREx  (m_at < m_items.size(), E_BOUNDS);

    hr = m_items[m_at].CopyTo (current);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShareSheet::ItemIterator::get_HasCurrent
//
////////////////////////////////////////////////////////////////////////////////

IFACEMETHODIMP Win32ShareSheet::ItemIterator::get_HasCurrent (boolean * hasCurrent)
{
    HRESULT  hr = S_OK;



    CBRAEx (hasCurrent != nullptr, E_POINTER);

    *hasCurrent = m_at < m_items.size();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShareSheet::ItemIterator::MoveNext
//
////////////////////////////////////////////////////////////////////////////////

IFACEMETHODIMP Win32ShareSheet::ItemIterator::MoveNext (boolean * hasCurrent)
{
    HRESULT  hr = S_OK;



    CBRAEx (hasCurrent != nullptr, E_POINTER);

    m_at        = (std::min) (m_at + 1, m_items.size());
    *hasCurrent = m_at < m_items.size();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShareSheet::ItemIterator::GetMany
//
////////////////////////////////////////////////////////////////////////////////

IFACEMETHODIMP Win32ShareSheet::ItemIterator::GetMany (unsigned capacity, StorageItem ** items, unsigned * actual)
{
    HRESULT   hr     = S_OK;
    unsigned  copied = 0;



    CBRAEx (actual != nullptr && (items != nullptr || capacity == 0), E_POINTER);

    for (; copied < capacity && m_at < m_items.size(); copied++, m_at++)
    {
        hr = m_items[m_at].CopyTo (&items[copied]);
        CHR (hr);
    }

Error:
    if (actual != nullptr)
    {
        *actual = copied;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShareSheet::ItemList::First
//
////////////////////////////////////////////////////////////////////////////////

IFACEMETHODIMP Win32ShareSheet::ItemList::First (AbiCollections::IIterator<StorageItem *> ** first)
{
    HRESULT               hr       = S_OK;
    ComPtr<ItemIterator>  iterator = Make<ItemIterator> (m_items);



    CBRAEx (first != nullptr, E_POINTER);
    CPR    (iterator);

    *first = iterator.Detach();

Error:
    return hr;
}
