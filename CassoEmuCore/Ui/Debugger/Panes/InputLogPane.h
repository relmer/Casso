#pragma once

#include "Pch.h"

#include "Ui/Debugger/Panes/EventLogPane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InputLogPane
//
//  The input log (FR-214): the host's input from the input journal and the
//  guest's reads of the keyboard and game port, each read only when its
//  value differs from the last read of the same source, with the cycle,
//  source, address, value and meaning, and the host's clock as an optional
//  column.
//
//  The filters are those of the emulator's old Input window: the guest's
//  keyboard, joystick and paddle reads, and the host's keyboard, with the
//  host's mouse added. Each paddle pair is viewed as a joystick or as two
//  paddles, and its view sets both the labels of its axes and which of the
//  joystick and paddle filters lists it. The PTRIG strobe arms both pairs,
//  so either filter lists it, but only a strobe whose next paddle row is a
//  read is listed: a loop that strobes without reading lists nothing.
//
////////////////////////////////////////////////////////////////////////////////

class InputLogPane : public EventLogPane<InputEvent>
{
public:
    enum Column : size_t
    {
        kTime,
        kCycle,
        kSource,
        kAddress,
        kValue,
        kMeaning,
        kColumnCount,
    };

    enum Lane : size_t
    {
        kGuestKeyboard,
        kJoystick,
        kPaddle,
        kHostKeyboard,
        kHostMouse,
        kLaneCount,
    };

    //  Which part of the game port an event is of: neither pair, one of
    //  them, or both at once.
    enum class GamePort
    {
        None,
        Pair0,
        Pair1,
        Both,
    };

    InputLogPane (DxuiListView * list, RequestFn request) : EventLogPane (list, std::move (request)) {}

    void  SetLaneShown         (Lane lane, bool isShown);
    bool  IsLaneShown          (Lane lane) const { return m_lanes[lane]; }
    void  SetPairJoystick      (int pair, bool isJoystick);
    bool  IsPairJoystick       (int pair) const  { return m_isPairJoystick[(size_t) (pair & 1)]; }

    //  The //c wires its mouse button to $C063, pressed when bit 7 is clear.
    void  SetMouseButtonAtC063 (bool isMouseButton);

    //  The filters, pair views and columns as text for the preferences, and
    //  back.
    std::string   GetOptionsText () const;
    void          SetOptionsText (const std::string & text);

    static const wchar_t *  GetLaneLabel (Lane lane);
    static const wchar_t *  GetLaneTip   (Lane lane);
    static GamePort         GetGamePort  (const InputEvent & event);

    //  The address, value and meaning cells of an event.
    void  FormatEvent (const InputEvent & event, std::wstring & outAddress, std::wstring & outValue, std::wstring & outMeaning) const;

protected:
    ColumnSpecs  GetColumnSpecs () const override;
    void         PrepareView    (const std::vector<Row> & rows) override;
    bool         Matches        (const Row & row, size_t index) const override;
    void         MakeCells      (const Row & row, Cells & out) const override;
    size_t       GetLabelColumn () const override { return kSource; }
    size_t       GetTextColumn  () const override { return kMeaning; }

private:
    static constexpr Word     kOpenApple   = 0xC061;
    static constexpr Word     kClosedApple = 0xC062;
    static constexpr Word     kButton2     = 0xC063;
    static constexpr Word     kPaddle0     = 0xC064;
    static constexpr Word     kPaddle1     = 0xC065;
    static constexpr Word     kPaddle2     = 0xC066;
    static constexpr Word     kPaddle3     = 0xC067;
    static constexpr uint8_t  kHighBit     = 0x80;
    static constexpr int      kAxesPerPair = 2;
    static constexpr int      kAxisCount   = 4;

    static const wchar_t *  GetLineName   (uint16_t line);
    static bool             IsSwitchLine  (uint16_t line);

    //  For each row, whether it is a PTRIG strobe whose next paddle row is a
    //  read.
    std::vector<bool>             m_isTriggerRead;
    std::array<bool, kLaneCount>  m_lanes                 = { true, true, true, true, true };
    std::array<bool, 2>           m_isPairJoystick        = { true, true };
    bool                          m_isMouseButtonAtC063   = false;
};
