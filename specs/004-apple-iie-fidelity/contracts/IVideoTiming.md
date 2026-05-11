# Contract: IVideoTiming

Cycle-accurate-enough video timing model. Source of `IsInVblank()` for
$C019 (RDVBLBAR).

## Header

```cpp
class IVideoTiming {
public:
    virtual          ~IVideoTiming () = default;
    virtual void      Tick (uint32_t cpuCycles) = 0;
    virtual bool      IsInVblank () const = 0;
    virtual uint32_t  GetCycleInFrame () const = 0;
    virtual void      Reset () = 0;
};
```

## NTSC parameters

- 65 CPU cycles per scanline.
- 262 scanlines per frame.
- 17,030 CPU cycles per frame.
- VBL active for scanlines 192..261 (i.e. cycle-in-frame ≥ 192*65 = 12,480).

## Test obligations

`VideoTimingTests`:
- After 12,480 cycles from frame start: `IsInVblank()` becomes true.
- After 17,030 cycles: counter wraps to 0; `IsInVblank()` returns false.
- A spin-loop reading $C019 (via the soft-switch bank) sees bit-7 transitions.
