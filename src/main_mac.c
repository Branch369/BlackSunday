/*
 * BLACK SUNDAY - main_mac.c: Macintosh front end.
 * Targets a Macintosh Plus (68000, 512x342, 1-bit) on System 6 or 7.
 * Builds with Retro68 or THINK C. Uses only ROM Toolbox calls: QuickDraw,
 * the Window, Menu and Event Managers. No resources, no C library.
 *
 * Drawing: every screen cell is a 16x16 tile copied with CopyBits from one
 * offscreen strip. Source and destination are both word-aligned, which is
 * the fast path on a 68000. A shadow copy of the screen means only cells
 * that changed are redrawn each turn.
 */
#include "game.h"

#ifdef THINK_C
/* THINK C: the MacHeaders precompiled header supplies the Toolbox, and the
   QuickDraw globals are plain variables. */
#define QD(x) x
#else
#include <Quickdraw.h>
#include <Windows.h>
#include <Menus.h>
#include <Fonts.h>
#include <Dialogs.h>
#include <TextEdit.h>
#include <Events.h>
#include <ToolUtils.h>
#include <OSUtils.h>
#define QD(x) qd.x
#endif

#define WIN_W    512
#define MAP_H    288            /* 18 rows of 16 pixels */
#define WIN_H    322            /* map plus a two-line status strip */
#define MENU_ID  129

static WindowPtr gWin;
static MenuHandle gFileMenu;
static BitMap gTiles;
static unsigned short gTileBits[16][T_COUNT];
static short gShadow[VIEW_H][VIEW_W];
static short gTitle = 1;        /* 1 while the title screen is up */
static short gQuit = 0;

/* ------------------------------------------------------------ text */

static short cLen(const char *s)
{
    short n;
    n = 0;
    while (s[n]) n++;
    return n;
}

static void pstr(unsigned char *d, const char *s)
{
    short n;
    n = 0;
    while (s[n] && n < 255) { d[n + 1] = (unsigned char)s[n]; n++; }
    d[0] = (unsigned char)n;
}

static void drawC(const char *s)
{
    DrawText((Ptr)s, 0, cLen(s));
}

static void drawCentered(const char *s, short y)
{
    short w;
    w = TextWidth((Ptr)s, 0, cLen(s));
    MoveTo((short)((WIN_W - w) / 2), y);
    drawC(s);
}

static void numToC(char *d, long v)
{
    char tmp[12];
    short i, n;
    i = 0;
    n = 0;
    do { tmp[i++] = (char)('0' + (v % 10)); v /= 10; } while (v > 0 && i < 11);
    while (i > 0) d[n++] = tmp[--i];
    d[n] = 0;
}

static void catC(char *d, const char *s)
{
    short n;
    n = cLen(d);
    while (*s) d[n++] = *s++;
    d[n] = 0;
}

/* ------------------------------------------------------------ tiles */

static void buildTiles(void)
{
    short t, r, c;
    unsigned short bits;

    for (t = 0; t < T_COUNT; t++) {
        for (r = 0; r < 16; r++) {
            bits = 0;
            for (c = 0; c < 16; c++)
                if (gTileArt[t][r][c] == '#')
                    bits = (unsigned short)(bits | (0x8000U >> c));
            gTileBits[r][t] = bits;
        }
    }
    gTiles.baseAddr = (Ptr)gTileBits;
    gTiles.rowBytes = T_COUNT * 2;
    SetRect(&gTiles.bounds, 0, 0, T_COUNT * 16, 16);
}

static void blitTile(short code, short vx, short vy)
{
    Rect src, dst;
    SetRect(&src, (short)(code * 16), 0, (short)(code * 16 + 16), 16);
    SetRect(&dst, (short)(vx * 16), (short)(vy * 16),
            (short)(vx * 16 + 16), (short)(vy * 16 + 16));
    CopyBits(&gTiles, &gWin->portBits, &src, &dst, srcCopy, 0L);
}

static void forgetScreen(void)
{
    short vx, vy;
    for (vy = 0; vy < VIEW_H; vy++)
        for (vx = 0; vx < VIEW_W; vx++)
            gShadow[vy][vx] = -1;
}

/* ------------------------------------------------------------ screens */

static void drawMap(void)
{
    short vx, vy, code;
    for (vy = 0; vy < VIEW_H; vy++) {
        for (vx = 0; vx < VIEW_W; vx++) {
            code = G_Cell(vx, vy);
            if (code != gShadow[vy][vx]) {
                gShadow[vy][vx] = code;
                blitTile(code, vx, vy);
            }
        }
    }
}

static void drawStatus(void)
{
    Rect r;
    char st[96];

    SetRect(&r, 0, MAP_H, WIN_W, WIN_H);
    EraseRect(&r);
    MoveTo(0, MAP_H);
    LineTo(WIN_W - 1, MAP_H);
    G_Status(st);
    TextFace(0);
    MoveTo(6, MAP_H + 14);
    drawC(st);
    MoveTo(6, MAP_H + 28);
    drawC(g.msg);
}

static void drawBox(short top, short bottom)
{
    Rect r;
    SetRect(&r, 64, top, WIN_W - 64, bottom);
    EraseRect(&r);
    FrameRect(&r);
    InsetRect(&r, 3, 3);
    FrameRect(&r);
}

