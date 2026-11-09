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


    //  The shell's drag image and a target's drop description are stored on
    //  the object and read back later in the drag; without that, no image.
    TEST_METHOD (SetData_KeepsWhatATargetStoresAndGivesItBack)
    {
        Fixture     fixture;
        CLIPFORMAT  logical = (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_LOGICALPERFORMEDDROPEFFECT);
        FORMATETC   format  = { logical, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM   medium  = {};
        STGMEDIUM   read    = {};
        HGLOBAL     global  = GlobalAlloc (GMEM_MOVEABLE, sizeof (DWORD));
        DWORD       move    = DROPEFFECT_MOVE;
        void      * locked  = nullptr;

        Build (fixture);

        Assert::IsNotNull (global);
        locked = GlobalLock (global);
        Assert::IsNotNull (locked);

        memcpy (locked, &move, sizeof (move));
        GlobalUnlock (global);
        medium.tymed   = TYMED_HGLOBAL;
        medium.hGlobal = global;

        Assert::AreEqual (DV_E_FORMATETC, fixture.source->QueryGetData (&format), L"Nothing is offered under it at first");
        Assert::AreEqual (S_OK, fixture.source->SetData (&format, &medium, TRUE));
        Assert::AreEqual (S_OK, fixture.source->QueryGetData (&format), L"and then it is");

        Assert::AreEqual (S_OK, fixture.source->GetData (&format, &read));
        Assert::AreEqual ((DWORD) DROPEFFECT_MOVE, *(const DWORD *) GlobalLock (read.hGlobal));
        GlobalUnlock (read.hGlobal);
        ReleaseStgMedium (&read);

        Assert::AreEqual ((DWORD) DROPEFFECT_MOVE, fixture.source->GetStoredDword (CFSTR_LOGICALPERFORMEDDROPEFFECT, DROPEFFECT_NONE));
        Assert::AreEqual ((DWORD) DROPEFFECT_COPY, fixture.source->GetStoredDword (CFSTR_PERFORMEDDROPEFFECT, DROPEFFECT_COPY),
                          L"A format never stored reads as the fallback");
    }


    TEST_METHOD (SetData_KeepsAStreamAndGivesItBackAsOne)
    {
        Fixture     fixture;
        CLIPFORMAT  context = (CLIPFORMAT) RegisterClipboardFormatW (L"DragContext");
        FORMATETC   format  = { context, nullptr, DVASPECT_CONTENT, -1, TYMED_ISTREAM };
        STGMEDIUM   medium  = {};
        STGMEDIUM   read    = {};
        uint8_t     bytes[] = { 1, 2, 3, 4, 5 };
        uint8_t     back[8] = {};
        ULONG       count   = 0;

        Build (fixture);

        medium.tymed = TYMED_ISTREAM;
        medium.pstm  = SHCreateMemStream (bytes, sizeof (bytes));
        Assert::IsNotNull (medium.pstm);

        Assert::AreEqual (S_OK, fixture.source->SetData (&format, &medium, TRUE), L"The drag helper sets some formats as streams");
        Assert::AreEqual (S_OK, fixture.source->QueryGetData (&format));
        Assert::AreEqual (S_OK, fixture.source->GetData (&format, &read));
        Assert::AreEqual ((DWORD) TYMED_ISTREAM, read.tymed, L"and reads them back as streams");

        Assert::AreEqual (S_OK, read.pstm->Read (back, sizeof (back), &count));
        Assert::AreEqual ((ULONG) sizeof (bytes), count);
        Assert::AreEqual (0, memcmp (bytes, back, sizeof (bytes)));
        ReleaseStgMedium (&read);
    }


    TEST_METHOD (SetDragImage_IsAcceptedByTheShellsDragHelper)
    {
        Fixture        fixture;
        DxuiIconImage  image;
        FORMATETC      bits = { (CLIPFORMAT) RegisterClipboardFormatW (L"DragImageBits"), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };

        Build (fixture);

        image.width  = 32;
        image.height = 32;
        image.bgraPremul.assign (32 * 32, 0xFF2060C0u);

        Assert::AreEqual (S_OK, DxuiDragDropSource::SetDragImage (static_cast<IDataObject *> (fixture.source), image),
                          L"The helper stores a stream as well as the image, and fails if the object refuses it");
        Assert::AreEqual (S_OK, fixture.source->QueryGetData (&bits));
    }


    //  Sets `size` bytes under a registered format, as a target or the shell's
    //  drag window does.
    static void  Store (DxuiDragDropSource * source, const wchar_t * name, const void * bytes, size_t size)
    {
        FORMATETC   format = { (CLIPFORMAT) RegisterClipboardFormatW (name), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM   medium = {};
        HGLOBAL     global = GlobalAlloc (GMEM_MOVEABLE, size);
        void      * locked = nullptr;

        Assert::IsNotNull (global);
        locked = GlobalLock (global);
        Assert::IsNotNull (locked);

        memcpy (locked, bytes, size);
        GlobalUnlock (global);
        medium.tymed   = TYMED_HGLOBAL;
        medium.hGlobal = global;

        Assert::AreEqual (S_OK, source->SetData (&format, &medium, TRUE));
    }


    static void  StoreDword (DxuiDragDropSource * source, const wchar_t * name, DWORD value)
    {
        Store (source, name, &value, sizeof (value));
    }


    static void  StoreDescription (DxuiDragDropSource * source, DROPIMAGETYPE type, const wchar_t * message, const wchar_t * insert)
    {
        DROPDESCRIPTION  description = {};

        description.type = type;
        wcscpy_s (description.szMessage, message);
        wcscpy_s (description.szInsert,  insert);

        Store (source, CFSTR_DROPDESCRIPTION, &description, sizeof (description));
    }


    static DROPDESCRIPTION  ReadDescription (DxuiDragDropSource * source)
    {
        DROPDESCRIPTION       description = {};
        HRESULT               hr          = S_OK;
        std::vector<uint8_t>  bytes       = Pull (source, (CLIPFORMAT) RegisterClipboardFormatW (CFSTR_DROPDESCRIPTION), -1, hr);

        Assert::AreEqual (S_OK, hr, L"A description is on the object");
        Assert::IsTrue (bytes.size() >= sizeof (description));

        memcpy (&description, bytes.data(), sizeof (description));

        return description;
    }


    //  The drag window's rereads asked of it so far, taken off its queue.
    static int  CountUpdates (HWND window)
    {
        MSG  message = {};
        int  count   = 0;

        //  DDWM_UPDATEWINDOW.
        while (PeekMessageW (&message, window, WM_USER + 3, WM_USER + 3, PM_REMOVE))
        {
            count++;
        }

        return count;
    }


    //  File Explorer words its description and leaves the type invalid, for
    //  the source to fill in; the drag window draws nothing until it is.
    TEST_METHOD (GiveFeedback_FillsInTheTypeATargetLeftInvalid)
    {
        Fixture          fixture;
        DROPDESCRIPTION  description = {};

        Build (fixture);

        StoreDword (fixture.source, L"IsShowingLayered", 1);
        StoreDescription (fixture.source, DROPIMAGE_INVALID, L"Copy to %1", L"out");

        Assert::AreEqual (S_OK, fixture.source->GiveFeedback (DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_SCROLL));

        description = ReadDescription (fixture.source);
        Assert::AreEqual ((int) DROPIMAGE_COPY, (int) description.type, L"An effect of copy or move is described as a copy");
        Assert::AreEqual (L"Copy to %1", description.szMessage, false, L"and the target's words are kept");
        Assert::AreEqual (L"out",        description.szInsert,  false);

        fixture.source->GiveFeedback (DROPEFFECT_MOVE);
        Assert::AreEqual ((int) DROPIMAGE_MOVE, (int) ReadDescription (fixture.source).type, L"A type the source set follows the effect");

        fixture.source->GiveFeedback (DROPEFFECT_NONE);
        Assert::AreEqual ((int) DROPIMAGE_INVALID, (int) ReadDescription (fixture.source).type, L"No effect draws no description, only the no-drop cursor");
    }


    TEST_METHOD (GiveFeedback_LeavesATypeTheTargetChose)
    {
        Fixture  fixture;

        Build (fixture);

        StoreDword (fixture.source, L"IsShowingLayered", 1);
        StoreDescription (fixture.source, DROPIMAGE_WARNING, L"Cannot copy here", L"");

        fixture.source->GiveFeedback (DROPEFFECT_COPY);

        Assert::AreEqual ((int) DROPIMAGE_WARNING, (int) ReadDescription (fixture.source).type);
    }


    TEST_METHOD (GiveFeedback_DescribesTheEffectForATargetThatSetsNoDescription)
    {
        Fixture          fixture;
        DROPDESCRIPTION  description = {};

        Build (fixture);

        fixture.source->GiveFeedback (DROPEFFECT_LINK);
        Assert::AreEqual ((DWORD) 0, fixture.source->GetStoredDword (CFSTR_DROPDESCRIPTION, 0),
                          L"Without the shell's drag window, OLE's cursors show the effect and no description is made");

        StoreDword (fixture.source, L"IsShowingLayered", 1);
        fixture.source->GiveFeedback (DROPEFFECT_LINK);

        description = ReadDescription (fixture.source);
        Assert::AreEqual ((int) DROPIMAGE_LINK, (int) description.type, L"so the drag window draws the effect's icon");
        Assert::AreEqual (L"", description.szMessage, false);
    }


    //  The drag window rereads what is on the object when it gets to the
    //  message, so a reread asked for only on a change can land while File
    //  Explorer has its words cleared for a moment, and a bare "+" stays
    //  drawn. Every feedback asks again, so the window catches up.
    TEST_METHOD (GiveFeedback_TellsTheDragWindowOnEveryFeedback)
    {
        Fixture  fixture;
        HWND     window = CreateWindowExW (0, L"STATIC", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);

        Assert::IsNotNull (window);
        Build (fixture);

        StoreDword (fixture.source, L"DragWindow", (DWORD) (ULONG_PTR) window);

        fixture.source->GiveFeedback (DROPEFFECT_COPY);
        Assert::AreEqual (0, CountUpdates (window), L"Without the drag window up, OLE's cursors show the effect");

        StoreDword (fixture.source, L"IsShowingLayered", 1);
        StoreDescription (fixture.source, DROPIMAGE_COPY, L"Copy to %1", L"out");

        fixture.source->GiveFeedback (DROPEFFECT_COPY);
        fixture.source->GiveFeedback (DROPEFFECT_COPY);
        fixture.source->GiveFeedback (DROPEFFECT_COPY);
        Assert::AreEqual (3, CountUpdates (window), L"The same description is asked to be reread each time");

        DestroyWindow (window);
    }
};
