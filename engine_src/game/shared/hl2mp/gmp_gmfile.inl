// GMod's dupe (.dupe, "DUP3") and save (.gms, "GMS3") files: an LZMA-alone stream of the JSON
// that util.TableToJSON writes. JSON maps to KeyValues the way tier1's KeyValuesJSONParser
// reads it: arrays become subkeys "0", "1"... Vectors are strings "[x y z]", angles "{p y r}".

#include "tier1/keyvaluesjson.h"
#include "tier1/utlbuffer.h"

// tier1 links the LZMA SDK decoder (utils/lzma/C/LzmaDec.c). Declared here rather than through
// its headers, whose Byte/UInt32 typedefs would land in the game DLL namespace.
struct GMPSzAlloc_t
{
	void *( *Alloc )( void *p, size_t size );
	void ( *Free )( void *p, void *address );
};
extern "C" int LzmaDecode( unsigned char *dest, size_t *destLen, const unsigned char *src, size_t *srcLen,
	const unsigned char *propData, unsigned propSize, int finishMode, int *status, GMPSzAlloc_t *alloc );

static void *GMP_LzmaAlloc( void *, size_t size ) { return malloc( size ); }
static void GMP_LzmaFree( void *, void *address ) { free( address ); }

// LZMA-alone: 5 property bytes, 8-byte little endian unpacked size, the stream.
static bool GMP_LzmaUnpack( const unsigned char *src, int len, CUtlBuffer &out )
{
	if ( len < 13 )
		return false;
	uint64 size = 0;
	for ( int i = 0; i < 8; i++ )
		size |= (uint64)src[5 + i] << ( 8 * i );
	if ( size == ~(uint64)0 || size > 256u * 1024 * 1024 )
		size = MIN( (uint64)len * 40, (uint64)256u * 1024 * 1024 );	// unknown size: decode as much as fits
	out.EnsureCapacity( (int)size + 1 );
	size_t destLen = (size_t)size, srcLen = len - 13;
	int status = 0;
	GMPSzAlloc_t alloc = { GMP_LzmaAlloc, GMP_LzmaFree };
	const int res = LzmaDecode( (unsigned char *)out.Base(), &destLen, src + 13, &srcLen, src, 5, 0 /* LZMA_FINISH_ANY */, &status, &alloc );
	if ( res != 0 )
		return false;
	out.SeekPut( CUtlBuffer::SEEK_HEAD, (int)destLen );
	return true;
}

// The game DLLs do not link the LZMA encoder: this writes a valid stream of literals only
// (lc 3, lp 0, pb 2 like GMod), so any LZMA decoder, GMod's included, reads it.
class CGMPLzmaWriter
{
public:
	CGMPLzmaWriter( CUtlBuffer &out ) : m_Out( out ), m_nLow( 0 ), m_nRange( 0xFFFFFFFF ), m_nCache( 0 ), m_nCacheSize( 1 )
	{
		for ( int i = 0; i < ARRAYSIZE( m_IsMatch ); i++ )
			m_IsMatch[i] = 1024;
		for ( int i = 0; i < ARRAYSIZE( m_Literal ); i++ )
			m_Literal[i] = 1024;
	}

	void Encode( const unsigned char *data, int len )
	{
		const unsigned int dict = 1 << 16;
		m_Out.PutUnsignedChar( 0x5D );	// ( pb 2 * 5 + lp 0 ) * 9 + lc 3
		for ( int i = 0; i < 4; i++ )
			m_Out.PutUnsignedChar( ( dict >> ( 8 * i ) ) & 0xFF );
		for ( int i = 0; i < 8; i++ )
			m_Out.PutUnsignedChar( i < 4 ? ( (unsigned int)len >> ( 8 * i ) ) & 0xFF : 0 );
		unsigned char prev = 0;
		for ( int pos = 0; pos < len; pos++ )
		{
			EncodeBit( m_IsMatch[pos & 3], 0 );	// state 0: literal after literal
			unsigned short *probs = m_Literal + 0x300 * ( prev >> 5 );
			unsigned int symbol = data[pos] | 0x100;
			for ( unsigned int ctx = 1; ctx < 0x100; )
			{
				const int bit = ( symbol >> 7 ) & 1;
				symbol <<= 1;
				EncodeBit( probs[ctx], bit );
				ctx = ( ctx << 1 ) | bit;
			}
			prev = data[pos];
		}
		for ( int i = 0; i < 5; i++ )
			ShiftLow();
	}

private:
	void EncodeBit( unsigned short &prob, int bit )
	{
		const unsigned int bound = ( m_nRange >> 11 ) * prob;
		if ( !bit )
		{
			m_nRange = bound;
			prob += ( 2048 - prob ) >> 5;
		}
		else
		{
			m_nLow += bound;
			m_nRange -= bound;
			prob -= prob >> 5;
		}
		while ( m_nRange < ( 1u << 24 ) )
		{
			m_nRange <<= 8;
			ShiftLow();
		}
	}

