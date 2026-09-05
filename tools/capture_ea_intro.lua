-- Captures the original $C1:4DE2 EA Sports presentation from a user-supplied
-- Madden NFL '95 ROM.  The generated PNG files are temporary extractor input;
-- they are never part of the source repository.

local output_dir = os.getenv("MF95_CAPTURE_DIR")
local capture_step = tonumber(os.getenv("MF95_CAPTURE_STEP") or "2")
local last_frame = tonumber(os.getenv("MF95_CAPTURE_LAST") or "330")
local boot_frames = tonumber(os.getenv("MF95_CAPTURE_BOOT") or "57")

if not output_dir or output_dir == "" then
    error("MF95_CAPTURE_DIR is required")
end

local loaded = assert(io.open(output_dir .. "/script.loaded", "w"))
loaded:write("capture script loaded\n")
loaded:close()

local global_frame = 0
local started = false
local intro_frame = 0

emu.addEventCallback(function()
    -- The verified USA ROM reaches $C1:4DE2 on global frame 57 with a neutral
    -- controller.  A frame trigger avoids debugger address-domain differences
    -- while remaining deterministic because the extractor rejects every other
    -- ROM hash.
    if not started and global_frame >= boot_frames then
        started = true
        intro_frame = 0
    end

    if not started then
        global_frame = global_frame + 1
        return
    end

    if intro_frame % capture_step == 0 then
        local filename = string.format("%s/frame_%04d.png", output_dir, intro_frame)
        local screenshot = assert(io.open(filename, "wb"))
        screenshot:write(emu.takeScreenshot())
        screenshot:close()
    end

    if intro_frame >= last_frame then
        local done = io.open(output_dir .. "/capture.done", "w")
        if done then
            done:write(string.format("last_frame=%d\nstep=%d\n", last_frame, capture_step))
            done:flush()
            done:close()
        end
        emu.stop(0)
        return
    end

    intro_frame = intro_frame + 1
    global_frame = global_frame + 1
end, emu.eventType.endFrame)
