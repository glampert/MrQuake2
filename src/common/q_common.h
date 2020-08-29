/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef Q_COMMON_H
#define Q_COMMON_H

#include "game/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

//============================================================================
//
// Definitions common between client and server, but not game
//
//============================================================================

#define VERSION 3.19
#define BASEDIRNAME "baseq2"

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)

#if defined(NDEBUG)
    #define BUILDSTRING "Win32 RELEASE"
#else
    #define BUILDSTRING "Win32 DEBUG"
#endif

#if defined(_M_IX86)
    #define CPUSTRING "x86"
#elif defined(_M_X64) || defined(_M_IA64) || defined(WIN64) || defined(_WIN64)
    #define CPUSTRING "x64"
#endif

#else // !WIN32

#define BUILDSTRING "NON-WIN32"
#define CPUSTRING   "NON-WIN32"

#endif // build/cpu strings

//============================================================================

typedef struct sizebuf_s
{
    qboolean allowoverflow; // if false, do a Com_Error
    qboolean overflowed;    // set to true if the buffer size failed
    qbyte * data;
    int maxsize;
    int cursize;
    int readcount;
} sizebuf_t;

void SZ_Init(sizebuf_t * buf, qbyte * data, int length);
void SZ_Clear(sizebuf_t * buf);
void * SZ_GetSpace(sizebuf_t * buf, int length);
void SZ_Write(sizebuf_t * buf, const void * data, int length);
void SZ_Print(sizebuf_t * buf, const char * data); // strcats onto the sizebuf

//============================================================================

struct usercmd_s;
struct entity_state_s;

void MSG_WriteChar(sizebuf_t * sb, int c);
void MSG_WriteByte(sizebuf_t * sb, int c);
void MSG_WriteShort(sizebuf_t * sb, int c);
void MSG_WriteLong(sizebuf_t * sb, int c);
void MSG_WriteFloat(sizebuf_t * sb, float f);
void MSG_WriteString(sizebuf_t * sb, char * s);
void MSG_WriteCoord(sizebuf_t * sb, float f);
void MSG_WritePos(sizebuf_t * sb, vec3_t pos);
void MSG_WriteAngle(sizebuf_t * sb, float f);
void MSG_WriteAngle16(sizebuf_t * sb, float f);
void MSG_WriteDeltaUsercmd(sizebuf_t * sb, struct usercmd_s * from, struct usercmd_s * cmd);
void MSG_WriteDeltaEntity(struct entity_state_s * from, struct entity_state_s * to, sizebuf_t * msg, qboolean force, qboolean newentity);
void MSG_WriteDir(sizebuf_t * sb, vec3_t vector);
void MSG_BeginReading(sizebuf_t * sb);
int MSG_ReadChar(sizebuf_t * sb);
int MSG_ReadByte(sizebuf_t * sb);
int MSG_ReadShort(sizebuf_t * sb);
int MSG_ReadLong(sizebuf_t * sb);
float MSG_ReadFloat(sizebuf_t * sb);
char * MSG_ReadString(sizebuf_t * sb);
char * MSG_ReadStringLine(sizebuf_t * sb);
float MSG_ReadCoord(sizebuf_t * sb);
void MSG_ReadPos(sizebuf_t * sb, vec3_t pos);
float MSG_ReadAngle(sizebuf_t * sb);
float MSG_ReadAngle16(sizebuf_t * sb);
void MSG_ReadDeltaUsercmd(sizebuf_t * sb, struct usercmd_s * from, struct usercmd_s * cmd);
void MSG_ReadDir(sizebuf_t * sb, vec3_t vector);
void MSG_ReadData(sizebuf_t * sb, void * buffer, int size);

//============================================================================

extern qboolean bigendien;

short BigShort(short l);
short LittleShort(short l);
int BigLong(int l);
int LittleLong(int l);
float BigFloat(float l);
float LittleFloat(float l);