	void ShiftLow( void )
	{
		if ( (unsigned int)m_nLow < 0xFF000000u || ( m_nLow >> 32 ) != 0 )
		{
			unsigned char temp = m_nCache;
			do
			{
				m_Out.PutUnsignedChar( (unsigned char)( temp + (unsigned char)( m_nLow >> 32 ) ) );
				temp = 0xFF;
			} while ( --m_nCacheSize != 0 );
			m_nCache = (unsigned char)( (unsigned int)m_nLow >> 24 );
		}
		m_nCacheSize++;
		m_nLow = (uint64)( ( (unsigned int)m_nLow & 0x00FFFFFF ) << 8 );
	}

	CUtlBuffer &m_Out;
	uint64 m_nLow;
	unsigned int m_nRange;
	unsigned char m_nCache;
	uint64 m_nCacheSize;
	unsigned short m_IsMatch[4];
	unsigned short m_Literal[0x300 * 8];
};

static void GMP_JSONString( CUtlBuffer &out, const char *s )
{
	out.PutChar( '"' );
	for ( ; *s; s++ )
	{
		const unsigned char c = *s;
		if ( c == '"' || c == '\\' )
		{
			out.PutChar( '\\' );
			out.PutChar( c );
		}
		else if ( c == '\n' )
			out.PutString( "\\n" );
		else if ( c == '\t' )
			out.PutString( "\\t" );
		else if ( c < 0x20 )
			out.Printf( "\\u%04x", c );
		else
			out.PutChar( c );
	}
	out.PutChar( '"' );
}

// Children "0".."n-1" in order are a JSON array. GMod writes some 0-based tables as objects;
// those keep their object form (PhysicsObjects, Entities, Constraints of a save).
static bool GMP_JSONIsArray( KeyValues *kv )
{
	const char *name = kv->GetName();
	if ( !Q_stricmp( name, "PhysicsObjects" ) || !Q_stricmp( name, "Entities" ) || !Q_stricmp( name, "BoneMods" ) )
		return false;
	int n = 0;
	for ( KeyValues *sub = kv->GetFirstSubKey(); sub; sub = sub->GetNextKey(), n++ )
	{
		char want[16];
		Q_snprintf( want, sizeof( want ), "%d", n );
		if ( Q_strcmp( sub->GetName(), want ) )
			return false;
	}
	return n > 0 || kv->GetBool( "__array" );
}

