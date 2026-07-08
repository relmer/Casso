# Feature Specification: Emulated Printer Support (ImageWriter II)

**Feature Branch**: `015-printer-support`

**Created**: 2026-07-07

**Status**: Draft

**Input**: User description: "Emulated printer support for the Apple //e emulator, with The Print Shop (and New Print Shop color printing) as the flagship scenario. Emulate an Apple ImageWriter II printer (C. Itoh 8510 command set with Apple extensions), including color ribbon support (ESC K color selection: black, yellow, red, blue, orange, green, purple), bit-image graphics (ESC G et al.), pitch-based horizontal densities (72-160 dpi), and 72/144 dpi vertical via half-line feeds. The printer interpreter is a pure byte-stream-in, page-raster-out component (unit-testable with synthetic byte streams, no hardware or system dependencies). The guest sends bytes via an emulated slot interface card: start with a generic parallel interface card (data latch + status register in the slot I/O window, plus a minimal custom slot firmware ROM we assemble with the in-repo assembler); emulate a Super Serial Card (6551 ACIA) only if Print Shop refuses to drive an ImageWriter II through a parallel card. Rasterized output goes to a user-selectable destination configured in settings: (1) clipboard as bitmap, (2) PNG file, (3) Windows printer via the standard Windows print dialog initially (a custom dxui print dialog is a later follow-on, out of scope). PDF output is achieved via the Windows \"Microsoft Print to PDF\" printer rather than a dedicated PDF writer. For clipboard/PNG destinations, emulate continuous fanfold paper so multi-page Print Shop banners render as one continuous image; pagination applies only to the Windows printer destination."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Print a Print Shop Page to a PNG File (Priority: P1)

An emulator user boots an unmodified The Print Shop disk on the emulated Apple //e, walks through Print Shop's printer setup (ImageWriter II on an interface card in slot 1), designs a greeting card or sign, and chooses Print. The emulated printer receives the guest's print data, renders it as it would have appeared on paper, and — with the PNG destination selected — writes an image file of the finished page to the user's configured output folder.

**Why this priority**: This is the flagship scenario the feature exists for. It exercises the entire pipeline — guest software, interface card, firmware, printer command interpretation, page rasterization, and file output — and delivers immediately demonstrable value on its own.

**Independent Test**: Two layers. (1) Feed synthetic and captured ImageWriter II byte streams (text, line feeds, bit-image graphics at multiple densities) directly to the printer component and compare the resulting raster against golden reference images. (2) End to end: boot the actual Print Shop disk, print a sign, inspect the PNG.

**Acceptance Scenarios**:

1. **Given** a machine configured with the printer interface card in slot 1 and PNG selected as the destination, **When** Print Shop prints a single-page sign, **Then** a PNG file of the complete page is created in the configured folder and its content is a faithful monochrome rendering of the designed sign.
2. **Given** the same setup, **When** the guest prints content mixing text and bit-image graphics at several dot densities on one page, **Then** every element lands at its documented paper position — no horizontal stretching, vertical gaps, or overlapping rows.
3. **Given** a machine with no printer card configured, **When** the guest reads or writes slot 1's I/O locations, **Then** the emulator behaves exactly as it does today for an empty slot and no output of any kind is produced.

---

### User Story 2 - Color Printing with a Four-Color Ribbon (Priority: P2)

The user selects the four-color ribbon in Print Shop's setup (offered by later revisions of the original Print Shop as well as The New Print Shop) and prints a greeting card with colored graphics. The output shows correct colors: the four ribbon primaries (black, yellow, red, blue), the three composites produced by overprinting (orange, green, purple), and paper white.

**Why this priority**: Color is the ImageWriter II's signature capability and the reason it was chosen as the printer to emulate. It builds directly on User Story 1's pipeline.

**Independent Test**: Feed a byte stream that selects each of the seven colors and prints a labeled band of graphics in each; verify each band renders in the correct color in the output raster.

**Acceptance Scenarios**:

1. **Given** the printer emulation with its color ribbon (always present), **When** the guest selects each of the seven colors via the color-selection command and prints graphics in each, **Then** each region renders in the correct color.
2. **Given** graphics printed in one primary color, **When** the guest overprints the same dot positions in a second primary (as the real printer does for orange, green, purple), **Then** those dots render as the correct composite color.
3. **Given** a guest that never issues a color-selection command, **When** it prints, **Then** all output is black on white.

---

### User Story 3 - Choosing the Output Destination (Priority: P3)

