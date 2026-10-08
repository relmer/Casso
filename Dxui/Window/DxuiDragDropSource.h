#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDragDropSource
//
//  The drag half beside DxuiDragDropTarget: an IDropSource and the IDataObject
//  it offers, over a list of formats rendered on demand.
//
//  NOTHING IS RENDERED UNTIL A TARGET ASKS. A drag of twenty files out of a
//  disk image offers twenty file contents, and a drop on a folder pulls each
//  one by index; a drag cancelled halfway pulls none. Each rendered item is
//  kept, so a target that asks twice is not charged twice.
//
//  A format with a count above one is indexed by the request's `lindex`, as
//  FILECONTENTS is; a format with a count of one ignores it.
//
//  Begin blocks in DoDragDrop on the calling thread, as OLE requires, and
//  Escape cancels. The shell draws the drag image, and a target that sets a
//  drop description -- "Move to Disks" -- has it drawn under the image.
//
//  A TARGET OR THE SHELL MAY STORE FORMATS OF ITS OWN on the object: the drag
//  image, the drop description, the effect it performed. Each is kept, and
//  read back as if the source had offered it, since the drag image cannot be
//  drawn without them.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiDragDropSource : public IDropSource, public IDataObject
{
public:
    struct Format
    {
        CLIPFORMAT                                                           format = 0;
        int                                                                  count  = 1;
        std::function<HRESULT (int index, std::vector<uint8_t> & outBytes)>  render;
    };

    //  A source over `formats`, with one reference the caller releases.
    static HRESULT  Create (std::vector<Format> formats, DxuiDragDropSource ** outSource);

    //  Builds a source and runs the drag from `startScreen`. `resultEffect` is
    //  what the target accepted, DROPEFFECT_NONE on a cancel. A target that
    //  moved the items itself -- an optimized move -- returns none and says so
    //  in `outLogicalEffect`, which is then DROPEFFECT_MOVE.
    static HRESULT  Begin (std::vector<Format> formats, DWORD allowedEffects, POINT startScreen,
                           DWORD & resultEffect, DWORD * outLogicalEffect = nullptr);

    //  A DWORD a target stored under the registered format, or `fallback`.
    DWORD  GetStoredDword (const wchar_t * formatName, DWORD fallback) const;

    //  How many times each item was rendered, for a test to count.
    int  GetRenderCount (CLIPFORMAT format, int index) const;

    STDMETHODIMP          QueryInterface (REFIID riid, void ** ppv) override;
    STDMETHODIMP_(ULONG)  AddRef         () override;
    STDMETHODIMP_(ULONG)  Release        () override;

    //  IDropSource
    STDMETHODIMP  QueryContinueDrag (BOOL escapePressed, DWORD keyState) override;
    STDMETHODIMP  GiveFeedback      (DWORD effect) override;

    //  IDataObject
    STDMETHODIMP  GetData               (FORMATETC * format, STGMEDIUM * medium) override;
    STDMETHODIMP  GetDataHere           (FORMATETC * format, STGMEDIUM * medium) override;
    STDMETHODIMP  QueryGetData          (FORMATETC * format) override;
    STDMETHODIMP  GetCanonicalFormatEtc (FORMATETC * formatIn, FORMATETC * formatOut) override;
    STDMETHODIMP  SetData               (FORMATETC * format, STGMEDIUM * medium, BOOL release) override;
    STDMETHODIMP  EnumFormatEtc         (DWORD direction, IEnumFORMATETC ** outEnum) override;
    STDMETHODIMP  DAdvise               (FORMATETC * format, DWORD advf, IAdviseSink * sink, DWORD * connection) override;
    STDMETHODIMP  DUnadvise             (DWORD connection) override;
    STDMETHODIMP  EnumDAdvise           (IEnumSTATDATA ** outEnum) override;

private:
    explicit DxuiDragDropSource (std::vector<Format> formats);
    virtual ~DxuiDragDropSource() = default;

    const Format *  FindFormat (CLIPFORMAT format) const;

    //  The rendered bytes for one item, rendering them the first time.
    HRESULT  GetRendered (const Format & format, int index, const std::vector<uint8_t> *& outBytes);

    std::atomic<ULONG>                                          m_refCount = 1;
    std::vector<Format>                                         m_formats;
    std::map<std::pair<CLIPFORMAT, int>, std::vector<uint8_t>>  m_rendered;
    std::map<std::pair<CLIPFORMAT, int>, int>                   m_renderCounts;
    std::map<CLIPFORMAT, std::vector<uint8_t>>                  m_stored;
};
