// GMod's sandbox tools: names, categories, help (resource/localization/en/tool.properties) and
// settings (each stool's TOOL.ClientConVar, same names and defaults: <tool>_<setting>).
//
// Setting kinds: 'n' number (min..max), 'b' checkbox, 'k' numpad key (ButtonCode_t, KEY_PAD_0 = 37),
// 's' one of choices ("value=Label|value|..."; a value without a label shows its file name),
// 't' free text (only set from the console).

struct GMPToolVar
{
	const char *name, *def;
	char kind;
	float min, max;
	const char *label, *choices;
};

struct GMPToolDef
{
	const char *mode, *name, *category;
	const GMPToolVar *vars;
	int numVars;
	// Help for stage 0 / 1 / 2 (left), stage 0 (right), reload; NULL: not used.
	const char *left, *left1, *left2, *right, *reload;
};

#define GMP_COLOR_VARS( r, g, b ) \
	{ r, "255", 'n', 0, 255, "Red", NULL }, { g, "255", 'n', 0, 255, "Green", NULL }, { b, "255", 'n', 0, 255, "Blue", NULL }

static const char s_GMPRopeMaterials[] = "cable/rope=Rope|cable/cable2=Cable|cable/cable=Thin cable|cable/xbeam=Beam|cable/redlaser=Laser|cable/blue_elec=Electric|cable/physbeam=Phys beam|cable/hydra=Hydra|cable/chain=Chain";

static const GMPToolVar s_GMPAxisVars[] = {
	{ "forcelimit", "0", 'n', 0, 50000, "Force Limit", NULL }, { "torquelimit", "0", 'n', 0, 50000, "Torque Limit", NULL },
	{ "hingefriction", "0", 'n', 0, 200, "Friction", NULL }, { "nocollide", "0", 'b', 0, 1, "No Collide", NULL } };
static const GMPToolVar s_GMPBallsocketVars[] = {
	{ "forcelimit", "0", 'n', 0, 50000, "Force Limit", NULL }, { "torquelimit", "0", 'n', 0, 50000, "Torque Limit", NULL },
	{ "nocollide", "0", 'b', 0, 1, "No Collide", NULL } };
static const GMPToolVar s_GMPElasticVars[] = {
	{ "constant", "500", 'n', 0, 4000, "Constant", NULL }, { "damping", "3", 'n', 0, 50, "Damping", NULL },
	{ "rdamping", "0.01", 'n', 0, 1, "Relative Damping", NULL }, { "material", "cable/cable", 's', 0, 0, "Material", s_GMPRopeMaterials },
	{ "width", "2", 'n', 0, 20, "Width", NULL }, { "stretch_only", "1", 'b', 0, 1, "Stretch Only", NULL },
	GMP_COLOR_VARS( "color_r", "color_g", "color_b" ) };
static const GMPToolVar s_GMPHydraulicVars[] = {
	{ "group", "37", 'k', 0, 0, "Key", NULL }, { "toggle", "1", 'b', 0, 1, "Toggle", NULL },
	{ "addlength", "100", 'n', -1000, 1000, "Length Change", NULL }, { "speed", "64", 'n', 0, 512, "Speed", NULL },
	{ "fixed", "1", 'b', 0, 1, "Fixed (slider)", NULL }, { "width", "3", 'n', 0, 20, "Width", NULL },
	{ "material", "cable/rope", 's', 0, 0, "Material", s_GMPRopeMaterials }, GMP_COLOR_VARS( "color_r", "color_g", "color_b" ) };
static const GMPToolVar s_GMPMotorVars[] = {
	{ "fwd", "45", 'k', 0, 0, "Forward Key", NULL }, { "bwd", "42", 'k', 0, 0, "Reverse Key", NULL },
	{ "torque", "500", 'n', 0, 10000, "Torque", NULL }, { "forcelimit", "0", 'n', 0, 50000, "Force Limit", NULL },
	{ "friction", "1", 'n', 0, 100, "Hinge Friction", NULL }, { "nocollide", "1", 'b', 0, 1, "No Collide", NULL },
	{ "toggle", "0", 'b', 0, 1, "Toggle", NULL }, { "forcetime", "0", 'n', 0, 120, "Force Time", NULL } };