//============================================================================

int COM_Argc(void);
char * COM_Argv(int arg); // range and null checked
void COM_ClearArgv(int arg);
int COM_CheckParm(const char * parm);
void COM_AddParm(char * parm);

void COM_Init(void);
void COM_InitArgv(int argc, char ** argv);

char * Q_CopyString(const char * in); // strdup replaces that used Z_Malloc.

//============================================================================

void Info_Print(const char * s);

// crc.h
void CRC_Init(unsigned short * crcvalue);
void CRC_ProcessByte(unsigned short * crcvalue, qbyte data);
unsigned short CRC_Value(unsigned short crcvalue);
unsigned short CRC_Block(qbyte * start, int count);

/*
==============================================================

PROTOCOL

==============================================================
*/

// protocol.h -- communications protocols

#define PROTOCOL_VERSION 34

//=========================================

#define PORT_MASTER 27900
#define PORT_CLIENT 27901
#define PORT_SERVER 27910

//=========================================

#define UPDATE_BACKUP 16 // copies of entity_state_t to keep buffered must be power of two
#define UPDATE_MASK (UPDATE_BACKUP - 1)

//==================
// the svc_strings[] array in cl_parse.c should mirror this
//==================

//
// server to client
//
enum svc_ops_e
{
    svc_bad,

    // these ops are known to the game dll
    svc_muzzleflash,
    svc_muzzleflash2,
    svc_temp_entity,
    svc_layout,
    svc_inventory,

    // the rest are private to the client and server
    svc_nop,
    svc_disconnect,
    svc_reconnect,
    svc_sound,        // <see code>
    svc_print,        // [qbyte] id [string] null terminated string
    svc_stufftext,    // [string] stuffed into client's console buffer, should be \n terminated
    svc_serverdata,   // [long] protocol ...
    svc_configstring, // [short] [string]
    svc_spawnbaseline,
    svc_centerprint,         // [string] to put in center of the screen
    svc_download,            // [short] size [size bytes]
    svc_playerinfo,          // variable
    svc_packetentities,      // [...]
    svc_deltapacketentities, // [...]
    svc_frame
};

//==============================================

//
// client to server
//
enum clc_ops_e
{
    clc_bad,
    clc_nop,
    clc_move,     // [[usercmd_t]
    clc_userinfo, // [[userinfo string]
    clc_stringcmd // [string] message
};

//==============================================

// plyer_state_t communication

#define PS_M_TYPE (1 << 0)
#define PS_M_ORIGIN (1 << 1)
#define PS_M_VELOCITY (1 << 2)
#define PS_M_TIME (1 << 3)
#define PS_M_FLAGS (1 << 4)
#define PS_M_GRAVITY (1 << 5)
#define PS_M_DELTA_ANGLES (1 << 6)

#define PS_VIEWOFFSET (1 << 7)
#define PS_VIEWANGLES (1 << 8)
#define PS_KICKANGLES (1 << 9)
#define PS_BLEND (1 << 10)
#define PS_FOV (1 << 11)
#define PS_WEAPONINDEX (1 << 12)
#define PS_WEAPONFRAME (1 << 13)
#define PS_RDFLAGS (1 << 14)

//==============================================

// user_cmd_t communication

// ms and light always sent, the others are optional
#define CM_ANGLE1 (1 << 0)
#define CM_ANGLE2 (1 << 1)
#define CM_ANGLE3 (1 << 2)
#define CM_FORWARD (1 << 3)
#define CM_SIDE (1 << 4)
#define CM_UP (1 << 5)
#define CM_BUTTONS (1 << 6)
#define CM_IMPULSE (1 << 7)

//==============================================

// a sound without an ent or pos will be a local only sound
#define SND_VOLUME (1 << 0)      // a qbyte
#define SND_ATTENUATION (1 << 1) // a qbyte
#define SND_POS (1 << 2)         // three coordinates
#define SND_ENT (1 << 3)         // a short 0-2: channel, 3-12: entity
#define SND_OFFSET (1 << 4)      // a qbyte, msec offset from frame start

