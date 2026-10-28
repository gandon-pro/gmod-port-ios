# Construct watcher

Included from gmp_physgun.inl on client and server. Enabled by default through
`gmp_construct_watcher 1`; use `gmp_construct_watcher 0` to disable it.
Only a single-player listen server on gm_construct is eligible.

The client samples baked map lighting twice per second. All three RGB channels
must be below 0.06. The server also requires a solid non-sky ceiling overhead.
This identifies dark indoor areas rather than a hardcoded room coordinate box;
other sufficiently dark indoor spots on gm_construct also qualify.
Leaving darkness, missing samples for 1.5 seconds, death, disabling the option,
and changing maps reset the pre-spawn timer. Movement within the room is allowed.

After more than 60 uninterrupted seconds, arm the encounter. It stays armed
when leaving the dark room: the player needs to be able to see outside roofs.
Every two seconds, try up to 24 positions 1200–3200 horizontal units away,
within +/-38 degrees of view yaw. Require an exterior horizontal brush top at
least 128 units above the player, open sky above, an empty room under its slab,
a clear human hull and a visible head within the full 3D view cone.
This is geometric roof detection, not an authored list of gm_construct roofs;
unusual solid/thick roofs may be rejected. No near-floor fallback exists.

Spawn models/Humans/Group01/male_07.mdl as npc_citizen, black render colour,
invulnerable, stationary, continuously facing the player. Every 20–40 seconds,
when the player cannot see it, relocate to another qualifying visible rooftop
at least 512 units from the last position. Never relocate under direct gaze.
Relocation does not reset the five-minute timer. If no roof qualifies, retain
the old position. This is discrete relocation, not roof-to-roof walking.

300 seconds after actual spawn, release the NPC and use native CHASE_ENEMY AI,
with ambient/voices/citizen_beaten3.wav. Navigation needs the map's AI graph.
One second after it reaches 80 units from the player, send the local ending
command. The client deliberately calls std::abort(): this really terminates the
process, can produce an OS crash report, and does not save the current session.
There is no timeout crash if the NPC cannot reach the player.
The ending command only acts in a sampled single-player gm_construct session.

Assets must be included in the phone's content package (civilian model and all
its materials/animations plus the scream WAV). No new custom model is needed.

Validation in this edit: API declarations checked against the pinned Source
headers, inclusion through the existing engine overlay confirmed, git diff
whitespace check. Not compiled for iOS or played on device yet. No IPA produced.
