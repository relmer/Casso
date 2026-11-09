#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponent.h"

class FileMapGridView;
class IInspectorComponentHost;
class InspectorSession;
class InspectorTableView;





////////////////////////////////////////////////////////////////////////////////
//
//  FilesView
//
//  The file map of the volume: the step buttons, the file picker and its
//  list, the volume tabs, "Copy map", and the grid of the volume's sectors
//  (FR-088 to FR-095, FR-157).
//
////////////////////////////////////////////////////////////////////////////////

class FilesView : public IInspectorComponent
{
public:
    FilesView (InspectorSession & session, IInspectorComponentHost & host);

    void          Create     () override;
    void          PaintOver  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    std::wstring  GetTip     (POINT pointPx, RECT & outAnchorPx) const override;
    void          GetPrefs   (DiskInspectorPrefs & inOut) const override;
    void          ApplyPrefs (const DiskInspectorPrefs & prefs) override;
    void          OnDiskStarted (const DiskChange & change) override;

    void  Layout          (const RECT & area, const DxuiDpiScaler & scaler);
    void  Show            (bool isShown);
    void  RefreshFileList ();
    void  SyncSelection   ();
    void  ChooseFile      (int file);
    void  ResetZoom       ();
    bool  IsOverGrid      (POINT pointPx) const;
    void  ZoomGrid        (int steps, POINT pointPx);
    bool  IsListAt        (POINT pointPx) const;
    void  CopyFileMap     ();
    bool  HasMap          () const;
    bool  GetGridTip      (POINT pointPx, std::wstring & outTip, RECT & outAnchorPx, vector<DxuiTooltipSwatch> & outSwatches) const;

    InspectorTableView & GetFileList () const { return *m_fileList; }

private:
    void  OpenFilePicker     (bool isOpen);
    void  PaintPicker        (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void  SetPickerLabel     (const std::wstring & label);
    int   GetPickerWidthPx   (const DxuiDpiScaler & scaler) const;
    void  SelectMapCell      (int cell);
    void  StepFileSector     (int step);
    int   GetSelectedMapCell () const;

    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    DxuiScrollPanel          * m_mapScroll          = nullptr;
    FileMapGridView          * m_mapGrid            = nullptr;
    InspectorTableView       * m_fileList           = nullptr;
    DxuiTabStrip             * m_mapVolumes         = nullptr;
    DxuiCheckbox             * m_mapDeleted         = nullptr;
    DxuiCheckbox             * m_mapBadOnly         = nullptr;
    DxuiButton               * m_mapPrev            = nullptr;
    DxuiButton               * m_mapNext            = nullptr;
    DxuiButton               * m_mapCopy            = nullptr;
    DxuiLabel                * m_mapStepLabel       = nullptr;
    DxuiButton               * m_filePicker         = nullptr;
    bool                       m_isPickerOpen       = true;
    std::wstring               m_pickerLabel        = L"Choose a file";
    float                      m_pickerTextPx       = 0.0f;
    int                        m_revealedCell       = -1;
    int                        m_fileSortColumn     = -1;
    bool                       m_fileSortDescending = false;
};