In the emulator's settings, the user chooses where finished pages go: the Windows clipboard (as a bitmap), a PNG file in a configurable folder, or a Windows printer via the standard Windows print dialog. Selecting "Microsoft Print to PDF" in that dialog yields PDF output with no dedicated PDF support in the emulator.

**Why this priority**: Destinations multiply the value of the pipeline but any single destination (PNG, from User Story 1) already proves it. Clipboard and Windows printing are additive conveniences.

**Independent Test**: With each destination selected in turn, complete a one-page print and verify the page arrives at that destination.

**Acceptance Scenarios**:

1. **Given** the clipboard destination, **When** a page completes, **Then** a bitmap of the page is on the Windows clipboard and pastes correctly into a standard image editor.
2. **Given** the Windows printer destination, **When** a print job completes, **Then** the standard Windows print dialog appears, and confirming it prints the job's pages; choosing "Microsoft Print to PDF" produces a PDF.
3. **Given** the Windows printer destination, **When** the user cancels the print dialog, **Then** no output is produced, the job remains available to re-emit, and emulation continues unaffected.
4. **Given** any destination selection, **When** the emulator is closed and relaunched, **Then** the selection persists.

---

### User Story 4 - The Printer on the Desk (Priority: P4)

When the printer card is enabled, a small printer indicator sits in the window chrome, clear of the centered disk drives. The full skeuomorphic printer — an ImageWriter-class body with its four-color ribbon cartridge visible and paper rising from the platen showing the actual rendered output — lives in a docked side panel shown on demand: it reveals itself automatically the moment the guest engages the printer (firmware activation such as `PR#1`, or the first data byte written to the card), and can be opened or closed any time via the indicator. Pressing the panel's Form Feed button performs the eject/finish-job action; closing the panel with output pending leaves the indicator signaling until the job is delivered. Hovering either surface summarizes the virtual configuration.

**Why this priority**: The indicator and panel are the discoverability and control surface for everyday printing — they answer "what printer do I tell Print Shop I have?" without documentation and host the eject affordance the pipeline needs. It outranks banners because it is used on every print.

**Independent Test**: Enable the printer card, observe indicator and panel through a complete engage-print-eject cycle, including closing the panel mid-job; disable the card and confirm both are absent.

**Acceptance Scenarios**:

1. **Given** a machine configured with the printer card enabled, **When** the emulator window opens, **Then** the printer indicator is visible without disturbing the disk-drive widgets' centered composition, and no panel is shown; **Given** the card is disabled, **Then** neither indicator nor panel appears.
2. **Given** the guest activates the card firmware (`PR#1`) or writes its first data byte, **When** that first engagement occurs, **Then** the docked panel reveals automatically and its paper shows the rendered content emerging as printing proceeds.
3. **Given** un-ejected output is pending and the panel is closed, **Then** the indicator continues to signal the pending state; **When** the user presses the panel's Form Feed button (or invokes the equivalent menu command), **Then** the job is finalized to the selected destination and the paper clears.
4. **Given** the pointer hovers over the indicator or the panel, **Then** a summary of the virtual configuration is shown (printer model, ribbon type, interface type, slot number).

---

### User Story 5 - Banner Printing on Continuous Fanfold Paper (Priority: P5)

The user prints a Print Shop banner — a message spanning many fanfold pages sideways. With the clipboard or PNG destination, the banner emerges as one continuous image with no page-break seams, something no physical printer could produce. With the Windows printer destination, the banner is split at page boundaries and printed as a sequence of pages.

**Why this priority**: Banners are a beloved Print Shop feature and the continuous-image rendering is a unique payoff of emulation, but it depends on the full pipeline plus destination handling already being in place.

**Independent Test**: Feed a byte stream spanning several page lengths without interruption; verify the PNG output is a single image of the full length and the paginated output tiles the same content across pages.

**Acceptance Scenarios**:

1. **Given** the PNG destination, **When** the guest prints a banner spanning multiple fanfold pages, **Then** a single PNG containing the entire banner is produced, with no seams, gaps, or duplicated rows at page boundaries.
2. **Given** the Windows printer destination, **When** the same banner is printed, **Then** the output is split into pages that tile the banner in order, together containing every row exactly once.

---

### User Story 6 - Text Printing from BASIC and DOS (Priority: P6)

At the BASIC prompt, the user types `PR#1` and then `LIST` (or `CATALOG` under DOS 3.3). The program listing prints as readable text in the printer's built-in font at the current character pitch, and the user ejects the page to their selected destination.