static const GMPToolVar s_GMPMuscleVars[] = {
	{ "group", "37", 'k', 0, 0, "Key", NULL }, { "addlength", "100", 'n', -1000, 1000, "Length Change", NULL },
	{ "period", "1", 'n', 0.1f, 20, "Period", NULL }, { "starton", "0", 'b', 0, 1, "Start On", NULL },
	{ "fixed", "1", 'b', 0, 1, "Fixed (slider)", NULL }, { "width", "2", 'n', 0, 20, "Width", NULL },
	{ "material", "cable/rope", 's', 0, 0, "Material", s_GMPRopeMaterials }, GMP_COLOR_VARS( "color_r", "color_g", "color_b" ) };
static const GMPToolVar s_GMPPulleyVars[] = {
	{ "forcelimit", "0", 'n', 0, 50000, "Force Limit", NULL }, { "rigid", "0", 'b', 0, 1, "Rigid", NULL },
	{ "width", "3", 'n', 0, 20, "Width", NULL }, { "material", "cable/cable", 's', 0, 0, "Material", s_GMPRopeMaterials },
	GMP_COLOR_VARS( "color_r", "color_g", "color_b" ) };
static const GMPToolVar s_GMPRopeVars[] = {
	{ "forcelimit", "0", 'n', 0, 50000, "Force Limit", NULL }, { "addlength", "0", 'n', -500, 500, "Add Length", NULL },
	{ "rigid", "0", 'b', 0, 1, "Rigid", NULL }, { "width", "2", 'n', 0, 20, "Width", NULL },
	{ "material", "cable/rope", 's', 0, 0, "Material", s_GMPRopeMaterials }, GMP_COLOR_VARS( "color_r", "color_g", "color_b" ) };
static const GMPToolVar s_GMPSliderVars[] = {
	{ "width", "1.5", 'n', 0, 20, "Width", NULL }, { "material", "cable/cable", 's', 0, 0, "Material", s_GMPRopeMaterials },
	GMP_COLOR_VARS( "color_r", "color_g", "color_b" ) };
static const GMPToolVar s_GMPWeldVars[] = {
	{ "forcelimit", "0", 'n', 0, 50000, "Force Limit", NULL }, { "nocollide", "0", 'b', 0, 1, "No Collide", NULL } };
static const GMPToolVar s_GMPWinchVars[] = {
	{ "fwd_group", "44", 'k', 0, 0, "Forward Key", NULL }, { "bwd_group", "41", 'k', 0, 0, "Reverse Key", NULL },
	{ "fwd_speed", "64", 'n', 0, 1000, "Forward Speed", NULL }, { "bwd_speed", "64", 'n', 0, 1000, "Reverse Speed", NULL },
	{ "rope_width", "3", 'n', 0, 20, "Width", NULL }, { "rope_material", "cable/rope", 's', 0, 0, "Material", s_GMPRopeMaterials },
	GMP_COLOR_VARS( "color_r", "color_g", "color_b" ) };

static const GMPToolVar s_GMPBalloonVars[] = {
	{ "force", "500", 'n', -1000, 2000, "Force", NULL }, { "ropelength", "64", 'n', 0, 1000, "Rope Length", NULL },
	{ "model", "normal_skin1", 's', 0, 0, "Balloon", "normal=Balloon|normal_skin1=Balloon (Love)|normal_skin2=Balloon (Smile)|normal_skin3=Balloon (Bang)|gman=G-Man|mossman=Mossman|dog=Dog|heart=Heart|star=Star" },
	GMP_COLOR_VARS( "r", "g", "b" ) };