#define DEFAULT_SOUND_PACKET_VOLUME 1.0
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

//==============================================

// entity_state_t communication

// try to pack the common update flags into the first qbyte
#define U_ORIGIN1 (1 << 0)
#define U_ORIGIN2 (1 << 1)
#define U_ANGLE2 (1 << 2)
#define U_ANGLE3 (1 << 3)
#define U_FRAME8 (1 << 4) // frame is a qbyte
#define U_EVENT (1 << 5)
#define U_REMOVE (1 << 6)    // REMOVE this entity, don't add it
#define U_MOREBITS1 (1 << 7) // read one additional qbyte

// second byte
#define U_NUMBER16 (1 << 8) // NUMBER8 is implicit if not set
#define U_ORIGIN3 (1 << 9)
#define U_ANGLE1 (1 << 10)
#define U_MODEL (1 << 11)
#define U_RENDERFX8 (1 << 12) // fullbright, etc
#define U_EFFECTS8 (1 << 14)  // autorotate, trails, etc
#define U_MOREBITS2 (1 << 15) // read one additional qbyte

// third byte
#define U_SKIN8 (1 << 16)
#define U_FRAME16 (1 << 17)    // frame is a short
#define U_RENDERFX16 (1 << 18) // 8 + 16 = 32
#define U_EFFECTS16 (1 << 19)  // 8 + 16 = 32
#define U_MODEL2 (1 << 20)     // weapons, flags, etc
#define U_MODEL3 (1 << 21)
#define U_MODEL4 (1 << 22)
#define U_MOREBITS3 (1 << 23) // read one additional qbyte

// fourth byte
#define U_OLDORIGIN (1 << 24) // FIXME: get rid of this
#define U_SKIN16 (1 << 25)
#define U_SOUND (1 << 26)
#define U_SOLID (1 << 27)

/*
==============================================================

CMD

Command text buffering and command execution

==============================================================
*/

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.

The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();

*/

typedef enum
{
    EXEC_NOW    = 0, // don't return until completed
    EXEC_INSERT = 1, // insert at current position, but don't run yet
    EXEC_APPEND = 2  // add to end of the command buffer
} cmd_exec_when_t;

void Cbuf_Init(void);
// allocates an initial text buffer that will grow as needed

void Cbuf_AddText(const char * text);
// as new commands are generated from the console or keybindings,
// the text is added to the end of the command buffer.

void Cbuf_InsertText(const char * text);
// when a command wants to issue other commands immediately, the text is
// inserted at the beginning of the buffer, before any remaining unexecuted
// commands.

void Cbuf_ExecuteText(cmd_exec_when_t exec_when, const char * text);
// this can be used in place of either Cbuf_AddText or Cbuf_InsertText

void Cbuf_AddEarlyCommands(qboolean clear);
// adds all the +set commands from the command line

qboolean Cbuf_AddLateCommands(void);
// adds all the remaining + commands from the command line
// Returns true if any late commands were added, which
// will keep the demoloop from immediately starting

void Cbuf_Execute(void);
// Pulls off \n terminated lines of text from the command buffer and sends
// them through Cmd_ExecuteString.  Stops when the buffer is empty.
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function!

void Cbuf_CopyToDefer(void);
void Cbuf_InsertFromDefer(void);
// These two functions are used to defer any pending commands while a map
// is being loaded

//===========================================================================

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

*/

typedef void (*xcommand_t)(void);

void Cmd_Init(void);

void Cmd_AddCommand(const char * cmd_name, xcommand_t function);
// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory
// if function is NULL, the command will be forwarded to the server
// as a clc_stringcmd instead of executed locally
void Cmd_RemoveCommand(const char * cmd_name);

