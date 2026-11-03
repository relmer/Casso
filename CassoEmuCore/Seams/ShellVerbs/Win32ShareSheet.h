#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ShareSheet
//
//  Windows' share window for files, through the data transfer manager that
//  Windows documents for desktop apps. The window's manager raises its
//  DataRequested event when the share window opens, and the handler puts the
//  files from the last Show in the data package.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ShareSheet
{
public:
    Win32ShareSheet  () = default;
    ~Win32ShareSheet ();

    Win32ShareSheet (const Win32ShareSheet &)             = delete;
    Win32ShareSheet & operator= (const Win32ShareSheet &) = delete;

    //  Opens the share window over owner with the files and folders at paths.
    HRESULT  Show (HWND owner, const std::vector<std::wstring> & paths);

    //  What the share window titles the data: the one item's name, or a count.
    static std::wstring  GetTitle (const std::vector<std::wstring> & paths);

    static constexpr DWORD  kOpenTimeoutMs = 10000;

private:
    using StorageItem  = ABI::Windows::Storage::IStorageItem;
    using StorageItems = std::vector<Microsoft::WRL::ComPtr<StorageItem>>;

    class ItemIterator : public Microsoft::WRL::RuntimeClass<ABI::Windows::Foundation::Collections::IIterator<StorageItem *>>
    {
        InspectableClass (L"Casso.ShareItemIterator", BaseTrust)

    public:
        explicit ItemIterator (StorageItems items) : m_items (std::move (items)) {}

        IFACEMETHODIMP  get_Current    (StorageItem ** current) override;
        IFACEMETHODIMP  get_HasCurrent (boolean * hasCurrent) override;
        IFACEMETHODIMP  MoveNext       (boolean * hasCurrent) override;
        IFACEMETHODIMP  GetMany        (unsigned capacity, StorageItem ** items, unsigned * actual) override;

    private:
        StorageItems  m_items;
        size_t        m_at = 0;
    };

    class ItemList : public Microsoft::WRL::RuntimeClass<ABI::Windows::Foundation::Collections::IIterable<StorageItem *>>
    {
        InspectableClass (L"Casso.ShareItemList", BaseTrust)

    public:
        explicit ItemList (StorageItems items) : m_items (std::move (items)) {}

        IFACEMETHODIMP  First (ABI::Windows::Foundation::Collections::IIterator<StorageItem *> ** first) override;

    private:
        StorageItems  m_items;
    };

    static HRESULT  OpenItem (const std::wstring & path, Microsoft::WRL::ComPtr<StorageItem> & outItem);

    template <typename Result, typename Abi>
    static HRESULT  Await    (ABI::Windows::Foundation::IAsyncOperation<Result *> * operation, Abi ** outResult);

    HRESULT  OnDataRequested (ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs * args);
    void     Unhook          ();

    Microsoft::WRL::ComPtr<ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager>  m_manager;
    Microsoft::WRL::ComPtr<ItemList>                                                           m_items;
    std::wstring                                                                               m_title;
    HWND                                                                                       m_owner   = nullptr;
    EventRegistrationToken                                                                     m_token   = {};
};