static void drawEnd(void)
{
    char line[96], num[12];

    drawBox(96, 192);
    TextFace(1);
    drawCentered(g.state == ST_WON ? "OUT OF THE DUST" : "THE DUST TAKES YOU", 122);
    TextFace(0);
    drawCentered(g.state == ST_WON ? "Behind you, Harrow is already gone."
                                   : "Harrow keeps what it catches.", 142);
    line[0] = 0;
    catC(line, "You lasted ");
    numToC(num, g.turn);
    catC(line, num);
    catC(line, " turns.  Press Return to go back in.");
    drawCentered(line, 170);
    forgetScreen();             /* the box covered part of the map */
}

static void drawTitle(void)
{
    short vx, vy;

    for (vy = 0; vy < VIEW_H; vy++)
        for (vx = 0; vx < VIEW_W; vx++)
            blitTile((short)(T_DUST0 + ((vx * 7 + vy * 13) % 3)), vx, vy);
    drawBox(24, 264);
    TextFace(1);
    drawCentered("B L A C K   S U N D A Y", 52);
    TextFace(0);
    drawCentered("Harrow, Oklahoma.  April, 1935.", 70);
    drawCentered("The storm has not stopped in nine days. The trains still", 98);
    drawCentered("run somewhere past the edge of town. Find the depot.", 112);
    drawCentered("Stay out of the wind when you can.", 126);
    drawCentered("You are not the only thing moving in it.", 140);
    drawCentered("Arrow keys or W A S D: walk. Walk into a thing to hit it.", 170);
    drawCentered("L: lantern (you see farther, and so do they)", 184);
    drawCentered("F, then a direction: fire the shotgun", 198);
    drawCentered("R: breathe through a rag    B: bandage    Space: wait", 212);
    drawCentered("Roofs are shelter. Lungs clear indoors.", 226);
    TextFace(1);
    drawCentered("Press Return to walk out into it.", 250);
    TextFace(0);
    {
        Rect r;
        SetRect(&r, 0, MAP_H, WIN_W, WIN_H);
        EraseRect(&r);
        MoveTo(0, MAP_H);
        LineTo(WIN_W - 1, MAP_H);
    }
    forgetScreen();
}

static void drawAll(void)
{
    if (gTitle) { drawTitle(); return; }
    forgetScreen();
    drawMap();
    drawStatus();
    if (g.state != ST_PLAY) drawEnd();
}

static void startGame(void)
{
    gTitle = 0;
    G_New((u32)TickCount());
    drawAll();
}

/* ------------------------------------------------------------ events */

static void doMenu(long choice)
{
    if (HiWord(choice) == MENU_ID) {
        switch (LoWord(choice)) {
        case 1: startGame(); break;
        case 3: gQuit = 1; break;
        }
    }
    HiliteMenu(0);
}

static void doKey(short c, short mods)
{
    short cmd;

    if (mods & cmdKey) { doMenu(MenuKey((char)c)); return; }
    ObscureCursor();

    if (gTitle || g.state != ST_PLAY) {
        if (c == 13 || c == 3) startGame();     /* Return or Enter */
        return;
    }

    cmd = 0;
    switch (c) {
    case 30: case 'w': case 'W': case '8': cmd = CMD_NORTH; break;
    case 31: case 's': case 'S': case '2': cmd = CMD_SOUTH; break;
    case 29: case 'd': case 'D': case '6': cmd = CMD_EAST; break;
    case 28: case 'a': case 'A': case '4': cmd = CMD_WEST; break;
    case ' ': case '.': case '5':          cmd = CMD_WAIT; break;
    case 'l': case 'L':                    cmd = CMD_LAMP; break;
    case 'f': case 'F':                    cmd = CMD_FIRE; break;
    case 'r': case 'R':                    cmd = CMD_RAG; break;
    case 'b': case 'B':                    cmd = CMD_BANDAGE; break;
    }
    if (cmd == 0) return;

    G_Command(cmd);
    drawMap();
    drawStatus();
    if (g.state != ST_PLAY) {
        SysBeep(10);
        drawEnd();
    }
}

int main(void)
{
    EventRecord ev;
    WindowPtr w;
    Rect r;
    unsigned char ps[256];
    short left, top;

    InitGraf(&QD(thePort));
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(0L);
    InitCursor();
    FlushEvents(everyEvent, 0);

    pstr(ps, "File");
    gFileMenu = NewMenu(MENU_ID, ps);
    pstr(ps, "New Game/N;(-;Quit/Q");
    AppendMenu(gFileMenu, ps);
    InsertMenu(gFileMenu, 0);
    DrawMenuBar();

    /* On a Plus this fills the screen below the menu bar. On a bigger
       screen (an emulated Mac II, say) it is centred. */
    left = (short)((QD(screenBits).bounds.right - WIN_W) / 2);
    top = (short)(20 + (QD(screenBits).bounds.bottom - 20 - WIN_H) / 2);
    SetRect(&r, left, top, (short)(left + WIN_W), (short)(top + WIN_H));
    ps[0] = 0;
    gWin = NewWindow(0L, &r, ps, true, plainDBox, (WindowPtr)-1L, false, 0L);
    SetPort(gWin);
    TextFont(4);                /* Monaco 9: fixed width, in the Plus ROM */
    TextSize(9);

    buildTiles();
    drawTitle();

    while (!gQuit) {
        if (!GetNextEvent(everyEvent, &ev)) continue;
        switch (ev.what) {
        case keyDown:
        case autoKey:
            doKey((short)(ev.message & charCodeMask), (short)ev.modifiers);
            break;
        case mouseDown:
            if (FindWindow(ev.where, &w) == inMenuBar)
                doMenu(MenuSelect(ev.where));
            break;
        case updateEvt:
            BeginUpdate((WindowPtr)ev.message);
            drawAll();
            EndUpdate((WindowPtr)ev.message);
            break;
        }
    }
    return 0;
}