static const GMPToolVar s_GMPButtonVars[] = {
	{ "keygroup", "37", 'k', 0, 0, "Key", NULL }, { "toggle", "1", 'b', 0, 1, "Toggle", NULL },
	{ "model", "models/maxofs2d/button_05.mdl", 's', 0, 0, "Model", "models/maxofs2d/button_01.mdl|models/maxofs2d/button_02.mdl|models/maxofs2d/button_03.mdl|models/maxofs2d/button_04.mdl|models/maxofs2d/button_05.mdl|models/maxofs2d/button_06.mdl|models/maxofs2d/button_slider.mdl|models/dav0r/buttons/button.mdl|models/dav0r/buttons/switch.mdl" },
	{ "description", "", 't', 0, 0, "Description", NULL } };
static const GMPToolVar s_GMPDynamiteVars[] = {
	{ "group", "52", 'k', 0, 0, "Key", NULL }, { "damage", "200", 'n', 0, 500, "Damage", NULL },
	{ "delay", "0", 'n', 0, 10, "Delay", NULL }, { "remove", "0", 'b', 0, 1, "Remove after", NULL },
	{ "model", "models/dav0r/tnt/tnt.mdl", 's', 0, 0, "Model", "models/dav0r/tnt/tnt.mdl|models/dav0r/tnt/tnttimed.mdl|models/props_phx/oildrum001_explosive.mdl|models/props_c17/oildrum001_explosive.mdl|models/props_junk/propane_tank001a.mdl" } };
static const GMPToolVar s_GMPEmitterVars[] = {
	{ "key", "51", 'k', 0, 0, "Key", NULL }, { "delay", "1", 'n', 0.05f, 20, "Delay", NULL },
	{ "scale", "1", 'n', 0, 6, "Scale", NULL }, { "toggle", "1", 'b', 0, 1, "Toggle", NULL },
	{ "starton", "0", 'b', 0, 1, "Start On", NULL },
	{ "effect", "sparks", 's', 0, 0, "Effect", "sparks=Sparks|stunstickimpact=Stunstick|manhacksparks=Manhack Sparks|bloodspray=Blood Spray|striderblood=Strider Blood|shells=Shells|rifleshells=Rifle Shells|shotgunshells=Shotgun Shells|cball_explode=Combine Ball Explode|cball_bounce=Combine Ball Bounce|thumperdust=Thumper Dust|ar2impact=AR2 Impact|ar2explosion=AR2 Explosion|explosion=Explosion|helicoptermegabomb=Helicopter Bomb|underwaterexplosion=Underwater Explosion|watersplash=Water Splash|dirtywatersplash=Dirty Water Splash|glassimpact=Glass Impact|bloodimpact=Blood Impact|smoke=Smoke" } };
static const GMPToolVar s_GMPHoverballVars[] = {
	{ "keyup", "46", 'k', 0, 0, "Up Key", NULL }, { "keydn", "43", 'k', 0, 0, "Down Key", NULL },
	{ "keyon", "40", 'k', 0, 0, "On/Off Key", NULL }, { "speed", "5", 'n', 0, 20, "Movement Speed", NULL },
	{ "resistance", "5", 'n', 0, 10, "Air Resistance", NULL }, { "strength", "10", 'n', 0.1f, 20, "Strength", NULL },
	{ "model", "models/dav0r/hoverball.mdl", 's', 0, 0, "Model", "models/dav0r/hoverball.mdl|models/maxofs2d/hover_basic.mdl|models/maxofs2d/hover_classic.mdl|models/maxofs2d/hover_plate.mdl|models/maxofs2d/hover_propeller.mdl|models/maxofs2d/hover_rings.mdl" } };
