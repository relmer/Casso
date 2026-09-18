#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSourceTests
//
//  The data object as a drop target consumes it, without a real drag: a fake
//  consumer enumerates the formats, pulls FILECONTENTS items by index, and
//  checks that each item is rendered once however often it is asked for and
//  that an item never asked for is never rendered. Also the drop source's
//  answers to Escape and a released button.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiDragDropSourceTests)
{
public:

    struct Fixture
    {
        CLIPFORMAT                           descriptor = 0;
        CLIPFORMAT                           contents   = 0;
        DxuiDragDropSource                 * source     = nullptr;
        std::shared_ptr<std::vector<int>>    renders    = std::make_shared<std::vector<int>>();

        ~Fixture()
        {
            if (source != nullptr)
            {
                source->Release();
            }
        }
    };


    static void  Build (Fixture & fixture)
    {
        std::vector<DxuiDragDropSource::Format>  formats;
        auto                                     renders = fixture.renders;
        HRESULT                                  hr      = S_OK;

        fixture.descriptor = (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILEDESCRIPTORW);
        fixture.contents   = (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_FILECONTENTS);

        formats.push_back ({ fixture.descriptor, 1, [] (int, std::vector<uint8_t> & out)
        {
            out.assign (sizeof (FILEGROUPDESCRIPTORW), 0);
            return S_OK;
        } });

        formats.push_back ({ fixture.contents, 3, [renders] (int index, std::vector<uint8_t> & out)
        {
            renders->push_back (index);
            out.assign ((size_t) (index + 1) * 10, (uint8_t) ('A' + index));
            return S_OK;
        } });

        hr = DxuiDragDropSource::Create (std::move (formats), &fixture.source);
        Assert::AreEqual (S_OK, hr);
    }


    static std::vector<uint8_t>  Pull (IDataObject * data, CLIPFORMAT format, LONG index, HRESULT & hr)
    {
        FORMATETC             request = { format, nullptr, DVASPECT_CONTENT, index, TYMED_HGLOBAL };
        STGMEDIUM             medium  = {};
        std::vector<uint8_t>  bytes;

        hr = data->GetData (&request, &medium);

        if (SUCCEEDED (hr))
        {
            size_t          size   = GlobalSize (medium.hGlobal);
            const uint8_t * locked = (const uint8_t *) GlobalLock (medium.hGlobal);

            bytes.assign (locked, locked + size);
            GlobalUnlock (medium.hGlobal);
            ReleaseStgMedium (&medium);
        }

        return bytes;
    }


    TEST_METHOD (EnumFormatEtc_ListsEveryFormat)
    {
        Fixture                          fixture;
        IEnumFORMATETC *                 enumerator = nullptr;
        FORMATETC                        one        = {};
        ULONG                            fetched    = 0;
        std::vector<CLIPFORMAT>          seen;

        Build (fixture);

        Assert::AreEqual (S_OK, fixture.source->EnumFormatEtc (DATADIR_GET, &enumerator));

        while (enumerator->Next (1, &one, &fetched) == S_OK)
        {
            seen.push_back (one.cfFormat);
            Assert::AreEqual ((DWORD) TYMED_HGLOBAL, one.tymed);
        }

        enumerator->Release();

        Assert::AreEqual ((size_t) 2, seen.size());
        Assert::AreEqual (fixture.descriptor, seen[0]);
        Assert::AreEqual (fixture.contents,   seen[1]);
    }


    TEST_METHOD (FileContents_RenderedOncePerIndexOnDemand)
    {
        Fixture               fixture;
        IDataObject *         data = nullptr;
        HRESULT               hr   = S_OK;
        std::vector<uint8_t>  bytes;

        Build (fixture);

        Assert::AreEqual (S_OK, fixture.source->QueryInterface (IID_IDataObject, (void **) &data));
        Assert::IsTrue (fixture.renders->empty());

        bytes = Pull (data, fixture.contents, 2, hr);
        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue (bytes.size() >= 30);
        Assert::AreEqual ((uint8_t) 'C', bytes[0]);

        bytes = Pull (data, fixture.contents, 2, hr);
        bytes = Pull (data, fixture.contents, 0, hr);
        Assert::AreEqual ((uint8_t) 'A', bytes[0]);

        Assert::AreEqual ((size_t) 2, fixture.renders->size());
        Assert::AreEqual (1, fixture.source->GetRenderCount (fixture.contents, 2));
        Assert::AreEqual (1, fixture.source->GetRenderCount (fixture.contents, 0));
        Assert::AreEqual (0, fixture.source->GetRenderCount (fixture.contents, 1));

        data->Release();
    }


    TEST_METHOD (GetData_RefusesUnknownFormatIndexAndMedium)
    {
        Fixture      fixture;
        HRESULT      hr      = S_OK;
        FORMATETC    request = { fixture.contents, nullptr, DVASPECT_CONTENT, 0, TYMED_ISTREAM };
        STGMEDIUM    medium  = {};

        Build (fixture);

        Pull (fixture.source, CF_UNICODETEXT, -1, hr);
        Assert::AreEqual (DV_E_FORMATETC, hr);

        Pull (fixture.source, fixture.contents, 3, hr);
        Assert::AreEqual (DV_E_LINDEX, hr);

        request.cfFormat = fixture.contents;
        Assert::AreEqual (DV_E_TYMED, fixture.source->GetData (&request, &medium));

        request = { fixture.descriptor, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        Assert::AreEqual (S_OK, fixture.source->QueryGetData (&request));
        Assert::IsTrue (fixture.renders->empty());
    }


    TEST_METHOD (QueryContinueDrag_EscapeCancelsReleaseDrops)
    {
        Fixture  fixture;

        Build (fixture);

        Assert::AreEqual (DRAGDROP_S_CANCEL, fixture.source->QueryContinueDrag (TRUE,  MK_LBUTTON));
        Assert::AreEqual (DRAGDROP_S_DROP,   fixture.source->QueryContinueDrag (FALSE, 0));
        Assert::AreEqual (S_OK,              fixture.source->QueryContinueDrag (FALSE, MK_LBUTTON));
        Assert::AreEqual (DRAGDROP_S_USEDEFAULTCURSORS, fixture.source->GiveFeedback (DROPEFFECT_COPY));
    }
};
