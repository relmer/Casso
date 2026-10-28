# History

While the machine runs, Casso takes snapshots of it and logs your input, so you
can go back to any point in the last few minutes, look around, and run on from
there. History is on by default, and works the same with the debugger open or
closed: the debugger's timeline and the emulator window's rewind bar are one
history.

## Rewinding

- **F2**: while it is down, the machine runs backward, a frame at a time, with
  each frame drawn and the sound silent. Let go, and the machine pauses on the
  frame on screen. The guest never receives F2.
- **History > Rewind 10 seconds** and **History > Forward 10 seconds** pause
  the machine and move ten seconds back, or toward the live end. Each stops
  at the oldest point or the live end.
- **History > Step back** (Ctrl+F11) steps back one instruction.
- **History > Go live** (Ctrl+F2) returns to where the machine was before you
  went back.
- **History > History on** turns recording on or off.

## The rewind bar

Move the pointer to the bottom edge of the emulated screen, and the rewind bar
opens over the screen. It has small pictures of the screen from the oldest
point in history to the live end, the position the machine is at, and marks
for resets, power cycles, disk changes, loaded states and gaps. Rest the
pointer on a picture to see it full size. Click or drag along the bar to move
the machine there; the machine is paused where you let go. Move the pointer
away and the bar closes.

## Running from the past

Press Run anywhere behind the live end, and the machine replays what it did,
your input included, with a pulsing "Replaying" note and the time of the
position over the screen. The machine is live again once the replay is at the
live end.

Change anything while the machine is behind live (type a key the guest reads,
change a disk, reset, edit memory in the debugger) and history forks there:
what happened after that point becomes a branch, and the machine runs on from
the change. There is no question first, and no history is lost.

If a disk's file changes outside Casso while the machine is behind live, the
"Disk modified outside Casso" message appears once the machine is live again.
Behind live, the screen is drawn with each disk as it was at that moment.

## Branches

A fork is a mark on the timeline, a line with a branch rising from it, and a
count when more than one branch forks there. Rest the pointer on the mark to
list the branches: the one you are on first, then the others, the most recently
left first. Each row has the branch's last frame, how long it ran past the fork,
and when you left it. Rest on a row to see that frame beside the list, above
the bar; click it to switch to that branch, which pauses the machine at the
branch's live end, exactly as it was there. The path you left becomes a branch in turn.

Rewinding, stepping and the timeline follow the active branch only. All
branches share the history budget: when it is full, the oldest parts of the
branches you left go first, and the active branch's history goes last. There
are at most 8 branches you left; past that, the oldest is dropped whole.

## Saved history

History is saved with the machine's automatic state at exit and on a machine
switch, and loaded at the next launch, branches included: the timeline spans
the same history, and a save made behind live resumes there, paused. File >
Save state includes history too, and loading such a file replaces the history
with the file's. Loading a file without history marks the load as a point in
the current history; behind live, history forks there first.

History saved by a build that saves the machine differently, or saved history
that does not read, is ignored: the machine loads, and history starts afresh
from there.

Until disk versions are saved per branch, history is not saved while it
includes a disk taken out of a drive; the machine is saved without it.

## Settings

The History page of Settings has history on or off, and the memory budget (64
MB by default). The History page of the debugger's options edits the same
settings. Turning history off deletes all of the machine's history, in memory
and saved, after a confirmation.

While the Uthernet II has a socket or a connection open, history is off: its
connections are on the host, and no replay can open them again. Recording
starts again once all are closed.