qboolean Cmd_Exists(const char * cmd_name);
// used by the cvar code to check for cvar / command name overlap

const char * Cmd_CompleteCommand(const char * partial);
// attempts to match a partial command for automatic command line completion
// returns NULL if nothing fits

int Cmd_Argc(void);
const char * Cmd_Argv(int arg);
const char * Cmd_Args(void);
// The functions that execute commands get their parameters with these
// functions. Cmd_Argv () will return an empty string, not a NULL
// if arg > argc, so string operations are always safe.

void Cmd_TokenizeString(const char * text, qboolean macroExpand);
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

void Cmd_ExecuteString(const char * text);
// Parses a single line of text into arguments and tries to execute it
// as if it was typed at the console

void Cmd_ForwardToServer(void);
// adds the current command line as a clc_stringcmd to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.

/*
==============================================================

CVAR

==============================================================
*/

/*
cvar_t variables are used to hold scalar or string variables that can be changed or
displayed at the console or prog code as well as accessed directly
in C code.

The user can access cvars from the console in three ways:
r_draworder			prints the current value
r_draworder 0		sets the current value to 0
set r_draworder 0	as above, but creates the cvar if not present
Cvars are restricted from having the same names as commands to keep this
interface from being ambiguous.
*/
extern cvar_t * cvar_vars;

cvar_t * Cvar_Get(const char * var_name, const char * value, int flags);
// creates the variable if it doesn't exist, or returns the existing one
// if it exists, the value will not be changed, but flags will be ORed in
// that allows variables to be unarchived without needing bitflags

cvar_t * Cvar_Set(const char * var_name, const char * value);
// will create the variable if it doesn't exist

cvar_t * Cvar_ForceSet(const char * var_name, const char * value);
// will set the variable even if NOSET or LATCH

cvar_t * Cvar_FullSet(const char * var_name, const char * value, int flags);

void Cvar_SetValue(const char * var_name, float value);
// expands value to a string and calls Cvar_Set

float Cvar_VariableValue(const char * var_name);
// returns 0 if not defined or non numeric

char * Cvar_VariableString(const char * var_name);
// returns an empty string if not defined

char * Cvar_CompleteVariable(const char * partial);
// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits

void Cvar_GetLatchedVars(void);
// any CVAR_LATCHED variables that have been set will now take effect

qboolean Cvar_Command(void);
// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)

void Cvar_WriteVariables(const char * path);
// appends lines containing "set variable value" for all variables
// with the archive flag set to true.

void Cvar_Init(void);

char * Cvar_Userinfo(void);
// returns an info string containing all the CVAR_USERINFO cvars

char * Cvar_Serverinfo(void);
// returns an info string containing all the CVAR_SERVERINFO cvars

extern qboolean userinfo_modified;
// this is set each time a CVAR_USERINFO variable is changed
// so that the client knows to send it to the server

/*
==============================================================

NET layer

==============================================================
*/

// net.h -- quake's interface to the networking layer

enum
{
    PORT_ANY = -1,
    MAX_MSGLEN = 1400, // max length of a message
    PACKET_HEADER = 10 // two ints and a short
};

typedef enum
{
    NA_LOOPBACK,
    NA_BROADCAST,
    NA_IP,
    NA_IPX,
    NA_BROADCAST_IPX
} netadrtype_t;

typedef enum
{
    NS_CLIENT,
    NS_SERVER
} netsrc_t;

typedef struct
{
    netadrtype_t type;
    qbyte ip[4];
    qbyte ipx[10];
    unsigned short port;
} netadr_t;

void NET_Init(void);
void NET_Shutdown(void);
void NET_Config(qboolean multiplayer);
void NET_Sleep(int msec);

qboolean NET_GetPacket(netsrc_t sock, netadr_t * net_from, sizebuf_t * net_message);
void NET_SendPacket(netsrc_t sock, int length, const void * data, netadr_t to);