static const GMPToolVar s_GMPLampVars[] = {
	{ "key", "37", 'k', 0, 0, "Key", NULL }, { "toggle", "1", 'b', 0, 1, "Toggle", NULL },
	{ "fov", "90", 'n', 10, 170, "FOV", NULL }, { "distance", "1024", 'n', 64, 2048, "Distance", NULL },
	{ "brightness", "4", 'n', 0, 8, "Brightness", NULL },
	{ "texture", "effects/flashlight001", 's', 0, 0, "Texture", "effects/flashlight001=Default|effects/flashlight/slit=Slit|effects/flashlight/circles=Circles|effects/flashlight/window=Window|effects/flashlight/logo=Logo|effects/flashlight/gradient=Gradient|effects/flashlight/bars=Bars|effects/flashlight/tech=Tech Demo|effects/flashlight/soft=Soft|effects/flashlight/hard=Hard|effects/flashlight/caustics=Caustics|effects/flashlight/square=Square|effects/flashlight/camera=Camera|effects/flashlight/view=View" },
	{ "model", "models/lamps/torch.mdl", 's', 0, 0, "Model", "models/lamps/torch.mdl|models/maxofs2d/lamp_flashlight.mdl|models/maxofs2d/lamp_projector.mdl|models/props_wasteland/light_spotlight01_lamp.mdl|models/props_wasteland/light_spotlight02_lamp.mdl|models/props_c17/light_decklight01_off.mdl" },
	GMP_COLOR_VARS( "r", "g", "b" ) };
static const GMPToolVar s_GMPLightVars[] = {
	{ "key", "37", 'k', 0, 0, "Key", NULL }, { "toggle", "1", 'b', 0, 1, "Toggle", NULL },
	{ "ropelength", "64", 'n', 0, 256, "Rope Length", NULL }, { "brightness", "2", 'n', 0, 10, "Brightness", NULL },
	{ "size", "256", 'n', 0, 1024, "Radius", NULL }, { "ropematerial", "cable/rope", 's', 0, 0, "Rope", s_GMPRopeMaterials },
	GMP_COLOR_VARS( "r", "g", "b" ) };
static const GMPToolVar s_GMPPhyspropVars[] = {
	{ "material", "metal_bouncy", 's', 0, 0, "Material", "metal_bouncy=Bouncy Metal|metal=Metal|dirt=Dirt|slipperyslime=Slime|wood=Wood|glass=Glass|concrete_block=Concrete|ice=Ice|rubber=Rubber|paper=Paper|zombieflesh=Flesh|gmod_ice=Super Ice|gmod_bouncy=Super Bouncy" },
	{ "gravity_toggle", "1", 'b', 0, 1, "Gravity", NULL } };
static const GMPToolVar s_GMPThrusterVars[] = {
	{ "keygroup", "45", 'k', 0, 0, "Forward Key", NULL }, { "keygroup_back", "42", 'k', 0, 0, "Reverse Key", NULL },
	{ "force", "1500", 'n', 1, 10000, "Force", NULL }, { "toggle", "0", 'b', 0, 1, "Toggle", NULL },
	{ "collision", "0", 'b', 0, 1, "No Collide", NULL }, { "damageable", "0", 'b', 0, 1, "Damageable", NULL },
	{ "effect", "fire", 's', 0, 0, "Effect", "none=None|fire=Flames|smoke=Smoke|sparks=Sparks|plasma=Plasma" },
	{ "soundname", "PhysicsCannister.ThrusterLoop", 's', 0, 0, "Sound", "=None|PhysicsCannister.ThrusterLoop=Steam|WeaponDissolve.Charge=Zap|WeaponDissolve.Beam=Beam|eli_lab.elevator_move=Elevator|combine.sheild_loop=Energy|k_lab.ringsrotating=Rings|k_lab.teleport_rings_high=Resonance|k_lab2.DropshipRotorLoop=Dropship|Town.d1_town_01_spin_loop=Machine" },
	{ "model", "models/props_phx2/garbage_metalcan001a.mdl", 's', 0, 0, "Model", "models/props_phx2/garbage_metalcan001a.mdl|models/dav0r/thruster.mdl|models/maxofs2d/thruster_projector.mdl|models/maxofs2d/thruster_propeller.mdl|models/thrusters/jetpack.mdl|models/props_junk/plasticbucket001a.mdl|models/props_junk/propanecanister001a.mdl|models/props_junk/propane_tank001a.mdl|models/props_junk/popcan01a.mdl|models/props_junk/metalbucket01a.mdl|models/props_lab/jar01a.mdl|models/props_c17/lampshade001a.mdl|models/props_c17/canister01a.mdl|models/props_junk/trafficcone001a.mdl|models/props_c17/clock01.mdl|models/xqm/afterburner1.mdl|models/xqm/afterburner1big.mdl" } };
