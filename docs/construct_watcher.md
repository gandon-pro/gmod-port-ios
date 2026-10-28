# Construct watcher

`engine_src/game/shared/hl2mp/gmp_construct_watcher.inl`, included from gmp_physgun.inl
(server side only). Only a listen server on gm_construct with exactly one connected
player is eligible (hl2mp runs maxplayers 2, so maxClients is not checked).

## Watcher
`gmp_construct_watcher_delay` seconds (60) after the player is alive on gm_construct,
spawn `models/Humans/Group01/male_07.mdl` as npc_citizen on a rooftop the player looks
at: black render colour, a dim light at its feet (EF_DIMLIGHT), AI off, not solid,
invulnerable, idle, always facing the player. If no spot is found it retries every 0.5 s.

Roof search (`FindSpot`, 64 tries):
- the player must be under open sky (the up trace hits sky or nothing; gm_construct has no
  brush above the open map);
- a column 1452–2612 units out along the view yaw offset by 6–35 degrees is traced down
  from the sky height to the topmost surface: flat (normal z >= 0.9), not sky/nodraw;
- at least 128 units above the player's feet and at a roof edge (`AtEdge`: some point 96
  units away drops more than ~100 units), so hill slopes do not count;
- clear 32x32x72 hull, no solid at its centre, line of sight, in the view cone.

`GMP_WATCHER_NEAR` = 1312 units = 25 m (1 unit = 1.905 cm). Closer than that (2D) and it
vanishes (EF_NODRAW) for 4–10 s; so does keeping the crosshair on it for 5 s (the aim
window is the body plus ~4 degrees). It then turns up on another roof: a spot on the same
flat roof (`SameRoof`: height within 24 units and floor under 7 points between them) is
rejected. Every 15–30 s, while the player is not looking at it, it moves to another roof
the same way. Death, cleanup or removal restarts the delay.

## Chase and crash
`gmp_construct_watcher_chase` seconds (300) after the spawn, it plays ACT_RUN and moves
at the player with SetAbsOrigin, through walls, at max(500, 1.3 x player speed) + 40/s
u/s, so always faster than the player, sprint included. While the player is more than 40
units above it and a wall (or solid) is in front, it climbs straight up, then runs on over
the top: it follows onto roofs. Off a roof edge it drops at 900 u/s (floor trace 4096
down). During the chase the player's noclip is switched off every frame (console, bind and
touch button alike). It screams `npc/stalker/stalker_scream1.wav` (2.16 s) back to back.
That wave is episode content: the phone gets it from gmcontent/npcpack. 0.4 s after it
gets within 48 units, the server calls `std::abort()`. That kills the whole
listen-server process (game/app) without saving. `gmp_construct_watcher_crash 0`
removes the NPC instead. One encounter per map load.

## Convars
`gmp_construct_watcher` (1, archived; also Options > Debug), `_delay` (60), `_chase`
(300), `_crash` (1). Debug output at `developer 2` (Options > Debug > developer messages).

## Verified (Windows dev build)
Rooftop spawn found ~2580 units out (yaw 225 from the start area); the approach vanish
fired. Earlier floor-spawn build: crosshair 5 s → vanished → back elsewhere, chase on the
floor, scream, caught; crash path ends the process. Not yet observed: climbing onto a
roof, noclip block, the same-roof rejection. Not run on iOS yet.
