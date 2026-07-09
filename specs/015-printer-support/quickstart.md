# Quickstart: Validating Printer Support

**Feature**: 015-printer-support

## Prerequisites

- Build via `Casso.sln` (not the vcxproj — solution-dir output rule) or the
  VS Code `Build + Test Debug` task; verify `Casso.exe` LastWriteTime is
  newer than the build before trusting any run.
- A Print Shop disk image (user-supplied) for end-to-end scenarios.

## Unit validation (no emulator, no system state)

```powershell
scripts\RunTests.ps1    # or VS Code task Build + Test Debug
```

Suites that must pass: `ImageWriterInterpreterTests` (per-command goldens,
determinism: same stream twice → identical raster hash),
`PaperRendererGoldenTests` (ink + plain styles, both dpi, composite colors,
weave determinism), `PrintRasterTests` (page boundaries, 60-page cap),
`PrinterCardTests` (ordering, ring, register contract, high-water backpressure
on ready — no byte loss when the drain stalls),
`FirmwareParityTests` (assemble `.a65`, compare embedded bytes),
`TitleRecognizerTests`, `PrintJobSerializerTests` (round-trip identity),
`PrinterPresenterTests` (paced replay / coalescing / fast-forward, injected
clock), `PrinterAudioSourceTests` (event-voice scheduling, synthetic PCM).

## End-to-end scenarios (map to spec user stories)

1. **US1 — Print Shop → PNG**: boot Print Shop; setup: printer "Apple DMP,
   ImageWriter, Scribe", interface "Apple II Parallel Interface", slot 1,
   four-color ribbon; design and print a sign. Expect: panel auto-reveals on
   first bytes, paper animates with audio, press Form Feed → PNG appears in
   the configured folder, monochrome sign, true geometry (circle test:
   width == height ±1%, SC-009).
2. **US2 — color**: print a color card with the four-color ribbon selected.
   Expect all seven colors; composites where colors overlap.
3. **US3 — destinations**: switch Settings → Printing to Windows printer;
   eject shows page count then the print dialog; "Microsoft Print to PDF"
   yields a PDF at true scale. Copy button pastes into Paint (legacy DIB) and
   into an editor that reads "PNG" format at full fidelity. Cancel the dialog
   → strip stays pending.
4. **US4 — widget**: card enabled → indicator present, drives untouched;
   `PR#1` from BASIC reveals panel; close panel mid-job → indicator blinks
   pending; hover shows config summary; discard prompts, then clears.
5. **US5 — banner**: print a multi-page Print Shop banner → single seamless
   PNG; same banner to Microsoft Print to PDF → paginated, no lost rows.
6. **US6 — text**: `PR#1` + `LIST` prints a readable listing; `CATALOG`
   under DOS 3.3 likewise.
7. **US7 — recognition**: mount `print shop.dsk` (or a WOZ with META title)
   → one dismissible notice with setup answers; unknown disk → nothing.
8. **FR-026 — persistence**: print, don't eject, quit Casso, relaunch, open
   the same machine → paper restored, indicator pending; eject delivers.
9. **US4 preview redesign — long banner (FR-032/033/034, SC-010/011)**: print a
   long (many-page) Print Shop banner. Expect: the preview auto-opens on the
   fresh print and shows the head sweeping left→right laying ink (partial lines
   visible, not whole lines popping in), the ~1-page viewport following the
   newest rows; scroll back with wheel / Up-Down arrows to review earlier pages,
   then pause ~2 s → view snaps back to the printing row. Throughout, Casso
   memory stays bounded and the preview does not slow as the strip grows.
   Deliver: Copy and Finish→PNG complete without a multi-GB spike (delivery dpi
   auto-caps for very tall strips). Windows-printer failures now trace to the
   debug log (driver, geometry, failing GDI call + GetLastError).

## Empirical gates from research

- R-001/R-003: enable the byte-capture sink, run Print Shop test prints per
  interface selection, archive captures under `UnitTest/Fixtures/Printer/`
  (our own generated data) and lock the status-byte value + command grammar.
- R-009: time-boxed `IPrintManagerInterop` spike from the unpackaged exe —
  record outcome in tasks.md before building final dialog flow.

## Cleanup reminders

Byte-capture files and test PNGs are diagnostic artifacts — delete before
commit (workspace hygiene rule; do NOT add ignore patterns).