static const GMPToolVar s_GMPWheelVars[] = {
	{ "fwd", "45", 'k', 0, 0, "Forward Key", NULL }, { "bck", "42", 'k', 0, 0, "Reverse Key", NULL },
	{ "torque", "3000", 'n', 10, 10000, "Torque", NULL }, { "forcelimit", "0", 'n', 0, 50000, "Force Limit", NULL },
	{ "friction", "1", 'n', 0, 100, "Friction", NULL }, { "nocollide", "1", 'b', 0, 1, "No Collide", NULL },
	{ "toggle", "0", 'b', 0, 1, "Toggle", NULL },
	{ "model", "models/props_vehicles/carparts_wheel01a.mdl", 's', 0, 0, "Model", "models/props_vehicles/carparts_wheel01a.mdl|models/props_junk/sawblade001a.mdl|models/props_vehicles/apc_tire001.mdl|models/props_vehicles/tire001a_tractor.mdl|models/props_vehicles/tire001b_truck.mdl|models/props_vehicles/tire001c_car.mdl|models/props_borealis/bluebarrel001.mdl|models/props_c17/oildrum001.mdl|models/props_c17/playground_carousel01.mdl|models/props_c17/chair_office01a.mdl|models/props_c17/trappropeller_blade.mdl|models/props_wasteland/wheel01.mdl|models/props_trainstation/trainstation_clock001.mdl|models/props_junk/metal_paintcan001a.mdl|models/props_c17/pulleywheels_large01.mdl|models/props_phx/normal_tire.mdl|models/props_phx/smallwheel.mdl|models/props_phx/wheels/trucktire.mdl" },
	{ "rx", "90", 'n', -180, 180, "Rotate X", NULL }, { "ry", "0", 'n', -180, 180, "Rotate Y", NULL }, { "rz", "90", 'n', -180, 180, "Rotate Z", NULL } };

static const GMPToolVar s_GMPCameraVars[] = {
	{ "key", "37", 'k', 0, 0, "Key", NULL }, { "toggle", "1", 'b', 0, 1, "Toggle", NULL }, { "locked", "0", 'b', 0, 1, "Locked", NULL } };
static const GMPToolVar s_GMPColourVars[] = {
	GMP_COLOR_VARS( "r", "g", "b" ), { "a", "255", 'n', 0, 255, "Alpha", NULL },
	{ "mode", "0", 's', 0, 0, "Render Mode", "0=Normal|1=Trans Color|2=Trans Texture|3=Glow|4=Trans Alpha|5=Additive|9=World Glow" },
	{ "fx", "0", 's', 0, 0, "Render FX", "0=None|1=Pulse Slow|2=Pulse Fast|3=Pulse Slow Wide|4=Pulse Fast Wide|9=Strobe Slow|10=Strobe Fast|11=Strobe Faster|12=Flicker Slow|13=Flicker Fast|15=Distort|16=Hologram" } };