**Why this priority**: This is the classic verification that the interface card and its firmware behave like the real thing for the broad universe of 8-bit software, beyond Print Shop's direct-hardware drivers. It requires text-font rendering, which the graphics-driven flagship scenarios do not.

**Independent Test**: Feed plain ASCII text with line endings to the printer component and verify readable draft-font output; end to end, run `PR#1` + `LIST` in the emulator.

**Acceptance Scenarios**:

1. **Given** the machine at the BASIC prompt with the printer card in slot 1, **When** the user enters `PR#1` and `LIST`, **Then** the listing renders as readable text lines with correct line spacing and wrapping at the right margin.
2. **Given** DOS 3.3 booted, **When** the user enters `PR#1` and `CATALOG`, **Then** the disk catalog prints as readable text.

---

### User Story 7 - Recognizing Printing Software (Priority: P7)

The user mounts a disk whose image filename (e.g. contains "print shop"), embedded container metadata (e.g. a WOZ META title), or on-disk catalog (volume/file names, when the disk uses a standard filesystem) matches a curated list of printing-centric software. The emulator shows a brief, dismissible notice: the printer is ready, and here is what to answer in the software's setup menus. Recognition informs; it never configures.

**Why this priority**: Print Shop asks its setup questions before a single print byte flows, so the engagement-triggered panel reveal (User Story 4) arrives too late for that one moment. A mount-time notice delivers the answers exactly when they are needed — but it is garnish on a feature that works fully without it.

**Independent Test**: Recognition functions are testable with synthetic filenames and catalog data; end to end, mount known and unknown images and observe notice behavior.

**Acceptance Scenarios**:

1. **Given** a disk image whose filename contains a known signature, **When** it is mounted in Drive 1, **Then** a single auto-fading, dismissible notice appears with the setup answers.
2. **Given** a generically named image containing a standard DOS 3.3/ProDOS filesystem with known catalog signatures, **When** it is mounted, **Then** the same notice appears.
3. **Given** an unrecognized disk or one with an unreadable/non-standard filesystem, **When** it is mounted, **Then** no notice appears and nothing else changes — printing still reveals the panel via User Story 4.
4. **Given** any recognition outcome, **Then** machine configuration (card enablement, slot, output destination) is unchanged.

---

### Edge Cases

- **Job without a final form feed**: Print Shop and BASIC listings often stop mid-page without ejecting. The user must have an explicit "eject page / finish print job" action, and the emulator must indicate when printed-but-unejected data is pending.
- **Guest reset or reboot mid-print**: like a physical printer, pending page content survives a guest reset; it is finalized or discarded only by user action or a completed job.
- **Unknown or unsupported command sequences**: consumed without effect and without disturbing subsequent interpretation; never crashes or corrupts the page.
- **Output failure** (PNG folder missing or read-only, disk full, clipboard unavailable): the user is notified, the rendered page is not lost, and the user can retry after fixing the problem or switching destinations.
- **Extremely long banner**: continuous output has a documented practical maximum length; reaching it finalizes the strip and notifies the user rather than failing silently.
- **Full-speed byte bursts**: the guest may send bytes as fast as the CPU can write them; the card's readiness handshake must guarantee no byte is ever dropped or reordered.
- **Print dialog cancelled**: the job is retained as pending, not destroyed (Acceptance Scenario 3.3).
- **Panel closed or unavailable while output is pending**: the indicator always carries the pending/activity state, and the eject action remains available through a menu command. The indicator never disturbs the disk-drive widgets' centered composition, at any window size.

## Requirements *(mandatory)*

### Functional Requirements

**Interface card**

- **FR-001**: The emulator MUST offer a printer interface card that can be enabled in a configurable peripheral slot (default: slot 1) via the machine configuration; when disabled, slot behavior is unchanged from today.
- **FR-002**: The card MUST accept guest-written data bytes and expose a readiness indication such that a guest honoring the documented handshake never loses or reorders a byte, regardless of how fast it sends.
- **FR-003**: The card MUST provide slot firmware so that standard output redirection (`PR#<slot>`) and the firmware entry points used by common Apple II software function correctly. The firmware MUST be original work assembled from source maintained in this repository — no copyrighted third-party ROM images.
- **FR-004**: All bytes accepted by the card MUST be delivered to the emulated printer in the order written.

**Printer emulation**

