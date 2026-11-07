--[[
    NewOpcodes drawing and audio stream check: what the Wine tests cannot
    run, through moonloader/lib/newopcodes.lua.

    Copy into moonloader/tests/ and run it from the /sh menu: once you are
    spawned it draws for 10 seconds: a red-to-blue square at the top left,
    the samp cursor and a car light texture as sprites (one turning),
    "moonsamp draw check" in white, a corona and a light above you, a shadow
    under you and a spotlight shining down in front of you. Then it writes
    moonloader/logs/draw_check.log and unloads.
]]
script_name('draw_check')

local out, passed, failed = {}, 0, 0

local function fmt(v)
    if type(v) == 'number' and math.floor(v) ~= v then return string.format('%.4f', v) end
    return tostring(v)
end

local function check(label, ok, ...)
    local detail = {}
    for i = 1, select('#', ...) do detail[#detail + 1] = fmt(select(i, ...)) end
    out[#out + 1] = string.format('CHECK %-52s %s  %s', label, ok and 'ok' or 'FAIL', table.concat(detail, ', '))
    if ok then passed = passed + 1 else failed = failed + 1 end
end

local function close(a, b, eps) return type(a) == 'number' and math.abs(a - b) < (eps or 0.01) end

local function run(no)
    -- textures
    check('loadTxd models\\generic\\vehicle.txd', no.loadTxd('mscheck', 'models\\generic\\vehicle.txd'))
    local tok, lights = no.findTextureInTxdWithName('vehiclelights128', 'mscheck')
    check('findTextureInTxdWithName vehiclelights128', tok and lights ~= 0, lights)
    local pok, cursor = no.loadTextureFromPngFile('mouse.png')
    check('loadTextureFromPngFile mouse.png', pok and cursor ~= 0, cursor)
    local dok, icons = no.loadTextureFromDdsFile('moonloader\\resource\\fastmap\\sa_map_icons.dds')
    check('loadTextureFromDdsFile sa_map_icons.dds', dok and icons ~= 0, icons)
    check('loadTextureFromBmpFile of a missing file is false', not no.loadTextureFromBmpFile('nothere.bmp', ''))

    -- audio stream
    local sound = getWorkingDirectory() .. '\\resource\\audio\\hitsounds\\p1_bell.mp3'
    local stream = doesFileExist(sound) and loadAudioStream(sound)
    if stream then
        setAudioStreamVolume(stream, 0)
        setAudioStreamState(stream, 1)
        wait(100)
        no.setAudioStreamPositionTime(stream, 0.5)
        local t = no.getAudioStreamPositionTime(stream)
        check('setAudioStreamPositionTime / get', close(t, 0.5, 0.05), t)
        local fx = no.setAudioStreamEffect(stream, 3, 0)         -- BASS_FX_DX8_ECHO
        check('setAudioStreamEffect echo gives an effect', fx ~= 0, fx)
        no.resetAudioStreamEffect(fx)
        no.removeAudioStreamEffect(stream, fx)
        no.setAudioStreamAttribute(stream, 3, -0.5)              -- BASS_ATTRIB_PAN
        local pan = no.getAudioStreamAttribute(stream, 3)
        check('setAudioStreamAttribute pan / get', close(pan, -0.5), pan)
        local mode = no.get3dAudioStreamAttributes(stream)
        out[#out + 1] = 'VALUE get3dAudioStreamAttributes mode ' .. tostring(mode)
        setAudioStreamState(stream, 0)
        releaseAudioStream(stream)
    else
        out[#out + 1] = 'SKIP  no hit sound to load: the audio stream checks did not run'
    end

    -- 10 seconds of drawing
    local queued = no.drawShape(5, 0, 4, 1, true, 5, 6, 0)
    no.setupShapeVertex(1, 1, 40, 40, 0, 1, 255, 0, 0, 255, 0, 0, false, false)
    no.setupShapeVertex(1, 2, 140, 40, 0, 1, 255, 0, 0, 255, 1, 0, false, false)
    no.setupShapeVertex(1, 3, 140, 140, 0, 1, 0, 0, 255, 255, 1, 1, false, false)
    no.setupShapeVertex(1, 4, 40, 140, 0, 1, 0, 0, 255, 255, 0, 1, false, false)
    check('drawShape queues', queued)
    local t0 = os.clock()
    while os.clock() - t0 < 10 do
        local x, y, z = getCharCoordinates(PLAYER_PED)
        local h = math.rad(getCharHeading(PLAYER_PED))
        local fx, fy = x - math.sin(h) * 6, y + math.cos(h) * 6
        no.drawShape(5, 0, 4, 1, true, 5, 6, 0)
        if cursor ~= 0 then no.draw2dSprite(cursor, 160, 40, 200, 80, 255, 255, 255, 255, 0) end
        if lights ~= 0 then
            no.draw2dSpriteWithGradient(lights, 220, 40, 280, 100, 255, 255, 255, 255, 255, 0, 0, 255,
                0, 255, 0, 255, 0, 0, 255, 255, (os.clock() - t0) * 90)
        end
        no.printTemporaryText('moonsamp draw check', 40, 300, 0.6, 1.4, 1)
        no.printTemporaryTextEx('centred, yellow, background', 640, 340, 0.5, 1.2, 2, true, 0, 600, false,
            255, 255, 0, 255, 1, 0, 0, 0, 0, 255, true, 0, 0, 0, 160)
        no.drawTemporaryCorona(0, 255, 255, 0, 255, 0, x, y, z + 1.6, 0.6)
        no.drawTemporaryLight(0, x, y, z + 1.5, 0, 0, -1, 6, 255, 255, 0, 0)
        no.drawTemporaryShadow(1, x, y, z, 3, 3, 45, 4, 3, 200, 255, 0, 0, 0)
        no.drawSpotlight(fx, fy, z + 8, fx, fy, z - 1, 0.1, 2.5, false, 255, false, false)
        wait(0)
    end
    if cursor ~= 0 then no.cleanLoadedTexture(cursor) end
    if icons ~= 0 then no.cleanLoadedTexture(icons) end
end

function main()
    while not isPlayerPlaying(PLAYER_HANDLE) do wait(100) end
    wait(3000)
    local ok, no = pcall(require, 'newopcodes')
    if not ok then
        out[#out + 1] = 'FAIL  require newopcodes: ' .. tostring(no)
        failed = failed + 1
    elseif not no.available then
        out[#out + 1] = 'FAIL  newopcodes: moonsamp.asi has no moonsamp_run'
        failed = failed + 1
    else
        local good, err = pcall(run, no)
        if not good then
            out[#out + 1] = 'FAIL  error: ' .. tostring(err)
            failed = failed + 1
        end
    end
    table.insert(out, 1, string.format('draw check: %d passed, %d failed', passed, failed))
    local f = io.open(getWorkingDirectory() .. '\\logs\\draw_check.log', 'w')
    if f then
        f:write(table.concat(out, '\n'), '\n')
        f:close()
    end
    sampAddChatMessage(string.format('[draw check] %d passed, %d failed: logs\\draw_check.log', passed, failed), 0xFFFFFF)
    thisScript():unload()
end