static const GMPToolVar s_GMPMaterialVars[] = {
	{ "override", "debug/env_cubemap_model", 's', 0, 0, "Material", "models/wireframe|debug/env_cubemap_model|models/shadertest/shader3|models/shadertest/shader4|models/shadertest/shader5|models/shiny|models/debug/debugwhite|models/effects/comball_sphere|models/effects/comball_tape|models/effects/splodearc_sheet|models/effects/vol_light001|models/props_combine/stasisshield_sheet|models/props_combine/portalball001_sheet|models/props_combine/com_shield001a|models/props_c17/frostedglass_01a|models/props_lab/tank_glass001|models/props_combine/tprings_globe|models/rendertarget|models/screenspace|brick/brick_model|models/props_pipes/guttermetal01a|models/props_pipes/pipesystem01a_skin3|models/props_wasteland/wood_fence01a|models/props_foliage/tree_deciduous_01a_trunk|models/props_c17/furniturefabric003a|models/props_c17/furnituremetal001a|models/props_c17/paper01|models/flesh|phoenix_storms/metalset_1-2|phoenix_storms/metalfloor_2-3|phoenix_storms/plastic|phoenix_storms/wood|phoenix_storms/bluemetal|phoenix_storms/cube|phoenix_storms/dome|phoenix_storms/gear|phoenix_storms/stripes|phoenix_storms/wire/pcb_green|phoenix_storms/wire/pcb_red|phoenix_storms/wire/pcb_blue|hunter/myplastic|models/xqm/lightlinesred_tool" } };
static const GMPToolVar s_GMPPaintVars[] = {
	{ "decal", "Blood", 's', 0, 0, "Decal", "Eye|Smile|Light|Dark|Blood|YellowBlood|Impact.Metal|Scorch|BeerSplash|ExplosiveGunshot|BirdPoop|PaintSplatPink|PaintSplatGreen|PaintSplatBlue|ManhackCut|FadingScorch|Antlion.Splat|Splash.Large|BulletProof|GlassBreak|Impact.Sand|Impact.BloodyFlesh|Impact.Antlion|Impact.Glass|Impact.Wood|Impact.Concrete|Noughtsncrosses|Nought|Cross" } };
static const GMPToolVar s_GMPTrailsVars[] = {
	GMP_COLOR_VARS( "r", "g", "b" ), { "a", "255", 'n', 0, 255, "Alpha", NULL },
	{ "length", "5", 'n', 0, 10, "Length", NULL }, { "startsize", "32", 'n', 0, 128, "Start Size", NULL },
	{ "endsize", "0", 'n', 0, 128, "End Size", NULL },
	{ "material", "trails/lol", 's', 0, 0, "Material", "trails/plasma=Plasma|trails/tube=Tube|trails/electric=Electric|trails/smoke=Smoke|trails/laser=Laser|trails/physbeam=Phys Beam|trails/love=Love|trails/lol=LOL" } };

#define GMP_TOOL( mode, name, cat, vars, l, l1, l2, r, rl ) { mode, name, cat, vars, ARRAYSIZE( vars ), l, l1, l2, r, rl }
#define GMP_TOOL_NOVARS( mode, name, cat, l, l1, l2, r, rl ) { mode, name, cat, NULL, 0, l, l1, l2, r, rl }

