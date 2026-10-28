# Construct watcher

`engine_src/game/shared/hl2mp/gmp_construct_watcher.inl`, included from gmp_physgun.inl
on client and server. Only a listen server on gm_construct with exactly one connected
player is eligible (hl2mp runs maxplayers 2, so maxClients is not checked).

## Dark detection
The client (`Update`) samples `engine->GetLightForPoint` at the eye twice per second;
dark = all channels < 0.06. It sends `gmp_watcher_sample 0/1` to the server, which
also requires a non-sky ceiling above the player. Any dark indoor spot qualifies
(the dark room near -4442 -1852 -136 measures 0.000), not a hardcoded box.
Leaving the dark, or no samples, resets the timer.

## Watcher
After `gmp_construct_watcher_dark` seconds (60) of darkness, spawn
`models/Humans/Group01/male_07.mdl` as npc_citizen: black render colour, AI off,
not solid, invulnerable, idle, always facing the player. The spot is traced 250–1000
units along the view yaw offset by 10–32 degrees (in view, but not under the
crosshair), at least 160 units away, on a floor, with a clear hull and line of sight.
Every 15–30 s, while the player is not looking at it, it moves to a new such spot.

## Chase and crash
`gmp_construct_watcher_chase` seconds (300) after the spawn, it plays ACT_RUN and moves
straight at the player with SetAbsOrigin (through walls), at 420 + 40/s u/s, screaming
`ambient/voices/citizen_beaten3.wav` every 2.5 s. 0.4 s after it gets within 48 units,
the server calls `std::abort()`. That kills the whole listen-server process (game/app)
without saving. `gmp_construct_watcher_crash 0` removes the NPC instead.
One encounter per map load.

## Convars
`gmp_construct_watcher` (1, archived), `_dark` (60), `_chase` (300), `_crash` (1),
`_force` (cheat, treat any indoor spot as dark). Debug output at `developer 2`.

## Verified (Windows dev build)
Full cycle: dark room → spawn → chase → caught → process gone. A screenshot with
mat_fullbright showed the black citizen standing, then running. Not run on iOS yet.
