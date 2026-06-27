-- example_player.lua
-- PocketEngine demo script: ok tuşları ile entity'yi hareket ettirir.
-- Bu script bir entity'ye ScriptEngine::loadScript(entity, "scripts/example_player.lua")
-- ile bağlanır. Bağlandığı entity'nin ID'si `entity_id` globaline yazılır.
--
-- Lua API:
--   pocket.log(msg)
--   pocket.input.keyDown(scancode)   -- SDL scancode
--   pocket.getTransform(entityId) -> {x,y,z}
--   pocket.setTransform(entityId, x, y, z)
--   pocket.spawn(tagName) -> entityId
--   pocket.audio.playSound(id, vol, pitch, loop)
--   pocket.audio.playMusic(id, vol, loop)
--
-- SDL scancodes (sık kullanılanlar):
--   Left  = 80, Right = 79, Up = 82, Down = 81
--   Space = 44, Enter = 40, Esc   = 41

local SPEED = 5.0  -- birim/saniye

function on_start()
    pocket.log("Player script started")
end

function on_update(dt)
    -- entity_id globali ScriptEngine tarafından set edilir
    local t = pocket.getTransform(entity_id)
    if not t then return end

    -- Hareket (ok tuşları)
    if pocket.input.keyDown(80) then t.x = t.x - SPEED * dt end  -- Left
    if pocket.input.keyDown(79) then t.x = t.x + SPEED * dt end  -- Right
    if pocket.input.keyDown(82) then t.y = t.y + SPEED * dt end  -- Up
    if pocket.input.keyDown(81) then t.y = t.y - SPEED * dt end  -- Down

    -- Yeni transformu geri yaz
    pocket.setTransform(entity_id, t.x, t.y, t.z)
end
