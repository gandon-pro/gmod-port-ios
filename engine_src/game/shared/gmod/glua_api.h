// GModPort GLua layer: the contract between the parts written by Aero (runtime, entities, gamemode,
// vgui) and by GPT (value types, engine-independent libraries). Personal use only.
//
// Rules:
//  - Every native function is a plain lua_CFunction registered through a GLuaReg table below;
//    nothing calls lua_register / lua_setglobal for natives directly.
//  - Value types (Vector, Angle, VMatrix) are full userdata holding the engine type by value,
//    with the metatable stored in the registry under its GMod type name ("Vector", ...).
//  - Errors use luaL_argerror / luaL_error so messages match GMod's "bad argument #n to 'f'".
#pragma once

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

class Vector;
class QAngle;
class VMatrix;

namespace glua
{
	// Which Lua states a function exists in (GMod has separate client, server and menu states).
	enum Realm
	{
		REALM_CLIENT = 1 << 0,
		REALM_SERVER = 1 << 1,
		REALM_MENU   = 1 << 2,
		REALM_SHARED = REALM_CLIENT | REALM_SERVER,
		REALM_ALL    = REALM_SHARED | REALM_MENU,
	};

	// One native. lib: NULL for a global ("CurTime"), "util" for util.X, or a metatable name
	// prefixed with ':' (":Vector") for a method.
	struct Reg
	{
		const char    *lib;
		const char    *name;
		lua_CFunction  fn;
		int            realms;
	};

	// Tables register themselves at static-init time; Install() pushes every entry that matches
	// the state's realm. A NULL-terminated array per source file:
	//   static const glua::Reg s_Util[] = { { "util", "CRC", util_CRC, glua::REALM_ALL }, { 0 } };
	//   GLUA_REGISTER( s_Util );
	struct RegList
	{
		RegList( const Reg *regs );
		const Reg *m_pRegs;
		RegList   *m_pNext;
	};
	#define GLUA_REGISTER( table ) static glua::RegList s_GLuaRegList_##table( table )

	// Creates libraries and metatables as needed and sets every registered function for 'realm'.
	void Install( lua_State *L, Realm realm );
	Realm GetRealm( lua_State *L );

	// Value types. Check* raise a Lua argument error on a wrong type; Push* copy the value.
	Vector  *CheckVector( lua_State *L, int idx );
	QAngle  *CheckAngle( lua_State *L, int idx );
	VMatrix *CheckMatrix( lua_State *L, int idx );
	Vector  *ToVector( lua_State *L, int idx );		// NULL if not a Vector
	QAngle  *ToAngle( lua_State *L, int idx );
	VMatrix *ToMatrix( lua_State *L, int idx );
	void PushVector( lua_State *L, const Vector &v );
	void PushAngle( lua_State *L, const QAngle &a );
	void PushMatrix( lua_State *L, const VMatrix &m );

	// Engine objects (Entity, Player, Panel, IMaterial, ...) are Aero's. GPT-side code that needs
	// one takes it through these hooks and must not assume their layout.
	typedef void ( *PushObjectFn )( lua_State *L, void *pObject );
	void SetEntityPusher( PushObjectFn fn );
	void PushEntity( lua_State *L, void *pEntity );	// pushes NULL entity for NULL
}