qboolean NET_CompareAdr(netadr_t a, netadr_t b);
qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b);
qboolean NET_IsLocalAddress(netadr_t adr);

char * NET_AdrToString(netadr_t addr);
qboolean NET_StringToAdr(const char * s, netadr_t * addr);

//============================================================================

enum { MAX_LATENT = 32 };

typedef struct
{
    qboolean fatal_error;

    netsrc_t sock;
    int dropped; // between last packet and previous

    int last_received; // for timeouts
    int last_sent;     // for retransmits

    netadr_t remote_address;
    int qport; // qport value to write when transmitting

    // sequencing variables
    int incoming_sequence;
    int incoming_acknowledged;
    int incoming_reliable_acknowledged; // single bit
    int incoming_reliable_sequence;     // single bit, maintained local

    int outgoing_sequence;
    int reliable_sequence;      // single bit
    int last_reliable_sequence; // sequence number of last send

    // reliable staging and holding areas
    sizebuf_t message;                 // writing buffer to send to server
    qbyte message_buf[MAX_MSGLEN - 16]; // leave space for header

    // message is copied to this buffer when it is first transfered
    int reliable_length;
    qbyte reliable_buf[MAX_MSGLEN - 16]; // unacked reliable message
} netchan_t;

extern netadr_t net_from;
extern sizebuf_t net_message;
extern qbyte net_message_buffer[MAX_MSGLEN];

void Netchan_Init(void);
void Netchan_Setup(netsrc_t sock, netchan_t * chan, netadr_t adr, int qport);

void Netchan_Transmit(netchan_t * chan, int length, qbyte * data);
void Netchan_OutOfBand(int net_socket, netadr_t adr, int length, qbyte * data);
void Netchan_OutOfBandPrint(int net_socket, netadr_t adr, char * format, ...);

qboolean Netchan_Process(netchan_t * chan, sizebuf_t * msg);
qboolean Netchan_CanReliable(netchan_t * chan);
qboolean Netchan_NeedReliable(netchan_t * chan);

/*
==============================================================

CMODEL

==============================================================
*/

#include "common/q_files.h"

cmodel_t * CM_LoadMap(char * name, qboolean clientload, unsigned * checksum);
cmodel_t * CM_InlineModel(char * name); // *1, *2, etc

int CM_NumClusters(void);
int CM_NumInlineModels(void);
char * CM_EntityString(void);

// creates a clipping hull for an arbitrary box
int CM_HeadnodeForBox(vec3_t mins, vec3_t maxs);

// returns an ORed contents mask
int CM_PointContents(vec3_t p, int headnode);
int CM_TransformedPointContents(vec3_t p, int headnode, vec3_t origin, vec3_t angles);

trace_t CM_BoxTrace(vec3_t start, vec3_t end,
                    vec3_t mins, vec3_t maxs,
                    int headnode, int brushmask);

trace_t CM_TransformedBoxTrace(vec3_t start, vec3_t end,
                               vec3_t mins, vec3_t maxs,
                               int headnode, int brushmask,
                               vec3_t origin, vec3_t angles);

qbyte * CM_ClusterPVS(int cluster);
qbyte * CM_ClusterPHS(int cluster);

int CM_PointLeafnum(vec3_t p);

// call with topnode set to the headnode, returns with topnode
// set to the first node that splits the box
int CM_BoxLeafnums(vec3_t mins, vec3_t maxs, int * list,
                   int listsize, int * topnode);

int CM_LeafContents(int leafnum);
int CM_LeafCluster(int leafnum);
int CM_LeafArea(int leafnum);

void CM_SetAreaPortalState(int portalnum, qboolean open);
qboolean CM_AreasConnected(int area1, int area2);

int CM_WriteAreaBits(qbyte * buffer, int area);
qboolean CM_HeadnodeVisible(int headnode, qbyte * visbits);