- **FR-005**: The printer MUST interpret the subset of the ImageWriter II command set exercised by The Print Shop, The New Print Shop, and plain text printing: printable ASCII text; carriage return, line feed, and form feed; character-pitch selection across the printer's documented range of horizontal densities; line-spacing control including half-height line feeds (144 rows per inch); bit-image graphics commands; the seven-color selection command; and printer reset.
- **FR-006**: The printer MUST maintain a page raster whose cell grid matches the printer's maximum documented dot placement (160 dots per inch horizontally, 144 rows per inch vertically) so that every addressable dot position maps to exactly one raster cell.
- **FR-007**: The color model MUST reproduce paper white, the four ribbon primaries (black, yellow, red, blue), and the three overprint composites (orange, green, purple). Dots struck in two primaries at the same position render as the corresponding composite.
- **FR-008**: The paper model MUST be continuous-form stock 8.5 inches wide with an 8-inch printable width and an 11-inch page height used for pagination; these defaults MUST be documented.
- **FR-009**: Command interpretation MUST be deterministic: the same byte stream always produces an identical raster. Unrecognized commands are consumed without effect (FR-005's subset defines support; everything else is tolerated).
- **FR-010**: Printer state (pitch, line spacing, selected color, print position) MUST reset to power-on defaults on the printer-reset command and at emulator machine start.

**Output destinations**

- **FR-011**: The user MUST be able to select the output destination — clipboard, PNG file, or Windows printer — in the emulator's settings, and the selection MUST persist across emulator sessions.
- **FR-012**: The PNG destination MUST write automatically named files (collision-free, e.g. timestamped) to a user-configurable folder. On write failure the user is notified and the rendered output is retained for retry.
- **FR-013**: The clipboard destination MUST place the page on the Windows clipboard as a standard bitmap pasteable into common applications.
- **FR-014**: The Windows printer destination MUST present the standard Windows print dialog for the completed job; cancellation retains the job as pending. PDF output is provided by the user selecting the system's "Microsoft Print to PDF" printer — the feature includes no dedicated PDF generation.
- **FR-015**: For the clipboard and PNG destinations, a job spanning multiple form lengths MUST render as one continuous image (fanfold paper); for the Windows printer destination, the same job MUST be split at page boundaries with no lost or duplicated content.
- **FR-016**: A form feed completes the current page. The user MUST also have an explicit "eject page / finish job" action for output the guest never terminates, and the emulator MUST indicate when unejected output is pending.

**Printer indicator and panel**

- **FR-019**: When the printer card is enabled, the emulator MUST display a compact printer indicator in the window chrome that never disturbs the disk-drive widgets' centered composition, and MUST provide a docked panel containing the skeuomorphic printer view — an ImageWriter-class printer with its four-color ribbon visible and paper rising from the platen showing the rendered output so far. Neither surface appears when the card is disabled.
- **FR-020**: The panel MUST reveal automatically on the guest's first engagement of the card (slot-firmware activation or first data byte written) and MUST be openable and dismissible by user action at any time. Indicator and panel MUST reflect printer state — idle, receiving data, pending un-ejected output, and output-delivery failure (error light) — and the pending state MUST remain visible on the indicator whenever the panel is closed. The panel's Form Feed control performs the FR-016 eject/finish-job action.
- **FR-021**: Indicator and panel MUST summarize the virtual printer configuration (printer model, ribbon type, interface type, slot number) on hover, so users can answer guest software setup menus without external documentation.

**Print-title recognition**

- **FR-022**: At Drive 1 mount time, the emulator MUST check the disk image's filename against a curated, bundled list of print-centric title signatures (case-insensitive substring match).
- **FR-023**: When the mounted image exposes a readable standard filesystem (DOS 3.3 or ProDOS), the emulator MUST additionally match the volume and catalog file names against curated signatures. Both recognition checks MUST be pure functions over supplied data (filename string, decoded catalog entries) so unit tests can drive them synthetically.
- **FR-024**: On recognition, the emulator MUST surface one brief, dismissible notice per mount stating that the printer is available and what to answer in the software's setup menus. Recognition MUST NOT change any configuration (card enablement, slot, destination) and MUST do nothing at all for unrecognized or unreadable disks.
- **FR-025**: When the disk image container carries embedded descriptive metadata (e.g. a WOZ META chunk with a title), recognition MUST match that metadata against the curated signatures as the highest-confidence tier, ahead of filename and filesystem checks. (Note: DOS 3.3 filesystems have no volume name — catalog file names, stored as high-bit ASCII, are the matchable strings there; ProDOS volume names are plain ASCII.)

**Quality constraints**

- **FR-017**: The printer interpretation and rasterization component MUST consume plain byte streams with no dependency on the emulator machine, user interface, or any system service, so unit tests can drive it with synthetic streams and verify rasters against golden references.
- **FR-018**: Printing MUST NOT measurably degrade emulation speed, audio continuity, or video smoothness while the guest is sending print data.

### Key Entities

- **Printer Interface Card**: The emulated slot peripheral the guest talks to — accepts data bytes, reports readiness, and carries the slot firmware. Configured per machine (enabled/disabled, slot number).
- **Emulated Printer**: Consumes the card's byte stream, maintains interpretation state (pitch, spacing, color, position), and places dots on the page raster.
- **Page Raster**: The in-progress image of the current page (or continuous strip) at the printer's maximum dot resolution, in color.
- **Print Job**: One or more completed pages (or one continuous strip) awaiting or undergoing delivery to the selected destination; survives guest resets until delivered or discarded.
- **Output Destination Setting**: The persisted user choice of clipboard, PNG folder, or Windows printer, plus destination-specific options (e.g. PNG folder path).
- **Slot Firmware**: The original firmware image the card exposes to the guest, built from source in this repository.
- **Print-Title Signature List**: Curated, bundled data (filename substrings and filesystem volume/file names) identifying known print-centric software; informational only — it drives the mount-time notice, never configuration.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can boot an unmodified The Print Shop disk, print a sign or greeting card, and obtain a complete, faithful PNG of the page with no manual steps beyond Print Shop's own flow plus (at most) one eject action.
- **SC-002**: Print Shop four-color-ribbon prints render all seven ribbon colors correctly, verified against golden reference rasters for each color and each composite.
- **SC-003**: A multi-page Print Shop banner produces a single continuous image on the PNG/clipboard destinations — zero seams, zero missing or duplicated rows at former page boundaries — and the same content split cleanly into pages on the Windows printer destination.
- **SC-004**: `PR#1` followed by `LIST` or `CATALOG` yields a readable text printout on the first attempt.
- **SC-005**: Automated tests reproduce golden rasters bit-for-bit for every supported command (text, all pitches, line spacing, all graphics densities, all seven colors, reset), and run without touching files, clipboard, printers, or any other system state.
- **SC-006**: Emulation remains at full speed with uninterrupted audio and video while a print job is streaming.
- **SC-007**: Switching the output destination in settings takes effect for the next print without restarting the emulator.
- **SC-008**: A user with no documentation can answer Print Shop's setup questions (printer, ribbon type, interface, slot) correctly using only what the emulator UI shows them.

## Assumptions

- **Print Shop will drive an ImageWriter II through a parallel-type interface card.** Print Shop's printer and interface-card menus are independent selections, so this pairing is expected to work even though the physical ImageWriter II was serial. If testing proves Print Shop's ImageWriter II driver requires serial-card firmware behavior, emulating a Super Serial Card becomes required follow-on work — it is explicitly out of scope for this feature until that is proven necessary. Setup-menu evidence (2026-07-07) supports the pairing: Print Shop's printer and interface lists are independent selections, offering "Apple DMP, ImageWriter, Scribe" alongside "Apple II Parallel Interface", and its ribbon menu offers a four-color ribbon in the original Print Shop. End-to-end byte flow remains to be verified once the card exists.
- Slot 1 is the default printer slot, per long-standing Apple II convention.
- The emulated printer always has a color ribbon installed; a black-ribbon-only mode is not modeled.
- Default paper is US Letter fanfold (8.5 × 11 inches) at the printer's default 6 lines per inch.
- Downloadable custom character sets, proportional-text justification, and MouseText character printing are not exercised by the flagship scenarios and are out of scope.
- The standard Windows print dialog suffices for the initial release; a themed in-app (dxui) print dialog is a later follow-on, out of scope here.
- PDF output relies on the "Microsoft Print to PDF" system printer; no dedicated PDF writer is built.
- Physical-printer theater — print-head sound effects, bidirectional head-travel timing, ribbon wear artifacts — is not modeled.
- Print-title recognition is heuristic and intentionally low-consequence: a false negative is fully covered by the engagement-triggered panel reveal, and a false positive costs one dismissible notice. Copy-protected originals with non-standard filesystems are expected to go unrecognized.

## Out of Scope

- Super Serial Card / 6551 serial interface emulation (contingency only, per Assumptions).
- Epson ESC/P or any second printer command set.
- Custom dxui print dialog (follow-on).
- Dedicated PDF generation.
- Print-head audio emulation and other physical-fidelity theater.
- Apple IIgs printing paths.