static const GMPToolDef s_GMPToolDefs[] =
{
	GMP_TOOL( "axis", "Axis", "Constraints", s_GMPAxisVars, "Select an object to begin an auto aligning Axis constraint", "Attach the object with an Axis constraint", NULL, "Select an object to begin an Axis constraint", "Clear all Axis constraints" ),
	GMP_TOOL( "ballsocket", "Ball Socket", "Constraints", s_GMPBallsocketVars, "Select an object to Ball Socket", "Select an object to Ball Socket to", NULL, NULL, "Clear all Ball Socket constraints" ),
	GMP_TOOL( "elastic", "Elastic", "Constraints", s_GMPElasticVars, "Select an object to begin an Elastic constraint", "Select an object to finish the Elastic constraint", NULL, NULL, "Clear all Elastic constraints" ),
	GMP_TOOL( "hydraulic", "Hydraulic", "Constraints", s_GMPHydraulicVars, "Select an object to begin a Hydraulic constraint", "Select an object to finish the Hydraulic constraint", NULL, "Create a Hydraulic constraint perpendicular to the hit surface", "Clear all Hydraulic constraints" ),
	GMP_TOOL( "motor", "Motor", "Constraints", s_GMPMotorVars, "Select an object to begin a Motor constraint", "Select an object to finish the Motor constraint", NULL, NULL, "Clear all Motor constraints" ),
	GMP_TOOL( "muscle", "Muscle", "Constraints", s_GMPMuscleVars, "Select an object to begin a Muscle constraint", "Select an object to finish the Muscle constraint", NULL, "Create a Muscle constraint perpendicular to the hit surface", "Clear all Muscle constraints" ),
	GMP_TOOL( "pulley", "Pulley", "Constraints", s_GMPPulleyVars, "Select an object to begin a Pulley constraint", "Select a world position for the first anchor point", "Select a world position for the second anchor point", NULL, "Clear all Pulley constraints" ),
	GMP_TOOL( "rope", "Rope", "Constraints", s_GMPRopeVars, "Click anywhere to start a Rope constraint", "Click anywhere to finish the Rope constraint", NULL, "Add a new segment to the Rope", "Clear all Rope constraints" ),
	GMP_TOOL( "slider", "Slider", "Constraints", s_GMPSliderVars, "Select an object to begin a Slider constraint", "Select an object to finish the Slider constraint", NULL, "Create a Slider constraint perpendicular to the hit surface", "Clear all Slider constraints" ),
	GMP_TOOL( "weld", "Weld", "Constraints", s_GMPWeldVars, "Select an object to weld", "Select an object to weld to", NULL, "Select an object to begin an Easy Weld constraint", "Clear all Weld constraints" ),
	GMP_TOOL( "winch", "Winch", "Constraints", s_GMPWinchVars, "Select an object to begin a Winch constraint", "Select an object to finish the Winch constraint", NULL, "Create a Winch constraint perpendicular to the hit surface", "Clear all Winch constraints" ),

	GMP_TOOL( "balloon", "Balloons", "Construction", s_GMPBalloonVars, "Attach or update a Balloon", NULL, NULL, "Spawn a Balloon without a rope", NULL ),
	GMP_TOOL( "button", "Button", "Construction", s_GMPButtonVars, "Create or update a Button", NULL, NULL, "Spawn a Button without welding", NULL ),
	GMP_TOOL_NOVARS( "duplicator", "Duplicator", "Construction", "Paste duplication", NULL, NULL, "Copy contraption", NULL ),
	GMP_TOOL( "dynamite", "Dynamite", "Construction", s_GMPDynamiteVars, "Create or update Dynamite", NULL, NULL, NULL, NULL ),
	GMP_TOOL( "emitter", "Emitter", "Construction", s_GMPEmitterVars, "Create or update an Emitter", NULL, NULL, "Attach an Emitter", NULL ),
	GMP_TOOL( "hoverball", "Hoverball", "Construction", s_GMPHoverballVars, "Create or update a Hoverball", NULL, NULL, NULL, NULL ),
	GMP_TOOL( "lamp", "Lamps", "Construction", s_GMPLampVars, "Create or update a Lamp", NULL, NULL, "Copy settings of a Lamp", NULL ),
	GMP_TOOL( "light", "Light", "Construction", s_GMPLightVars, "Create or update a Light", NULL, NULL, "Spawn a Light without a rope", NULL ),
	GMP_TOOL_NOVARS( "nocollide", "No Collide", "Construction", "Select an object", "Select an object to No Collide it with", NULL, "Make an object have no collisions with anything but the world", "Clear all No Collide constraints" ),
	GMP_TOOL( "physprop", "Physical Properties", "Construction", s_GMPPhyspropVars, "Apply selected Physical Properties to an object", NULL, NULL, NULL, NULL ),
	GMP_TOOL_NOVARS( "remover", "Remover", "Construction", "Remove an object", NULL, NULL, "Remove selected object and all objects constrained to it", "Clear all constraints on selected object" ),
	GMP_TOOL( "thruster", "Thruster", "Construction", s_GMPThrusterVars, "Create or update a Thruster", NULL, NULL, NULL, NULL ),
	GMP_TOOL( "wheel", "Wheel", "Construction", s_GMPWheelVars, "Create or update a Wheel", NULL, NULL, "Update a Wheel with new settings", NULL ),

	GMP_TOOL( "camera", "Camera", "Render", s_GMPCameraVars, "Place a Camera", NULL, NULL, "Place a locked Camera", NULL ),
	GMP_TOOL( "colour", "Color", "Render", s_GMPColourVars, "Apply color to an object", NULL, NULL, "Copy the color of an object", "Reset the color of an object" ),
	GMP_TOOL( "material", "Material", "Render", s_GMPMaterialVars, "Apply a material to an object", NULL, NULL, "Copy the material of an object", "Reset the material of an object" ),
	GMP_TOOL( "paint", "Paint", "Render", s_GMPPaintVars, "Paint decals with a fast fire rate", NULL, NULL, "Paint decals with a slow fire rate", "Clear decals from an object" ),
	GMP_TOOL( "trails", "Trails", "Render", s_GMPTrailsVars, "Apply a trail to an object", NULL, NULL, "Clear the trail from an object", NULL ),
};

