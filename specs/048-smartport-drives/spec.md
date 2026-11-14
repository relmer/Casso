# Feature Specification: SmartPort and hard drive devices

**Feature Branch**: `048-smartport-drives`

**Created**: 2026-11-13

**Status**: Draft

**Input**: User description: "Emulated SmartPort and hard drive devices for Casso, so ProDOS volumes larger than 140K (800K, 5 MB, 20 MB, 32 MB) boot and run on Casso's emulated machines. Six devices, all on this spec, and the //c Plus to host the Apple 3.5 Drive: the Apple UniDisk 3.5, the Apple 3.5 Drive, the Apple II SCSI Card with an Apple Hard Disk 20SC, the ProFile on the ProFile interface card, the CFFA 2.0 and CFFA 3000 (later dropped by the owner), and the A2Pico running A2retroNET's SmartPort firmware (also dropped later). Real firmware with the hardware around it emulated faithfully, clean-room, and a realistic 3D desk scene model, audio and event log for each external drive."

## Overview

Casso emulates the Disk II and nothing larger, so a ProDOS volume over 140K
cannot boot in it. Spec 033 found this while building Casso Explorer's A2Pico
and CFFA support: the images Casso Explorer reads and writes for those cards
were checked in MAME, because Casso had no card to boot them on. This spec adds
the cards and drives that boot them.

Every device runs its real firmware, which is Apple's ROM for that device.
Casso emulates the hardware
those programs drive (the NCR 5380 SCSI controller, the IWM and SWIM disk
controllers, and the 65C02 inside the UniDisk 3.5 and the 3.5 controller card),
so that the firmware runs unmodified and software that accesses the hardware
directly works as it does on a real machine.

Each external drive appears on the 3D desk as a model of the real unit, with
the interaction, sound and event log the Disk II already has.

## Clarifications

### Session 2026-11-13

- Q: Which drives does this spec emulate? A: All six offered: the UniDisk 3.5,
  the Apple 3.5 Drive, the Apple II SCSI Card with an HD20SC, the ProFile, the
  CFFA 2.0 and 3000, and the A2Pico with A2retroNET.
- Q: The Apple 3.5 Drive worked only on a //c Plus, a IIgs or the 3.5
  controller card, and Casso has neither machine. Add the //c Plus? A: Yes,
  this spec adds the //c Plus as a machine.
- Q: Should hard disks have their real spin-up and seek times? A: Real timing is
  available as a per-drive setting, and the default is speed: drives are ready
  at power-on and seek without delay. The sounds are audible either way.
- Q: The CFFA 3000's firmware runs on an AVR microcontroller. How should Casso
  run it? A: Drop the CFFA from Casso entirely, the 2.0 and the 3000. It can be
  added later. Casso Explorer's CFFA card layout support from spec 033 stays.
- Q: Where does the device event log appear? A: In the debugger of spec 035,
  built on this spec's event feeds. This spec does not add a standalone log window.
- Q: Does any other emulator model the A2Pico? A: None found. MAME 0.289, MAME's
  current source and AppleWin have no A2Pico or A2retroNET device.
- Q: Should the A2Pico stay? A: Drop it from this spec as well. It can be added later.
  Spec 033's A2Pico card folder support in Casso Explorer stays.
- Q: What is a SCSI disk whose image is over 20 MB? A: Always an HD20SC. The
  model number on the front of the box follows the image's size in MB: "20SC"
  for 20 MB, "32SC" for a 32 MB image. Reference photograph:
  https://macs.billiehawkins.com/wp-content/uploads/2018/11/20sc.jpg
- Q: Are writes to a 3.5-inch WOZ image saved to the WOZ file? A: Yes, by the
  rules Casso already applies to a 5.25-inch WOZ image on the Disk II,
  including its write-protect and save rules.
- Q: How does Casso's CPU speed setting work on the //c Plus? A: "Authentic" is
  4 MHz and the Machine tab displays 4 MHz, not 1. "2x" is 8 MHz. "Max" is max.
