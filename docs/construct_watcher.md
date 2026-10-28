# Construct watcher

`engine_src/game/shared/hl2mp/gmp_construct_watcher.inl`, included from gmp_physgun.inl
(server side only). Only a listen server on gm_construct with exactly one connected
player is eligible (hl2mp runs maxplayers 2, so maxClients is not checked).

## Watcher
`gmp_construct_watcher_delay` seconds (60) after the player is alive on gm_construct,
spawn `models/Humans/Group01/male_07.mdl` as npc_citizen anywhere on the map, wherever
the player looks: black render colour, a dim light at its feet (EF_DIMLIGHT), AI off,
not solid, invulnerable, idle, always facing the player. If no spot is found it retries
every 0.5 s. The spot is traced 420–900 units along the view yaw offset by 10–32 degrees
(in view, but not under the crosshair), 350–850 units away in 2D, on a floor (normal z
≥ 0.7), with a clear 32x32x72 hull, no solid at its centre and line of sight.
Every 15–30 s, while the player is not looking at it, it moves to a new such spot.
Keep the crosshair on it for 5 s and it vanishes (EF_NODRAW) for 4–10 s, then turns up
at another spot at least 400 units from the old one.
Death, cleanup or removal restarts the delay.

## Chase and crash
`gmp_construct_watcher_chase` seconds (300) after the spawn, it plays ACT_RUN and moves
straight at the player with SetAbsOrigin (through walls, feet kept on the floor below
it), at 420 + 40/s u/s, screaming `npc/stalker/stalker_scream1.wav` (2.16 s) back to back.
That wave is episode content: the phone gets it from gmcontent/npcpack. 0.4 s after it
gets within 48 units, the server calls `std::abort()`. That kills the whole
listen-server process (game/app) without saving. `gmp_construct_watcher_crash 0`
removes the NPC instead. One encounter per map load.

## Convars
`gmp_construct_watcher` (1, archived; also Options > Debug), `_delay` (60), `_chase`
(300), `_crash` (1). Debug output at `developer 2`.

## Verified (Windows dev build)
On the open map: spawn at ~420 units in view, crosshair 5 s → vanished → back at another
spot, chase running on the floor, scream played, caught. Earlier: crash path ends the
process. Not run on iOS yet.