static void GMP_WriteJSONValue( CUtlBuffer &out, KeyValues *kv )
{
	if ( kv->GetFirstSubKey() || kv->GetDataType() == KeyValues::TYPE_NONE )
	{
		const bool array = GMP_JSONIsArray( kv );
		out.PutChar( array ? '[' : '{' );
		bool first = true;
		for ( KeyValues *sub = kv->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
		{
			if ( !first )
				out.PutChar( ',' );
			first = false;
			if ( !array )
			{
				GMP_JSONString( out, sub->GetName() );
				out.PutChar( ':' );
			}
			GMP_WriteJSONValue( out, sub );
		}
		out.PutChar( array ? ']' : '}' );
		return;
	}
	switch ( kv->GetDataType() )
	{
	case KeyValues::TYPE_INT:
		out.Printf( "%d", kv->GetInt() );
		break;
	case KeyValues::TYPE_UINT64:
		out.Printf( "%llu", (unsigned long long)kv->GetUint64() );
		break;
	case KeyValues::TYPE_FLOAT:
	{
		char text[64];
		Q_snprintf( text, sizeof( text ), "%.6g", kv->GetFloat() );
		out.PutString( text );
		if ( !strpbrk( text, ".eEn" ) )
			out.PutString( ".0" );
		break;
	}
	case KeyValues::TYPE_PTR:
		out.PutString( kv->GetPtr() ? "true" : "null" );
		break;
	default:
		GMP_JSONString( out, kv->GetString() );
		break;
	}
}

// GMod booleans: KeyValues has no bool type, the JSON parser turns true/false into ints.
static void GMP_SetJSONBool( KeyValues *kv, const char *key, bool value ) { kv->SetInt( key, value ? 1 : 0 ); }

static KeyValues *GMP_ParseJSON( const char *text, int len )
{
	// GMod pads the JSON with \1 / \2 markers in saves; trim to the outer object.
	const char *start = (const char *)memchr( text, '{', len );
	if ( !start )
		return NULL;
	const char *end = text + len;
	while ( end > start && end[-1] != '}' )
		end--;
	KeyValuesJSONParser parser( start, (int)( end - start ) );
	KeyValues *kv = parser.ParseFile();
	if ( !kv )
		Warning( "GModPort: JSON error line %d: %s\n", parser.m_nLine, parser.m_szErrMsg );
	return kv;
}

static Vector GMP_ParseVector( const char *s )
{
	Vector v( 0, 0, 0 );
	if ( s && ( *s == '[' || *s == '{' ) )
		s++;
	if ( s )
		sscanf( s, "%f %f %f", &v.x, &v.y, &v.z );
	return v;
}

static QAngle GMP_ParseAngle( const char *s )
{
	Vector v = GMP_ParseVector( s );
	return QAngle( v.x, v.y, v.z );
}

static void GMP_SetVector( KeyValues *kv, const char *key, const Vector &v )
{
	kv->SetString( key, UTIL_VarArgs( "[%.4f %.4f %.4f]", v.x, v.y, v.z ) );
}

static void GMP_SetAngle( KeyValues *kv, const char *key, const QAngle &a )
{
	kv->SetString( key, UTIL_VarArgs( "{%.4f %.4f %.4f}", a.x, a.y, a.z ) );
}

// "DUP3" / "GMS3" + header strings + LZMA JSON. headerStrings: the NUL-terminated strings after
// the magic (GMS3: map name, "WSID:..."), returned in *pHeader when given.
static KeyValues *GMP_ReadGMFile( const char *path, const char *pathID, const char *magic, int headerStrings, CUtlString *pFirstHeader )
{
	CUtlBuffer file;
	if ( !filesystem->ReadFile( path, pathID, file ) )
		return NULL;
	const unsigned char *data = (const unsigned char *)file.Base();
	int len = file.TellPut(), pos = 4;
	if ( len < 4 || memcmp( data, magic, 4 ) )
	{
		Warning( "GModPort: %s is not a %s file\n", path, magic );
		return NULL;
	}
	for ( int i = 0; i < headerStrings; i++ )
	{
		const unsigned char *nul = (const unsigned char *)memchr( data + pos, 0, len - pos );
		if ( !nul )
			return NULL;
		if ( i == 0 && pFirstHeader )
			pFirstHeader->Set( (const char *)data + pos );
		pos = nul - data + 1;
	}
	CUtlBuffer json;
	if ( !GMP_LzmaUnpack( data + pos, len - pos, json ) )
	{
		Warning( "GModPort: %s: LZMA error\n", path );
		return NULL;
	}
	return GMP_ParseJSON( (const char *)json.Base(), json.TellPut() );
}

static bool GMP_WriteGMFile( const char *path, const char *magic, const char *const *headerStrings, int count, KeyValues *kv )
{
	CUtlBuffer json( 0, 0, CUtlBuffer::TEXT_BUFFER );	// text mode: PutString / Printf add no NUL
	GMP_WriteJSONValue( json, kv );
	CUtlBuffer file;
	file.Put( magic, 4 );
	for ( int i = 0; i < count; i++ )
		file.Put( headerStrings[i], Q_strlen( headerStrings[i] ) + 1 );
	CGMPLzmaWriter *writer = new CGMPLzmaWriter( file );
	writer->Encode( (const unsigned char *)json.Base(), json.TellPut() );
	delete writer;
	char dir[MAX_PATH];
	Q_strncpy( dir, path, sizeof( dir ) );
	Q_StripFilename( dir );
	filesystem->CreateDirHierarchy( dir, "MOD" );
	return filesystem->WriteFile( path, "MOD", file );
}
