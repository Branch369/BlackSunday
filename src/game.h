/*
 * BLACK SUNDAY - a dust-storm survival game for the Macintosh Plus.
 * game.h: shared definitions. The game logic (game.c) is plain C with no
 * Toolbox calls, so the same code runs under the Mac front end and under a
 * test harness on a modern machine.
 *
 * Integer sizes: THINK C uses a 16-bit int, Retro68 a 32-bit int. Nothing in
 * this code uses plain "int"; it uses the fixed-size names below.
 */
#ifndef GAME_H
#define GAME_H

#ifdef HOST_TEST
typedef int s32;            /* modern host: int is 32 bits */
typedef unsigned int u32;
#else
typedef long s32;           /* 68k Mac: long is 32 bits */
typedef unsigned long u32;
#endif
typedef short s16;
typedef unsigned short u16;

#define VIEW_W   32         /* map cells across the screen */
#define VIEW_H   18         /* map cells down the screen   */
#define LOT_W    16         /* a town lot, including its road edge */
#define LOT_H    9
#define TILE_PX  16

/* Tile numbers. The order must match the art in tiles.c. */
enum {
    T_GROUND, T_GROUND2, T_ROAD, T_WALL, T_DOOR, T_FLOOR, T_ROOF,
    T_FENCE, T_TREE, T_WRECK, T_DRIFT, T_GRAVE, T_CROP, T_PLATFORM,
    T_HANDCAR, T_WELL,
    T_OIL, T_RAG, T_SHELLS, T_BANDAGE,
    T_PLAYER, T_HUSK, T_HOUND, T_CROW,
    T_DUST0, T_DUST1, T_DUST2,
    T_COUNT
};

/* Commands the front end sends to the game. */
#define CMD_NORTH   1
#define CMD_SOUTH   2
#define CMD_EAST    3
#define CMD_WEST    4
#define CMD_WAIT    5
#define CMD_LAMP    6
#define CMD_FIRE    7
#define CMD_RAG     8
#define CMD_BANDAGE 9

#define ST_PLAY 0
#define ST_DEAD 1
#define ST_WON  2

#define M_NONE  0
#define M_HUSK  1           /* slow, follows sound and light, opens doors   */
#define M_HOUND 2           /* fast, lives in the wind, will not go indoors */
#define M_CROW  3           /* scarecrow: only moves when you look away     */

#define MAX_MON    16
#define MAX_TAKEN  128
#define MAX_SEEN   8
#define MSG_LEN    80
#define MAX_HEALTH 5

typedef struct {
    s32 x, y;
    s16 kind, hp;
} Mon;

typedef struct {
    u32 seed, rng;
    s32 px, py;             /* player position, world cells */
    s16 fx, fy;             /* facing (last direction tried) */
    s16 health, lungs, oil, shells, rags, bandages;
    s16 lamp, aiming, noise, indoors, vis, storm, state;
    s16 hit;                /* set when the last command hurt the player   */
    s16 whistled;           /* set when the last command blew the whistle  */
    s16 whistleDir;         /* 0=N 1=NE 2=E 3=SE 4=S 5=SW 6=W 7=NW          */
    s32 turn;
    s32 viewX, viewY;       /* world cell at the top-left of the screen */
    s32 inX0, inY0, inX1, inY1;   /* building the player is inside */
    s32 depotLX, depotLY;   /* lot that holds the rail depot */
    Mon mon[MAX_MON];
    s32 takenX[MAX_TAKEN], takenY[MAX_TAKEN];
    s16 takenN, takenHead;
    s32 seenLX[MAX_SEEN], seenLY[MAX_SEEN];
    s16 seenN, seenHead;
    char msg[MSG_LEN];
} Game;

extern Game g;
extern const char *gTileArt[T_COUNT][16];

void G_New(u32 seed);
void G_Command(s16 cmd);
s16  G_Cell(s16 vx, s16 vy);        /* tile to draw at a screen cell */
void G_DirtyBox(s16 *x0, s16 *y0, s16 *x1, s16 *y1);   /* cells that can show something */
void G_Status(char *buf);           /* one status line, under 80 chars */

/* Exposed for the test harness. */
s16  G_TileAt(s32 x, s32 y);
s16  G_Passable(s16 tile);

#endif
