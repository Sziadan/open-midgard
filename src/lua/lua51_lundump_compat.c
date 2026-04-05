/*
** Lua 5.1 undumper with compatibility for 32-bit precompiled chunks on x64.
*/

#include <string.h>

#define lundump_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstring.h"
#include "lundump.h"
#include "lzio.h"

typedef struct {
 lua_State* L;
 ZIO* Z;
 Mbuffer* b;
 const char* name;
 size_t chunkSizeTSize;
} LoadState;

#ifdef LUAC_TRUST_BINARIES
#define IF(c,s)
#define error(S,s)
#else
#define IF(c,s)         if (c) error(S,s)

static void error(LoadState* S, const char* why)
{
 luaO_pushfstring(S->L, "%s: %s in precompiled chunk", S->name, why);
 luaD_throw(S->L, LUA_ERRSYNTAX);
}
#endif

#define LoadMem(S,b,n,size) LoadBlock(S,b,(n)*(size))
#define LoadByte(S)         (lu_byte)LoadChar(S)
#define LoadVar(S,x)        LoadMem(S,&x,1,sizeof(x))
#define LoadVector(S,b,n,size) LoadMem(S,b,n,size)

static void LoadBlock(LoadState* S, void* b, size_t size)
{
 size_t r = luaZ_read(S->Z, b, size);
 IF (r != 0, "unexpected end");
}

static int LoadChar(LoadState* S)
{
 char x;
 LoadVar(S, x);
 return x;
}

static int LoadInt(LoadState* S)
{
 int x;
 LoadVar(S, x);
 IF (x < 0, "bad integer");
 return x;
}

static lua_Number LoadNumber(LoadState* S)
{
 lua_Number x;
 LoadVar(S, x);
 return x;
}

static size_t LoadChunkSize(LoadState* S)
{
 if (S->chunkSizeTSize == sizeof(size_t)) {
  size_t x;
  LoadVar(S, x);
  return x;
 }

 if (S->chunkSizeTSize == 4) {
  unsigned int x;
  LoadVar(S, x);
  return (size_t)x;
 }

 error(S, "unsupported size_t size");
 return 0;
}

static TString* LoadString(LoadState* S)
{
 size_t size = LoadChunkSize(S);
 if (size == 0)
  return NULL;
 else
 {
  char* s = luaZ_openspace(S->L, S->b, size);
  LoadBlock(S, s, size);
  return luaS_newlstr(S->L, s, size - 1);
 }
}

static void LoadCode(LoadState* S, Proto* f)
{
 int n = LoadInt(S);
 f->code = luaM_newvector(S->L, n, Instruction);
 f->sizecode = n;
 LoadVector(S, f->code, n, sizeof(Instruction));
}

static Proto* LoadFunction(LoadState* S, TString* p);

static void LoadConstants(LoadState* S, Proto* f)
{
 int i, n;
 n = LoadInt(S);
 f->k = luaM_newvector(S->L, n, TValue);
 f->sizek = n;
 for (i = 0; i < n; i++) setnilvalue(&f->k[i]);
 for (i = 0; i < n; i++)
 {
  TValue* o = &f->k[i];
  int t = LoadChar(S);
  switch (t)
  {
   case LUA_TNIL:
        setnilvalue(o);
    break;
   case LUA_TBOOLEAN:
        setbvalue(o, LoadChar(S) != 0);
    break;
   case LUA_TNUMBER:
    setnvalue(o, LoadNumber(S));
    break;
   case LUA_TSTRING:
    setsvalue2n(S->L, o, LoadString(S));
    break;
   default:
    error(S, "bad constant");
    break;
  }
 }
 n = LoadInt(S);
 f->p = luaM_newvector(S->L, n, Proto*);
 f->sizep = n;
 for (i = 0; i < n; i++) f->p[i] = NULL;
 for (i = 0; i < n; i++) f->p[i] = LoadFunction(S, f->source);
}

