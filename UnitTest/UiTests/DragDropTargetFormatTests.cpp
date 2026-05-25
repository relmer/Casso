#include "Pch.h"

#include "CppUnitTest.h"

#include "Ui/DragDropTarget.h"

#include <atomic>
#include <cstring>
#include <string>
#include <vector>


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DragDropTargetFormatTests
//
//  Verifies the testable subset of T141: that DragDropTarget::
//  ExtractFirstHDropPath round-trips each of the four supported disk
//  image extensions (.dsk, .nib, .woz, .po) through a CF_HDROP /
//  HGLOBAL pipe identical to what Explorer hands us.
//
//  Format acceptance / rejection by the mount path itself lives in
//  the mount adapter and is out of scope here -- ExtractFirstHDropPath
//  is extension-agnostic, but we still want regression coverage that
//  each extension survives the HDROP encode/decode dance.
//
//  The 200-attempt reliability harness called out in tasks.md T141 is
//  manual (requires a real Explorer window) and is not implemented in
//  unit tests.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    // Minimal IDataObject that serves one CF_HDROP global containing a
    // single wide path. Everything else returns DV_E_FORMATETC, which is
    // exactly how the real Explorer object behaves for unknown formats.
    class MockHDropDataObject : public IDataObject
    {
    public:
        explicit MockHDropDataObject (const std::wstring & path)
            : m_path (path)
        {
        }

        // -------- IUnknown --------
        STDMETHODIMP QueryInterface (REFIID riid, void ** ppv) override
        {
            if (ppv == nullptr) { return E_POINTER; }
            if (riid == IID_IUnknown || riid == IID_IDataObject)
            {
                *ppv = static_cast<IDataObject *> (this);
                AddRef();
                return S_OK;
            }
            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        STDMETHODIMP_(ULONG) AddRef () override
        {
            return m_ref.fetch_add (1, std::memory_order_acq_rel) + 1;
        }

        STDMETHODIMP_(ULONG) Release () override
        {
            return m_ref.fetch_sub (1, std::memory_order_acq_rel) - 1;
        }

        // -------- IDataObject --------
        STDMETHODIMP GetData (FORMATETC * pFormat, STGMEDIUM * pMedium) override
        {
            size_t      cbStruct = 0;
            size_t      cbPath   = 0;
            size_t      cbTotal  = 0;
            HGLOBAL     hMem     = nullptr;
            DROPFILES * pDrop    = nullptr;
            wchar_t   * pDst     = nullptr;

            if (pFormat == nullptr || pMedium == nullptr) { return E_POINTER; }
            if (pFormat->cfFormat != CF_HDROP)            { return DV_E_FORMATETC; }
            if ((pFormat->tymed & TYMED_HGLOBAL) == 0)    { return DV_E_TYMED;     }

            cbStruct = sizeof (DROPFILES);
            cbPath   = (m_path.size() + 2) * sizeof (wchar_t);   // +1 NUL +1 list-terminator
            cbTotal  = cbStruct + cbPath;

            hMem = GlobalAlloc (GHND, cbTotal);
            if (hMem == nullptr) { return E_OUTOFMEMORY; }

            pDrop = static_cast<DROPFILES *> (GlobalLock (hMem));
            if (pDrop == nullptr)
            {
                GlobalFree (hMem);
                return E_FAIL;
            }

            pDrop->pFiles = static_cast<DWORD> (cbStruct);
            pDrop->fWide  = TRUE;

            pDst = reinterpret_cast<wchar_t *> (reinterpret_cast<BYTE *> (pDrop) + cbStruct);
            std::memcpy (pDst, m_path.c_str(), m_path.size() * sizeof (wchar_t));
            pDst[m_path.size()]     = L'\0';
            pDst[m_path.size() + 1] = L'\0';

            GlobalUnlock (hMem);

            pMedium->tymed          = TYMED_HGLOBAL;
            pMedium->hGlobal        = hMem;
            pMedium->pUnkForRelease = nullptr;
            return S_OK;
        }

        // The remaining methods are required by the vtable but the
        // production code path never calls them -- stub to E_NOTIMPL.
        STDMETHODIMP GetDataHere       (FORMATETC *, STGMEDIUM *)               override { return E_NOTIMPL; }
        STDMETHODIMP QueryGetData      (FORMATETC *)                            override { return E_NOTIMPL; }
        STDMETHODIMP GetCanonicalFormatEtc (FORMATETC *, FORMATETC *)           override { return E_NOTIMPL; }
        STDMETHODIMP SetData           (FORMATETC *, STGMEDIUM *, BOOL)         override { return E_NOTIMPL; }
        STDMETHODIMP EnumFormatEtc     (DWORD, IEnumFORMATETC **)               override { return E_NOTIMPL; }
        STDMETHODIMP DAdvise           (FORMATETC *, DWORD, IAdviseSink *, DWORD *) override { return E_NOTIMPL; }
        STDMETHODIMP DUnadvise         (DWORD)                                  override { return E_NOTIMPL; }
        STDMETHODIMP EnumDAdvise       (IEnumSTATDATA **)                       override { return E_NOTIMPL; }

    private:
        std::atomic<ULONG>  m_ref  { 1 };
        std::wstring        m_path;
    };


    void ExpectRoundTrip (const std::wstring & input)
    {
        MockHDropDataObject  obj (input);
        std::wstring         got;
        HRESULT              hr = DragDropTarget::ExtractFirstHDropPath (&obj, got);

        Assert::AreEqual (S_OK, hr, L"ExtractFirstHDropPath should return S_OK");
        Assert::AreEqual (input.c_str(), got.c_str(),
            L"Path round-trip must preserve the original wide string verbatim");
    }
}


namespace UiTests
{

TEST_CLASS (DragDropTargetFormatTests)
{
public:

    TEST_METHOD (ExtractFirstHDropPath_RoundTrips_Dsk)
    {
        ExpectRoundTrip (L"C:\\Disks\\Apple\\MASTER.DSK");
    }

    TEST_METHOD (ExtractFirstHDropPath_RoundTrips_Nib)
    {
        ExpectRoundTrip (L"C:\\Disks\\Apple\\copy-protected.nib");
    }

    TEST_METHOD (ExtractFirstHDropPath_RoundTrips_Woz)
    {
        ExpectRoundTrip (L"C:\\Disks\\Apple\\Prince of Persia.woz");
    }

    TEST_METHOD (ExtractFirstHDropPath_RoundTrips_Po)
    {
        ExpectRoundTrip (L"C:\\Disks\\Apple\\prodos.po");
    }

    TEST_METHOD (ExtractFirstHDropPath_RoundTrips_UnicodePath)
    {
        // Wide-string fidelity matters; force a non-ASCII codepoint
        // through the pipe to prove fWide=TRUE is being honoured.
        ExpectRoundTrip (L"C:\\Disks\\\u00C5pple\\caf\u00E9.dsk");
    }

    TEST_METHOD (ExtractFirstHDropPath_NullDataObject_ReturnsFalse)
    {
        std::wstring  got = L"sentinel";
        HRESULT       hr  = DragDropTarget::ExtractFirstHDropPath (nullptr, got);

        Assert::AreEqual (S_FALSE, hr);
        Assert::IsTrue   (got.empty(), L"outPath must be cleared on failure");
    }
};

}   // namespace UiTests