- Q: Are the new cards part of a machine's default setup? A: Yes, on by
  default on the //e and enhanced //e: the SCSI card in slot 7 with an HD20SC,
  and the UniDisk 3.5 interface card in slot 5 with a UniDisk 3.5. The time the
  cards add to a floppy boot is measured and reported, with no limit set.
- Q: What does the default HD20SC contain? A: A stock image: an empty ProDOS
  volume with a blank boot block, so it is not bootable and floppies boot as
  they do today. A bootable image the user attaches boots ahead of the floppy,
  as on real hardware, where a floppy then takes Ctrl-Reset and `PR#6`.
- Q: Does the //c get a UniDisk 3.5 on its disk port by default? A: No. The
  //c's default setup stays as it is, and the user adds the drive from the
  Machine tab.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Boot a hard disk on the Apple II SCSI Card (Priority: P1)

A user with a 32 MB ProDOS hard disk image (Total Replay, a GS/OS-era 8-bit
collection, or one they made with `casso disk create --size 32M --bootable`)
puts an Apple II SCSI Card in slot 7 of an enhanced //e, attaches an Apple Hard
Disk 20SC with the image at SCSI ID 6, and turns the machine on. The card's ROM
boots ProDOS from the hard disk, and the user runs software from it, saves
files to it, and finds those files in the image after quitting Casso. A second
image at another SCSI ID appears as a second ProDOS volume.

**Why this priority**: Most users with a large image are after a hard disk, and the SCSI card is the device with reference boots in MAME to compare
against. It also exercises everything the later stories build on: a block
device behind a slot card, the desk scene model, the audio and the log.

**Independent Test**: Boot each image in the MAME reference table of spec 033's
`validation.md` on the SCSI card in Casso, and compare the screen to MAME's.

**Acceptance Scenarios**:

1. **Given** an enhanced //e with the SCSI card in slot 7 and a bootable 32 MB
   `.hdv` at ID 6, **When** the machine starts, **Then** ProDOS boots from the
   hard disk to the same screen as in MAME.
2. **Given** the booted hard disk, **When** a program saves a file, **Then**
   the file is in the image on the host after Casso quits, and the image opens
   in Casso Explorer with no integrity errors.
3. **Given** images at two SCSI IDs, **When** the user catalogs from ProDOS,
   **Then** both volumes are online.
4. **Given** the desk scene theme, **When** the hard disk is read or written,
   **Then** the activity lamp on the HD20SC model lights, and its seek
   sounds are audible.
5. **Given** the SCSI card with no image attached at any ID, **When** the
   machine starts, **Then** the card's ROM moves on to the next slot, as on
   real hardware.

---

### User Story 2 - Use a UniDisk 3.5 on a //c and on a //e (Priority: P2)

