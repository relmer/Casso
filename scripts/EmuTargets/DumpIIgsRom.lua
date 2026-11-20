-- Writes the Apple IIgs's ROM 03, $FC0000 to $FFFFFF, from MAME's apple2gs
-- to the file CASSO_IIGS_ROM_OUT names, then ends MAME. KEGS and GSplus take
-- the ROM as one 256 KB image; MAME's two ROM files joined end to end aren't
-- that image.
local done = false

emu.register_frame_done(function()
    if done then return end
    done = true

    local space = manager.machine.devices[":maincpu"].spaces["program"]
    local out   = io.open(os.getenv("CASSO_IIGS_ROM_OUT"), "wb")

    for address = 0xFC0000, 0xFFFFFF do
        out:write(string.char(space:read_u8(address)))
    end

    out:close()
    manager.machine:exit()
end)