static int GMP_ToolIndex( const char *mode )
{
	for ( int i = 0; mode && i < ARRAYSIZE( s_GMPToolDefs ); i++ )
		if ( !Q_stricmp( mode, s_GMPToolDefs[i].mode ) )
			return i;
	return -1;
}

static int GMP_ToolVarIndex( const GMPToolDef &tool, const char *name )
{
	for ( int i = 0; i < tool.numVars; i++ )
		if ( !Q_stricmp( tool.vars[i].name, name ) )
			return i;
	return -1;
}

// Numpad keys as GMod names them on the binder buttons.
static const char *GMP_KeyName( int key )
{
	static const char *const s_Names[] = { "KP 0", "KP 1", "KP 2", "KP 3", "KP 4", "KP 5", "KP 6", "KP 7", "KP 8", "KP 9", "KP /", "KP *", "KP -", "KP +", "KP Enter", "KP ." };
	return ( key >= 37 && key <= 52 ) ? s_Names[key - 37] : "None";
}

// The label of choice value 'value' in "value=Label|value|...", or NULL; *pIndex its position.
static bool GMP_ChoiceAt( const char *choices, int index, char *value, int valueSize, char *label, int labelSize )
{
	const char *p = choices;
	for ( int i = 0; p && *p; i++ )
	{
		const char *end = strchr( p, '|' );
		const int len = end ? (int)( end - p ) : (int)strlen( p );
		if ( i == index )
		{
			const char *eq = (const char *)memchr( p, '=', len );
			const int vlen = eq ? (int)( eq - p ) : len;
			Q_strncpy( value, p, MIN( valueSize, vlen + 1 ) );
			if ( eq )
				Q_strncpy( label, eq + 1, MIN( labelSize, len - vlen ) );
			else
				Q_FileBase( value, label, labelSize );
			return true;
		}
		p = end ? end + 1 : NULL;
	}
	return false;
}

static int GMP_ChoiceFind( const char *choices, const char *value )
{
	char v[MAX_PATH], l[64];
	for ( int i = 0; GMP_ChoiceAt( choices, i, v, sizeof( v ), l, sizeof( l ) ); i++ )
		if ( !Q_stricmp( v, value ) )
			return i;
	return -1;
}