static void LoadDebug(LoadState* S, Proto* f)
{
 int i, n;
 n = LoadInt(S);
 f->lineinfo = luaM_newvector(S->L, n, int);
 f->sizelineinfo = n;
 LoadVector(S, f->lineinfo, n, sizeof(int));
 n = LoadInt(S);
 f->locvars = luaM_newvector(S->L, n, LocVar);
 f->sizelocvars = n;
 for (i = 0; i < n; i++) f->locvars[i].varname = NULL;
 for (i = 0; i < n; i++)
 {
  f->locvars[i].varname = LoadString(S);
  f->locvars[i].startpc = LoadInt(S);
  f->locvars[i].endpc = LoadInt(S);
 }
 n = LoadInt(S);
 f->upvalues = luaM_newvector(S->L, n, TString*);
 f->sizeupvalues = n;
 for (i = 0; i < n; i++) f->upvalues[i] = NULL;
 for (i = 0; i < n; i++) f->upvalues[i] = LoadString(S);
}

static Proto* LoadFunction(LoadState* S, TString* p)
{
 Proto* f;
 if (++S->L->nCcalls > LUAI_MAXCCALLS) error(S, "code too deep");
 f = luaF_newproto(S->L);
 setptvalue2s(S->L, S->L->top, f); incr_top(S->L);
 f->source = LoadString(S); if (f->source == NULL) f->source = p;
 f->linedefined = LoadInt(S);
 f->lastlinedefined = LoadInt(S);
 f->nups = LoadByte(S);
 f->numparams = LoadByte(S);
 f->is_vararg = LoadByte(S);
 f->maxstacksize = LoadByte(S);
 LoadCode(S, f);
 LoadConstants(S, f);
 LoadDebug(S, f);
 IF (!luaG_checkcode(f), "bad code");
 S->L->top--;
 S->L->nCcalls--;
 return f;
}

static void LoadHeader(LoadState* S)
{
 unsigned char h[LUAC_HEADERSIZE];
 int x = 1;
 const unsigned char expectedIntegral = (unsigned char)(((lua_Number)0.5) == 0);

 LoadBlock(S, h, LUAC_HEADERSIZE);

 IF (memcmp(h, LUA_SIGNATURE, sizeof(LUA_SIGNATURE) - 1) != 0, "bad header");
 IF (h[4] != LUAC_VERSION, "bad header");
 IF (h[5] != LUAC_FORMAT, "bad header");
 IF (h[6] != (unsigned char)*(char*)&x, "bad header");
 IF (h[7] != sizeof(int), "bad header");

 S->chunkSizeTSize = h[8];
 IF (S->chunkSizeTSize != sizeof(size_t) && S->chunkSizeTSize != 4, "unsupported size_t size");

 IF (h[9] != sizeof(Instruction), "bad header");
 IF (h[10] != sizeof(lua_Number), "bad header");
 IF (h[11] != expectedIntegral, "bad header");
}

Proto* luaU_undump(lua_State* L, ZIO* Z, Mbuffer* buff, const char* name)
{
 LoadState S;
 if (*name == '@' || *name == '=')
  S.name = name + 1;
 else if (*name == LUA_SIGNATURE[0])
  S.name = "binary string";
 else
  S.name = name;
 S.L = L;
 S.Z = Z;
 S.b = buff;
 S.chunkSizeTSize = sizeof(size_t);
 LoadHeader(&S);
 return LoadFunction(&S, luaS_newliteral(L, "=?"));
}

void luaU_header(char* h)
{
 int x = 1;
 memcpy(h, LUA_SIGNATURE, sizeof(LUA_SIGNATURE) - 1);
 h += sizeof(LUA_SIGNATURE) - 1;
 *h++ = (char)LUAC_VERSION;
 *h++ = (char)LUAC_FORMAT;
 *h++ = (char)*(char*)&x;
 *h++ = (char)sizeof(int);
 *h++ = (char)sizeof(size_t);
 *h++ = (char)sizeof(Instruction);
 *h++ = (char)sizeof(lua_Number);
 *h++ = (char)(((lua_Number)0.5) == 0);
}