void CM_WritePortalState(FILE * f);
void CM_ReadPortalState(FILE * f);

/*
==============================================================

PLAYER MOVEMENT CODE

Common between server and client so prediction matches

==============================================================
*/

extern float pm_airaccelerate;
void Pmove(pmove_t * pmove);

/*
==============================================================

FILESYSTEM

==============================================================
*/

void FS_InitFilesystem(void);
void FS_SetGamedir(const char * dir);
void FS_SetDefaultBasePath(const char * path);
char * FS_Gamedir(void);
char * FS_NextPath(char * prevpath);
void FS_ExecAutoexec(void);

int FS_FOpenFile(const char * filename, FILE ** file);
void FS_FCloseFile(FILE * f); // note: this can't be called from another DLL, due to MS libc issues

// a null buffer will just return the file length without loading
// a -1 length is not present
int FS_LoadFile(const char * path, void ** buffer);

// read specified number of bytes
int FS_LoadFilePortion(const char * path, void * dest_buffer, int num_bytes_to_read);

// properly handles partial reads
void FS_Read(void * buffer, int len, FILE * f);

void FS_FreeFile(void * buffer);
void FS_CreatePath(char * path);

/*
==============================================================

MISC

==============================================================
*/

void Com_BeginRedirect(int target, char * buffer, int buffersize, void (*flush)(int, char *));
void Com_EndRedirect(void);
void Com_Printf(const char * fmt, ...)  Q_PRINTF_FUNC(1, 2);
void Com_DPrintf(const char * fmt, ...) Q_PRINTF_FUNC(1, 2);
void Com_Error(error_level_t code, const char * fmt, ...) Q_PRINTF_FUNC(2, 3);
void Com_Quit(void);

int Com_ServerState(void); // this should have just been a cvar...
void Com_SetServerState(int state);

unsigned Com_BlockChecksum(void * buffer, int length);
qbyte COM_BlockSequenceCRCByte(qbyte * base, int length, int sequence);

float frand(void); //  0 to 1
float crand(void); // -1 to 1

extern cvar_t * developer;
extern cvar_t * dedicated;
extern cvar_t * host_speeds;
extern cvar_t * log_stats;

extern FILE * log_stats_file;

// host_speeds times
extern int time_before_game;
extern int time_after_game;
extern int time_before_ref;
extern int time_after_ref;

void Z_Free(void * ptr);
void * Z_Malloc(int size); // returns 0 filled memory
void * Z_TagMalloc(int size, int tag);
void Z_FreeTags(int tag);

void Qcommon_Init(int argc, char ** argv);
void Qcommon_Frame(int msec);
void Qcommon_Shutdown(void);

enum { NUMVERTEXNORMALS = 162 };
extern vec3_t bytedirs[NUMVERTEXNORMALS];

// this is in the client code, but can be used for debugging from server
void SCR_DebugGraph(float value, int color);

/*
==============================================================

NON-PORTABLE SYSTEM SERVICES

==============================================================
*/

void Sys_Init(void);
void Sys_AppActivate(void);

// loads the game dll and calls the api init function
void * Sys_GetGameAPI(void * parms);
void Sys_UnloadGame(void);

char * Sys_ConsoleInput(void);
void Sys_ConsoleOutput(const char * string);
void Sys_SendKeyEvents(void);

void Sys_Error(const char * error, ...) Q_PRINTF_FUNC(1, 2);
void Sys_Quit(void);

char * Sys_GetClipboardData(void);

/*
==============================================================

CLIENT / SERVER SYSTEMS

==============================================================
*/

void CL_Init(void);
void CL_Drop(void);
void CL_Shutdown(void);
void CL_Frame(int msec);
void Con_Print(char * text);
void SCR_BeginLoadingPlaque(void);

void SV_Init(void);
void SV_Shutdown(char * finalmsg, qboolean reconnect);
void SV_Frame(int msec);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // Q_COMMON_H
