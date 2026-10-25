-- Build-time extraction of Facepunch's NPC metadata; no gameplay is emulated.
SERVER=true
CT_DOWNTRODDEN=1; CT_REFUGEE=2; CT_REBEL=3; CT_UNIQUE=4
SF_CITIZEN_MEDIC=131072; SF_CITIZEN_RANDOM_HEAD=262144
SF_NPC_DROP_HEALTHKIT=8; SF_NPC_NO_PLAYER_PUSHAWAY=16384
SF_FLOOR_TURRET_CITIZEN=512
bit=require('bit')
function Angle(p,y,r) return {pitch=p,yaw=y,roll=r} end
-- Only the built-in HL2 + Episodes roster; mounted-game NPCs are excluded.
function IsMounted(name) return false end
local function json(v)
    local t=type(v)
    if t=='string' then return '"'..v:gsub('\\','\\\\'):gsub('"','\\"'):gsub('\n','\\n'):gsub('\r','\\r')..'"' end
    if t=='boolean' or t=='number' then return tostring(v) end
    if t~='table' then return 'null' end
    local parts={}
    if #v>0 then
        for i=1,#v do parts[#parts+1]=json(v[i]) end
        return '['..table.concat(parts,',')..']'
    end
    local keys={}
    for k in pairs(v) do keys[#keys+1]=k end
    table.sort(keys)
    for _,k in ipairs(keys) do parts[#parts+1]=json(k)..':'..json(v[k]) end
    return '{'..table.concat(parts,',')..'}'
end
list={Set=function(kind,id,data)
    assert(kind=='NPC')
    data.id=id
    io.write(json(data),'\n')
end}
assert(loadfile(arg[1]))()