A //c owner who had a UniDisk 3.5 plugs one into the //c's disk port in Casso,
inserts an 800K disk image, and boots it. The same drive on the UniDisk 3.5
interface card in a ][+ or //e boots the same image. A second UniDisk 3.5
chains behind the first, and a 5.25-inch drive at the end of the chain continues
to work on the //c.

**Why this priority**: The UniDisk 3.5 is the 800K drive a //c owner actually
had. It is also the most demanding device in the spec: the drive contains a
65C02 and an IWM, so Casso runs a second processor per drive.

**Independent Test**: Boot an 800K ProDOS image on a //c through the disk port,
and the same image on a //e through the interface card.

**Acceptance Scenarios**:

1. **Given** a //c with a UniDisk 3.5 on the disk port and a bootable 800K
   image inserted, **When** the machine starts, **Then** ProDOS boots from it.
2. **Given** an enhanced //e with the UniDisk 3.5 interface card in slot 5,
   **When** the machine starts with slot 5 as the boot slot, **Then** the same
   image boots.
3. **Given** two UniDisk 3.5 drives in a chain, **When** ProDOS starts,
   **Then** both are online, with the unit numbers the firmware assigns.
4. **Given** the desk scene, **When** the user clicks the drive's eject area,
   **Then** the disk ejects with the 3.5-inch drive's eject sound, the model
   is drawn empty, and the disk browser opens.
5. **Given** software that ejects the disk through SmartPort, **When** it runs,
   **Then** the drive ejects the disk in the same way.

---

### User Story 3 - Use an Apple 3.5 Drive, including copy-protected disks (Priority: P2)

A user inserts an 800K disk image into an Apple 3.5 Drive on the Apple II 3.5
Disk Controller Card in a //e and boots it. Because this drive has no internal
controller, Casso emulates it at the level of the disk controller's read and
write signals, so a WOZ image of a copy-protected 3.5-inch disk boots as it
does on the real drive.

**Why this priority**: It completes the 3.5-inch hardware Apple sold for 8-bit
machines, and it is the only device here that can run 3.5-inch copy
protection, since the WOZ image contains the flux that the protection checks.

**Independent Test**: Boot an 800K `.po` and a 3.5-inch WOZ image on the Apple
3.5 Drive.

**Acceptance Scenarios**:

1. **Given** a //e with the 3.5 controller card and an Apple 3.5 Drive with a
   bootable 800K `.po`, **When** the machine starts, **Then** ProDOS boots.
2. **Given** a 3.5-inch WOZ image of a copy-protected disk, **When** it boots,
   **Then** the protection check passes as it does on real hardware.
3. **Given** a DiskCopy 4.2 image, **When** a program writes to it, **Then**
   the written blocks are in the image after Casso quits, and the DiskCopy
   checksums in its header are updated.
4. **Given** the drive's motor, head and eject activity, **When** the drive
   runs, **Then** the activity appears on the desk and in the event log as it
   happens.

---

### User Story 4 - Run a //c Plus (Priority: P2)

A user picks the Apple //c Plus in the Machine tab. It starts with its ROM,
boots an 800K disk from its built-in 3.5-inch drive, and runs at its 4 MHz
accelerated speed or at 1 MHz when the user turns acceleration off, as the
real machine does when Esc is down at power-on or through its control panel. Its
external disk port takes an Apple 3.5 Drive, a UniDisk 3.5 or a 5.25-inch
drive.

**Why this priority**: The //c Plus is the machine the Apple 3.5 Drive was
made for, and the owner chose to add it here so the drive works on its real
port and not only on a card.

**Independent Test**: Boot an 800K ProDOS image on the //c Plus's internal
drive, and a second image on an external Apple 3.5 Drive.

**Acceptance Scenarios**:

1. **Given** the //c Plus with a bootable 800K image in its internal drive,
   **When** the machine starts, **Then** ProDOS boots from it.
2. **Given** acceleration on, **When** a timing loop runs, **Then** it runs at
   the //c Plus's accelerated speed, and at 1 MHz with acceleration off, with
   the real accelerator's slowdowns for slot and speaker accesses.
3. **Given** an Apple 3.5 Drive, a UniDisk 3.5 or a 5.25-inch drive on the
   external port, **When** the machine starts, **Then** each is online.
4. **Given** the desk scene, **When** the //c Plus is selected, **Then** its
   internal 3.5-inch drive is drawn as a drive on the desk, in the way the
   //c's internal drive is drawn today.

---

### User Story 5 - Use a ProFile (Priority: P3)

A user puts a ProFile interface card in a slot of a ][+ or //e, attaches a
ProFile 5 MB or 10 MB with an image, and boots ProDOS from it. The ProFile
appears on the desk as the wide beige box it was, and sounds like one.

**Why this priority**: The ProFile is the oldest and least common device in
the spec, and other devices boot the same images. It is here for the hardware
it is, not for capability the others lack.

**Independent Test**: Boot a 5 MB image on a ProFile in Casso.

**Acceptance Scenarios**:

1. **Given** a //e with a ProFile card and a bootable 5 MB image on a ProFile,
   **When** the machine starts, **Then** ProDOS boots from it.
2. **Given** an image larger than the attached ProFile model, **When** the
   user attaches it, **Then** the attach fails with an error that contains the
   model's capacity, and no truncated volume is presented.

---

### User Story 6 - Configure cards and drives, and attach images (Priority: P1)

A user adds any of these cards to a slot (or, on a //c, a drive to the disk
port) from the Machine tab of Settings, chooses which drives hang off it, and
attaches an image to each drive from the drive on the desk or from the command
line. The configuration persists across launches the way the Disk II's does.

**Why this priority**: Without it no other story is reachable from the
application. It is P1 because every device depends on it.

**Independent Test**: Add each card to a slot, attach images, quit, relaunch,
and confirm the same configuration and images come back.

**Acceptance Scenarios**:

1. **Given** the Machine tab, **When** the user adds a SCSI card to slot 7,
   **Then** the card and its SCSI IDs appear in the device tree, and the user can
   attach an HD20SC at any free ID.
2. **Given** a configured drive, **When** the user drops an image onto its
   model on the desk, **Then** the image is attached, as dropping onto a Disk II
   does.
3. **Given** a command line that attaches an image to a block device, **When**
   Casso starts, **Then** the image is attached to that device for this run.
4. **Given** a machine that cannot host a card (for example a 3.5 controller
   card on a ][+), **When** the user opens the card list, **Then** the card is
   unavailable, and its tip lists the machines that accept it.
5. **Given** an image whose size or format the chosen drive cannot present,
   **When** the user attaches it, **Then** the attach fails with an error that
   contains the drive's limits.

---

### User Story 7 - See, hear and watch every drive (Priority: P2)

In the desk scene theme, every attached external drive appears on the desk as
a realistic model of the real unit: the UniDisk 3.5, the Apple 3.5 Drive, the
HD20SC and the ProFile. Each has the Disk II's interaction: an activity lamp,
a visible loaded or empty state for removable media, a tip with the mounted
image and write-protect state, click to eject and browse, and click the body
to browse. Each has a set of sounds, mixed with the Disk II's. Each emits
an event feed, built as the Disk II's is, for the debugger of spec 035. In the compact
themes each drive is a 2D widget in the drive band.

**Why this priority**: Per the owner, every drive gets the full desk scene
treatment the Disk II has. It is P2 because the drives must work before there
is anything to draw.

**Independent Test**: Attach each drive in the desk scene, boot, and check the
lamp, the tip, the clicks and the sounds against the Disk II, and the events
on each device's feed.

**Acceptance Scenarios**:

1. **Given** each external drive attached, **When** the user looks at the desk,
   **Then** each appears as a model whose proportions match measurements of the
   real unit, with Apple's colors and badges for its era.
2. **Given** a hard disk at power-on, **When** the drive spins up, **Then** its spin-up
   sound is audible, followed by seek sounds on its accesses.
3. **Given** a hard disk with real timing turned on, **When** the machine
   starts, **Then** the firmware waits for the spin-up as on real hardware;
   with the default setting the drive is ready at once and seeks without
   delay.
4. **Given** a sink attached to a block device's event feed, **When** a block
   is read, **Then** the event contains the device, unit, operation and block
   number.
5. **Given** the compact theme, **When** drives are attached, **Then** each has
   a widget in the drive band with the same tip and clicks.

---

### Edge Cases

- A card is added to a slot that already has a card: the Machine tab replaces
  it only after the user confirms, as for other cards.
- Two drives are attached to the same image file: the second attach
  fails, since two writers on one file corrupt it.
- The image is read-only on the host, or locked by another program: the drive
  reports write-protected to the firmware, and the tip states why, in the Disk
  II's wording.
- The image changes on the host while attached (Casso Explorer writes to it):
  the existing external change handling of spec 033 applies.
- An image larger than the device can address (over 32 MB for a ProDOS block
  device, more than the ProFile model's capacity): the attach fails, with
  the limit in the message.
- A 2IMG image's header specifies DOS order or a locked flag: a DOS-order 2IMG
  fails to attach to a block device, and the locked flag is presented as write
  protection.
- A 3.5-inch WOZ image is attached to a drive that cannot read flux (a SCSI
  disk or a ProFile): Casso decodes it to blocks when every block decodes, and
  otherwise fails the attach with the unreadable block numbers.
- A machine reset mid-write: the block in progress is written whole or not at
  all, never in part.
- A required ROM is missing: the card appears in the Machine tab with the
  missing ROM file name and the folders Casso searched, and the machine does not start
  that card, rather than running with a blank ROM.
- The firmware selects a SCSI ID with nothing attached: the target does not
  respond, and the firmware times out as on real hardware.
- The chain on the //c's disk port has more devices than the //c ROM supports:
  Casso prevents configuring it, and the tip states the limit.

## Requirements *(mandatory)*

### Functional Requirements

**Firmware and fidelity**

- **FR-001**: Each device MUST run its real firmware unmodified: Apple's ROMs
  for the Apple II SCSI Card (341-0437-A), the UniDisk 3.5 interface card, the
  UniDisk 3.5 drive's internal controller, the Apple II 3.5 Disk Controller
  Card and the ProFile interface card; and the //c's ROM 4 for its disk port and the
  //c Plus's ROM for its internal and external drives.
- **FR-002**: ROMs not licensed for redistribution MUST be fetched by
  `scripts/FetchRoms.ps1` and never committed.
- **FR-002a**: Casso MUST download each new ROM itself, as it downloads the
  machine ROMs: a default card's ROM at startup, and any other card's or
  machine's ROM when the user selects it.
- **FR-003**: The hardware each firmware drives MUST be emulated at the
  register and timing level the firmware depends on: the NCR 5380 and SCSI bus
  phases for the SCSI card; the IWM for the //c disk port, the UniDisk 3.5
  interface card and the drive's internal controller; the SWIM for the 3.5
  controller card; the 65C02 inside the UniDisk 3.5 and on the 3.5 controller
  card; and the ProFile's handshake.
- **FR-004**: The emulation MUST be clean-room. GPL sources (MAME, AppleWin)
  MAY be read for behavior, and MUST NOT be copied or translated.
- **FR-005**: The SmartPort bus between a host and a UniDisk 3.5 MUST be
  emulated at the signal level the host firmware uses, so that software which
  accesses SmartPort devices directly, and not through the ROM's entry point,
  works.
- **FR-006**: The Apple 3.5 Drive and the 3.5-inch mechanism inside the UniDisk
  3.5 MUST be emulated as variable-speed GCR drives, with the zones, head
  stepping, side select, eject and disk-in-place status the controllers read.

**Media**

- **FR-007**: Block devices MUST accept ProDOS-order images (`.po`, `.hdv`),
  2IMG (`.2mg`) in ProDOS order, and DiskCopy 4.2 images for 800K drives,
  through spec 033's block store and partition readers.
- **FR-008**: The Apple 3.5 Drive and the UniDisk 3.5 MUST accept WOZ 2 images
  of 3.5-inch disks, and MUST present their flux to the controller, so that
  protected disks load as on the real drive. Writes to a 3.5-inch WOZ image
  MUST be saved by the rules that apply to a 5.25-inch WOZ image on the Disk II. Block images on these drives
  MUST be presented as the GCR a real 800K disk has.
- **FR-009**: Writes MUST be committed to the image file on the host through
  the durable commit path the Disk II uses, and a reset or crash mid-write MUST
  NOT result in a partly written block.
- **FR-010**: Each drive MUST present its real capacity limits: 800K for a
  3.5-inch drive (400K for a single-sided disk where the image is one), 5 MB or
  10 MB for the ProFile model, the image's size up to 32 MB per ProDOS volume
  for the SCSI disk.

**Machines and configuration**

- **FR-013**: The SCSI, ProFile and UniDisk 3.5 interface cards
  MUST work in every Casso machine with slots that the real card works in. The
  3.5 controller card MUST work in the //e and enhanced //e. The //c's disk port
  MUST accept a chain of UniDisk 3.5 drives followed by a 5.25-inch drive.
- **FR-013a**: Casso MUST add the Apple //c Plus as a machine: its ROM (fetched,
  never committed), its built-in 3.5-inch drive on its internal disk
  controller, its external disk port accepting the Apple 3.5 Drive, the UniDisk
  3.5 and a 5.25-inch drive, and its accelerator at the real machine's speed,
  with acceleration switchable as on the real machine and the slowdowns the
  real accelerator applies to slot and speaker accesses. Casso's CPU speed setting on the //c
  Plus MUST map "Authentic" to 4 MHz, displayed as 4 MHz in the Machine tab,
  "2x" to 8 MHz, and "Max" to the maximum speed.
- **FR-014**: Users MUST be able to add each card to a slot, attach drives to
  it, and attach an image to each drive from the Machine tab, from the desk
  scene and drive band, and from the command line, and the configuration MUST
  persist across launches. The command-line flags MUST be in `--help`.
- **FR-014a**: The //e and enhanced //e MUST ship with the SCSI card enabled in
  slot 7 with an HD20SC at ID 6, and the UniDisk 3.5 interface card enabled in
  slot 5 with one UniDisk 3.5. A user's existing machine copy MUST gain them
  only where slots 5 and 7 are unconfigured. A machine on which a card's real
  firmware does not boot MUST NOT have that card by default.
- **FR-014b**: The default HD20SC MUST have a stock image, created per user on
  first use: an empty 32 MB ProDOS volume with a blank boot block. With it
  attached, a floppy in slot 6 MUST boot as it does without the card. If
  Apple's SCSI ROM does not pass over a disk with a blank boot block, the
  default HD20SC ships with no image.
- **FR-014c**: The time the default cards add to a floppy boot MUST be
  measured and written into this spec's `validation.md`. It has no limit; the
  owner accepted the delay of Apple's ROM scanning the SCSI bus.
- **FR-015**: The Machine tab MUST offer a card only on a machine that can host
  it, and MUST state why a card is unavailable in its tip.
- **FR-016**: A missing ROM MUST be reported with the file name and the places
  Casso looked, and the card MUST NOT run with an empty ROM.
- **FR-017**: Attaching an image that the drive cannot present MUST fail with
  an error that states the drive's limits, and the image file MUST be left
  untouched.
- **FR-018**: Casso MUST NOT attach one image file to two drives at once.

**Desk scene, audio and debug**

- **FR-019**: Each external drive (UniDisk 3.5, Apple 3.5 Drive, HD20SC,
  ProFile) MUST have a 3D model built from measurements of the real unit, with
  the same pipeline the Disk II, the //c drive and the monitors use.
- **FR-020**: Each drive on the desk MUST have the Disk II's interaction:
  activity lamp, loaded or empty state for removable media, tip with the
  mounted image and write-protect state, click to eject and browse, click the
  body to browse, and drop to attach.
- **FR-021**: Each drive MUST have mechanical sounds of the real unit (spin-up, seek,
  head stepping, the 3.5-inch eject motor), placed in the stereo field by its
  position on the desk, and under the existing drive audio switch.
- **FR-022**: Each block device and 3.5-inch drive MUST emit an event feed
  (bus phase or command, unit, block or track and side, result) built as the
  Disk II's is, for spec 035's debugger to log. The log view itself is spec
  035's.
- **FR-023**: In the compact themes each drive MUST be a widget in the drive
  band, with the same tip and clicks as on the desk.
- **FR-023b**: Each hard disk MUST have a setting for real spin-up and seek
  timing, off by default. With it off the drive is ready at power-on and seeks
  without delay; with it on the firmware waits as on real hardware. The sounds
  are audible in both cases.
- **FR-024**: The desk scene MUST place the attached drives automatically
  beside the existing devices, without overlap, for any configuration the
  Machine tab allows.

**Testing**

- **FR-025**: Each card MUST be constructible in a scenario test machine over
  in-memory images, so that spec 033's block device tests (FR-194, T573) can
  list, read and write blocks through it.
- **FR-026**: Each device MUST be checked against a MAME reference boot where
  MAME 0.289 emulates it, using `scripts/MameSnapshot.ps1`, with the
  comparison written up in this spec's `validation.md`.

### Key Entities

- **Block device card**: A slot card whose firmware presents ProDOS block
  devices: the SCSI card and the ProFile card. It has a ROM, its bus interface, and the drives attached to it.
- **Card aliases**: UniDisk 3.5 interface card and Liron card are one card,
  device id `liron-card`. Apple II 3.5 Disk Controller Card, 3.5 controller
  card and SuperDrive card are one card, device id `apple35-card`.
- **SmartPort chain**: The ordered list of SmartPort devices on a //c disk port
  or a UniDisk 3.5 interface card, ending optionally in a 5.25-inch drive.
- **Drive**: A device the user attaches an image to: a 3.5-inch drive, a SCSI
  hard disk at an ID or a ProFile. It has
  a model, a capacity, a write-protect state and an attached image.
- **Drive model**: The 3D model, sounds and 2D widget of an external drive.
- **Device event**: One entry in a device's event feed: the device, the unit, the
  operation, the block or track, and the result.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every image in spec 033's MAME block device table boots in Casso
  on the SCSI card to the same screen as in MAME.
- **SC-003**: An 800K ProDOS image boots on a UniDisk 3.5 on the //c disk port,
  on the UniDisk 3.5 interface card in a //e, and on an Apple 3.5 Drive.
- **SC-004**: At least one copy-protected 3.5-inch disk, as a WOZ image, boots
  on the Apple 3.5 Drive.
- **SC-005**: A 5 MB image boots on a ProFile.
- **SC-005a**: The //c Plus boots an 800K image from its internal drive and
  from an external Apple 3.5 Drive.
- **SC-006**: A file written by a guest on each device is present in the image
  after Casso quits, and Casso Explorer reports the volume clean.
- **SC-007**: With real timing on, the firmware waits for a hard disk's
  spin-up before it boots; with the default setting the disk is ready at
  power-on.
- **SC-008**: In the desk scene, each external drive is recognizable as the
  real unit to the owner, judged side by side with reference photographs.
- **SC-009**: Spec 033's block device scenario tests run on at least one card
  from this spec.

## Assumptions

- The CFFA 2.0, the CFFA 3000 and the A2Pico are out of scope, by the owner's
  decision. Spec 033's block device tests run on the SCSI card. The IIgs
  is not a Casso machine and is out of scope. The Apple 3.5 Drive
  works on the //c Plus and on the 3.5 controller card in a //e.
- The //c Plus's internal drive is drawn as an Apple 3.5 Drive in the //c
  Plus's colors, as the //c's internal drive is drawn as a //c external drive.
- Every SCSI disk is an HD20SC, whatever the size of its image. The model
  number on the front of the desk model is the image's size in MB followed by
  "SC", so a 32 MB image is drawn as a "32SC".
- The desk scene's automatic placement is enough for this spec. User
  arrangement of devices is spec 009's work and is not started.
- Images Casso writes for these devices are the ones spec 033 already creates
  (`casso disk create --size`), and the large-volume `--bootable` fix on 033
  is present.
- Casso Explorer's block device actions over CassoLink are spec 033's; this
  spec supplies only the cards their tests boot.
- Performance: each emulated 65C02 inside a drive or card runs only while its
  device is active, so a machine with idle drives attached runs as fast as
  one without.
