/*
 * BLACK SUNDAY - game.c: world generation, rules, monsters.
 * No Toolbox calls and no C library calls, so it links with nothing extra.
 *
 * The town is endless. Nothing about it is stored: every cell is worked out
 * from the seed and its coordinates each time it is needed, so memory use
 * stays flat no matter how far the player walks.
 */
#include "game.h"

Game g;

#define L_HOUSE  0
#define L_STORE  1
#define L_FIELD  2
#define L_EMPTY  3
#define L_GRAVES 4
#define L_DRIFTS 5
#define L_DEPOT  6

typedef struct {
    s32 lx, ly;
    u32 h;
    s16 type;
    s16 bx0, by0, bx1, by1;     /* building, in lot-local cells */
    s16 doorX, doorY;
    s16 valid;
} Lot;

static Lot lotCache;

/* ------------------------------------------------------------ helpers */

static u32 rnd(void)
{
    g.rng = g.rng * (u32)1664525UL + (u32)1013904223UL;
    return (g.rng >> 8) & (u32)0xFFFFFFUL;
}

static u32 hash3(s32 a, s32 b, u32 c)
{
    u32 h;
    h = g.seed + c * (u32)0x9E3779B1UL;
    h ^= (u32)a * (u32)0x85EBCA6BUL;
    h = (h << 13) | (h >> 19);
    h ^= (u32)b * (u32)0xC2B2AE35UL;
    h ^= h >> 15;
    h *= (u32)0x2C1B3C6DUL;
    h ^= h >> 13;
    return h;
}

static s32 fdiv(s32 a, s32 b)       /* division that rounds down */
{
    s32 q;
    q = a / b;
    if ((a % b) != 0 && a < 0) q--;
    return q;
}

static s32 labs32(s32 v) { return v < 0 ? -v : v; }
static s16 sgn(s32 v)    { return (s16)(v < 0 ? -1 : (v > 0 ? 1 : 0)); }

static s16 sLen(const char *s)
{
    s16 n;
    n = 0;
    while (s[n]) n++;
    return n;
}

static void sCat(char *d, const char *s, s16 max)
{
    s16 n;
    n = sLen(d);
    while (*s && n < max - 1) d[n++] = *s++;
    d[n] = 0;
}

static void sNum(char *d, s32 v, s16 max)
{
    char tmp[12];
    s16 i;
    i = 0;
    if (v < 0) { sCat(d, "-", max); v = -v; }
    do { tmp[i++] = (char)('0' + (v % 10)); v /= 10; } while (v > 0 && i < 11);
    while (i > 0) {
        char one[2];
        one[0] = tmp[--i];
        one[1] = 0;
        sCat(d, one, max);
    }
}

static void say(const char *s)
{
    if (g.msg[0]) sCat(g.msg, " ", MSG_LEN);
    sCat(g.msg, s, MSG_LEN);
}

/* ------------------------------------------------------------ the town */

static void getLot(s32 lx, s32 ly, Lot *L)
{
    u32 h;
    s16 r, w, ht;

    if (lotCache.valid && lotCache.lx == lx && lotCache.ly == ly) {
        *L = lotCache;
        return;
    }
    h = hash3(lx, ly, (u32)1);
    L->lx = lx; L->ly = ly; L->h = h; L->valid = 1;
    L->bx0 = L->by0 = L->bx1 = L->by1 = 0;
    L->doorX = L->doorY = -9;

    r = (s16)((h >> 8) % 16);
    if (lx == g.depotLX && ly == g.depotLY) L->type = L_DEPOT;
    else if (lx == 0 && ly == 0) L->type = L_HOUSE;     /* where you wake up */
    else if (r < 7)  L->type = L_HOUSE;
    else if (r < 9)  L->type = L_STORE;
    else if (r < 11) L->type = L_FIELD;
    else if (r < 13) L->type = L_EMPTY;
    else if (r < 14) L->type = L_GRAVES;
    else             L->type = L_DRIFTS;

    if (L->type == L_HOUSE || L->type == L_STORE) {
        if (L->type == L_HOUSE) {
            w  = (s16)(5 + (h >> 12) % 5);      /* 5..9  */
            ht = (s16)(4 + (h >> 15) % 3);      /* 4..6  */
        } else {
            w  = (s16)(10 + (h >> 12) % 3);     /* 10..12 */
            ht = (s16)(5 + (h >> 15) % 2);      /* 5..6   */
        }
        /* Buildings stay inside cells 2..14 by 2..7, leaving a clear ring. */
        L->bx0 = (s16)(2 + (h >> 17) % (u32)(14 - w));
        L->by0 = (s16)(2 + (h >> 20) % (u32)(7 - ht));
        L->bx1 = (s16)(L->bx0 + w - 1);
        L->by1 = (s16)(L->by0 + ht - 1);
        switch ((s16)((h >> 23) & 3)) {
        case 0:
            L->doorX = (s16)(L->bx0 + 1 + (h >> 25) % (u32)(w - 2));
            L->doorY = L->by0;
            break;
        case 1:
            L->doorX = (s16)(L->bx0 + 1 + (h >> 25) % (u32)(w - 2));
            L->doorY = L->by1;
            break;
        case 2:
            L->doorX = L->bx0;
            L->doorY = (s16)(L->by0 + 1 + (h >> 25) % (u32)(ht - 2));
            break;
        default:
            L->doorX = L->bx1;
            L->doorY = (s16)(L->by0 + 1 + (h >> 25) % (u32)(ht - 2));
            break;
        }
    }
    lotCache = *L;
}

static s16 baseTile(s32 x, s32 y)
{
    Lot L;
    s32 lx, ly;
    s16 ox, oy, k, ground, dd;
    u32 hc;

    lx = fdiv(x, LOT_W);
    ly = fdiv(y, LOT_H);
    ox = (s16)(x - lx * LOT_W);
    oy = (s16)(y - ly * LOT_H);
    hc = hash3(x, y, (u32)7);
    k = (s16)((hc >> 8) % 24);
    ground = (s16)((((hc >> 4) & 3) == 0) ? T_GROUND2 : T_GROUND);

    /* Roads run along the west and north edge of every lot. */
    if (ox == 0 || oy == 0) {
        if (lx == 0 && ly == 0) return T_ROAD;
        if (((hc >> 16) & 31) == 0) return T_WRECK;
        if (((hc >> 16) & 31) == 1) return T_DRIFT;
        return T_ROAD;
    }
    /* The ring just inside the roads is always open ground. A blocked road
       can always be walked around, so the depot can always be reached. */
    if (ox == 1 || ox == LOT_W - 1 || oy == 1 || oy == LOT_H - 1) return ground;

    getLot(lx, ly, &L);
    switch (L.type) {
    case L_HOUSE:
    case L_STORE:
        if (ox >= L.bx0 && ox <= L.bx1 && oy >= L.by0 && oy <= L.by1) {
            if (ox == L.bx0 || ox == L.bx1 || oy == L.by0 || oy == L.by1)
                return (s16)((ox == L.doorX && oy == L.doorY) ? T_DOOR : T_WALL);
            return T_FLOOR;
        }
        if (lx == 0 && ly == 0) return ground;
        dd = (s16)((ox > L.doorX ? ox - L.doorX : L.doorX - ox) +
                   (oy > L.doorY ? oy - L.doorY : L.doorY - oy));
        if (dd == 1) return ground;             /* keep the doorstep clear */
        if (k == 0) return T_TREE;
        if (k == 1) return T_FENCE;
        if (k == 2 && ((hc >> 20) & 3) == 0) return T_WELL;
        return ground;
    case L_FIELD:
        if (ox == 2 || ox == 14 || oy == 2 || oy == 7)
            return (s16)((k % 4 == 0) ? ground : T_FENCE);
        if ((oy & 1) == 0) return T_CROP;
        return ground;
    case L_EMPTY:
        if (k == 0 || k == 12) return T_TREE;
        if (k == 1) return T_WRECK;
        if (k == 2) return T_DRIFT;
        return ground;
    case L_GRAVES:
        if (ox % 3 == 2 && (oy == 3 || oy == 5 || oy == 7)) return T_GRAVE;
        if (k == 0) return T_TREE;
        return ground;
    case L_DRIFTS:
        if (k % 4 == 0) return T_DRIFT;
        return ground;
    case L_DEPOT:
        if (ox == 8 && oy == 4) return T_HANDCAR;
        if (ox >= 3 && ox <= 13 && oy >= 3 && oy <= 6) return T_PLATFORM;
        return ground;
    }
    return ground;
}

static s16 isTaken(s32 x, s32 y)
{
    s16 i;
    for (i = 0; i < g.takenN; i++)
        if (g.takenX[i] == x && g.takenY[i] == y) return 1;
    return 0;
}

static void markTaken(s32 x, s32 y)
{
    g.takenX[g.takenHead] = x;
    g.takenY[g.takenHead] = y;
    g.takenHead = (s16)((g.takenHead + 1) % MAX_TAKEN);
    if (g.takenN < MAX_TAKEN) g.takenN++;
}

s16 G_TileAt(s32 x, s32 y)
{
    Lot L;
    s16 t, r, scale;

    t = baseTile(x, y);
    if (t != T_FLOOR) return t;
    getLot(fdiv(x, LOT_W), fdiv(y, LOT_H), &L);
    scale = (s16)(L.type == L_STORE ? 2 : 1);
    r = (s16)((hash3(x, y, (u32)11) >> 8) % 100);
    if (r >= 8 * scale) return T_FLOOR;
    if (isTaken(x, y)) return T_FLOOR;
    if (r < 3 * scale) return T_OIL;
    if (r < 5 * scale) return T_RAG;
    if (r < 7 * scale) return T_SHELLS;
    return T_BANDAGE;
}

s16 G_Passable(s16 t)
{
    switch (t) {
    case T_GROUND: case T_GROUND2: case T_ROAD: case T_DOOR: case T_FLOOR:
    case T_CROP: case T_PLATFORM: case T_HANDCAR:
    case T_OIL: case T_RAG: case T_SHELLS: case T_BANDAGE:
        return 1;
    }
    return 0;
}

static s16 isInside(s16 t)
{
    return (s16)(t == T_FLOOR || t == T_OIL || t == T_RAG ||
                 t == T_SHELLS || t == T_BANDAGE);
}

/* ------------------------------------------------------------ monsters */

static s16 monAt(s32 x, s32 y)
{
    s16 i;
    for (i = 0; i < MAX_MON; i++)
        if (g.mon[i].kind != M_NONE && g.mon[i].x == x && g.mon[i].y == y) return i;
    return -1;
}

static s16 addMon(s16 kind, s32 x, s32 y)
{
    s16 i;
    if (monAt(x, y) >= 0) return -1;
    if (x == g.px && y == g.py) return -1;
    if (!G_Passable(G_TileAt(x, y))) return -1;
    for (i = 0; i < MAX_MON; i++) {
        if (g.mon[i].kind == M_NONE) {
            g.mon[i].kind = kind;
            g.mon[i].x = x;
            g.mon[i].y = y;
            g.mon[i].hp = (s16)(kind == M_HUSK ? 2 : (kind == M_HOUND ? 1 : 3));
            return i;
        }
    }
    return -1;
}

static s16 monCanEnter(Mon *m, s32 x, s32 y)
{
    s16 t;
    if (x == g.px && y == g.py) return 0;
    if (monAt(x, y) >= 0) return 0;
    t = G_TileAt(x, y);
    if (!G_Passable(t)) return 0;
    if (m->kind == M_HOUND && (t == T_DOOR || isInside(t))) return 0;
    return 1;
}

static s16 monTry(Mon *m, s16 dx, s16 dy)
{
    if (dx == 0 && dy == 0) return 0;
    if (!monCanEnter(m, m->x + dx, m->y + dy)) return 0;
    m->x += dx;
    m->y += dy;
    return 1;
}

static void monStepToward(Mon *m)
{
    s32 dx, dy;
    s16 sx, sy;
    dx = g.px - m->x;
    dy = g.py - m->y;
    sx = sgn(dx);
    sy = sgn(dy);
    if (labs32(dx) >= labs32(dy)) {
        if (monTry(m, sx, 0)) return;
        if (monTry(m, 0, sy)) return;
        if (sy == 0) monTry(m, 0, (s16)((rnd() & 1) ? 1 : -1));
    } else {
        if (monTry(m, 0, sy)) return;
        if (monTry(m, sx, 0)) return;
        if (sx == 0) monTry(m, (s16)((rnd() & 1) ? 1 : -1), 0);
    }
}

static void monWander(Mon *m)
{
    switch ((s16)(rnd() % 4)) {
    case 0: monTry(m, 1, 0); break;
    case 1: monTry(m, -1, 0); break;
    case 2: monTry(m, 0, 1); break;
    default: monTry(m, 0, -1); break;
    }
}

static s16 nextTo(Mon *m)
{
    return (s16)(labs32(m->x - g.px) + labs32(m->y - g.py) == 1);
}

static void hurt(s16 n, const char *why)
{
    g.health = (s16)(g.health - n);
    say(why);
}

static s16 cellVisible(s32 x, s32 y)
{
    s32 dx, dy, d2;
    dx = x - g.px;
    dy = y - g.py;
    if (labs32(dx) > 40 || labs32(dy) > 40) return 0;
    d2 = dx * dx + dy * dy;
    if (g.indoors) {
        if (x >= g.inX0 && x <= g.inX1 && y >= g.inY0 && y <= g.inY1) return 1;
        return (s16)(d2 <= 2);
    }
    return (s16)(d2 <= (s32)g.vis * g.vis + g.vis);
}

static void monstersAct(void)
{
    s16 i, hear, step, struck, seen;
    s32 dist, vx, vy;
    Mon *m;

    for (i = 0; i < MAX_MON; i++) {
        m = &g.mon[i];
        if (m->kind == M_NONE) continue;
        vx = m->x - g.px;
        vy = m->y - g.py;
        dist = labs32(vx) > labs32(vy) ? labs32(vx) : labs32(vy);
        if (dist > 40) { m->kind = M_NONE; continue; }      /* lost in the storm */

        switch (m->kind) {
        case M_HUSK:
            if (nextTo(m)) { hurt(1, "A husk rakes at you."); break; }
            if (((g.turn + i) & 1) != 0) break;             /* husks are slow */
            hear = 7;
            if (g.lamp) hear = (s16)(hear + 4);
            if (g.noise > 0) hear = (s16)(hear + 9);
            if (dist <= hear) monStepToward(m);
            else if (rnd() % 3 == 0) monWander(m);
            break;

        case M_HOUND:
            struck = 0;
            for (step = 0; step < 2; step++) {
                if (nextTo(m)) {
                    if (!struck) { hurt(1, "Teeth out of the dust."); struck = 1; }
                } else if (dist <= 12 && !g.indoors) {
                    monStepToward(m);
                } else {
                    monWander(m);
                }
            }
            if (g.indoors && dist <= 4 && rnd() % 6 == 0)
                say("Something paces outside the walls.");
            break;

        case M_CROW:
            /* It holds still while you can see it and are facing it. */
            seen = (s16)(cellVisible(m->x, m->y) &&
                         (vx * g.fx + vy * g.fy) > 0);
            if (seen) break;
            if (nextTo(m)) { hurt(2, "Straw fingers close on your throat."); break; }
            monStepToward(m);
            if (!cellVisible(m->x, m->y) && !nextTo(m)) monStepToward(m);
            break;
        }
    }
}

static void spawnWild(void)
{
    s16 tries, lim, dx, dy, t, r, kind;
    s32 x, y;

    lim = (s16)(g.vis + 6);
    for (tries = 0; tries < 10; tries++) {
        dx = (s16)((s16)(rnd() % (u32)(2 * lim + 1)) - lim);
        dy = (s16)((s16)(rnd() % (u32)(2 * lim + 1)) - lim);
        if (dx * dx + dy * dy <= (g.vis + 2) * (g.vis + 2)) continue;
        x = g.px + dx;
        y = g.py + dy;
        t = baseTile(x, y);
        if (t != T_GROUND && t != T_GROUND2 && t != T_ROAD && t != T_CROP) continue;
        r = (s16)(rnd() % 100);
        kind = (s16)(r < 55 ? M_HUSK : (r < 85 ? M_HOUND : M_CROW));
        if (addMon(kind, x, y) >= 0) {
            if (kind == M_HOUND) say("Something howls inside the wind.");
        }
        return;
    }
}

static s16 lotSeen(s32 lx, s32 ly)
{
    s16 i;
    for (i = 0; i < g.seenN; i++)
        if (g.seenLX[i] == lx && g.seenLY[i] == ly) return 1;
    return 0;
}

static void markLot(s32 lx, s32 ly)
{
    g.seenLX[g.seenHead] = lx;
    g.seenLY[g.seenHead] = ly;
    g.seenHead = (s16)((g.seenHead + 1) % MAX_SEEN);
    if (g.seenN < MAX_SEEN) g.seenN++;
}

static void enterLot(void)
{
    Lot L;
    s32 lx, ly;
    s16 roll;

    lx = fdiv(g.px, LOT_W);
    ly = fdiv(g.py, LOT_H);
    if (lotSeen(lx, ly)) return;
    markLot(lx, ly);
    getLot(lx, ly, &L);
    roll = (s16)((L.h >> 27) & 3);
    if ((L.type == L_HOUSE || L.type == L_STORE) && roll == 0) {
        /* Something was already living in this one. */
        addMon(M_HUSK, lx * LOT_W + (L.bx0 + L.bx1) / 2, ly * LOT_H + (L.by0 + L.by1) / 2);
    } else if (L.type == L_FIELD && roll < 2) {
        addMon(M_CROW, lx * LOT_W + 8, ly * LOT_H + 5);
    } else if (L.type == L_GRAVES && roll < 2) {
        addMon(M_HUSK, lx * LOT_W + 7, ly * LOT_H + 4);
        say("The graves here are open.");
    }
}

/* ------------------------------------------------------------ turns */

static const char *depotDir(void)
{
    s32 dx, dy, ax, ay;
    dx = (g.depotLX * LOT_W + 8) - g.px;
    dy = (g.depotLY * LOT_H + 4) - g.py;
    ax = labs32(dx);
    ay = labs32(dy);
    if (ax > 2 * ay) return dx > 0 ? "east" : "west";
    if (ay > 2 * ax) return dy > 0 ? "south" : "north";
    if (dx > 0) return dy > 0 ? "south-east" : "north-east";
    return dy > 0 ? "south-west" : "north-west";
}

static void whistle(void)
{
    say("A train whistle, far off to the");
    say(depotDir());
    sCat(g.msg, ".", MSG_LEN);
}

static void refresh(void)
{
    Lot L;
    s16 t;

    t = baseTile(g.px, g.py);
    g.indoors = (s16)(t == T_FLOOR || t == T_DOOR);
    if (g.indoors) {
        getLot(fdiv(g.px, LOT_W), fdiv(g.py, LOT_H), &L);
        g.inX0 = L.lx * LOT_W + L.bx0;
        g.inX1 = L.lx * LOT_W + L.bx1;
        g.inY0 = L.ly * LOT_H + L.by0;
        g.inY1 = L.ly * LOT_H + L.by1;
    }
    g.viewX = fdiv(g.px, VIEW_W) * VIEW_W;
    g.viewY = fdiv(g.py, VIEW_H) * VIEW_H;
}

static void endTurn(void)
{
    s16 chance;

    g.turn++;
    refresh();
    enterLot();

    g.storm = (s16)(g.turn / 180);
    if (g.storm > 3) g.storm = 3;

    if (g.indoors) {
        g.lungs = (s16)(g.lungs - 4);
        if (g.lungs < 0) g.lungs = 0;
    } else {
        g.lungs = (s16)(g.lungs + (g.storm >= 2 ? 3 : 2));
        if (g.lungs >= 100) {
            g.lungs = 100;
            if (g.turn & 1) hurt(1, "You are choking on dust.");
        } else if (g.lungs >= 75 && g.turn % 4 == 0) {
            say("You cough up mud.");
        }
    }

    if (g.lamp) {
        if ((g.turn & 1) == 0) g.oil--;
        if (g.oil <= 0) {
            g.oil = 0;
            g.lamp = 0;
            say("The lantern gutters out.");
        }
    }

    g.vis = (s16)(4 - (g.storm + 1) / 2);
    if (rnd() % 6 == 0) g.vis--;                /* a gust */
    if (g.lamp) g.vis = (s16)(g.vis + 2);
    if (g.vis < 1) g.vis = 1;

    if (g.noise > 0) g.noise--;

    if (!g.indoors) {
        chance = (s16)(4 + g.storm * 3 + (g.noise > 0 ? 6 : 0));
        if ((s16)(rnd() % 100) < chance) spawnWild();
    }

    monstersAct();

    if (g.health <= 0) {
        g.health = 0;
        g.state = ST_DEAD;
        return;
    }
    if (g.turn % 25 == 0 && g.msg[0] == 0) whistle();
}

static void pickUp(void)
{
    s16 t;
    t = G_TileAt(g.px, g.py);
    switch (t) {
    case T_OIL:     g.oil = (s16)(g.oil + 12); say("Lamp oil."); break;
    case T_RAG:     g.rags++;                  say("A clean rag."); break;
    case T_SHELLS:  g.shells = (s16)(g.shells + 2); say("Two shotgun shells."); break;
    case T_BANDAGE: g.bandages++;              say("A roll of bandage."); break;
    default: return;
    }
    markTaken(g.px, g.py);
}

static void tryMove(s16 dx, s16 dy)
{
    s32 nx, ny;
    s16 mi, t;

    g.fx = dx;
    g.fy = dy;
    nx = g.px + dx;
    ny = g.py + dy;
    mi = monAt(nx, ny);
    if (mi >= 0) {
        g.mon[mi].hp--;
        if (g.mon[mi].hp <= 0) {
            g.mon[mi].kind = M_NONE;
            say("You beat it down with the tire iron.");
        } else {
            say("You swing the tire iron. It does not stop.");
        }
        endTurn();
        return;
    }
    t = G_TileAt(nx, ny);
    if (!G_Passable(t)) {
        /* Turning to face something costs no time. */
        return;
    }
    g.px = nx;
    g.py = ny;
    pickUp();
    if (t == T_HANDCAR) {
        g.state = ST_WON;
        g.msg[0] = 0;
        say("You pump the handcar until the sky ahead turns blue.");
        refresh();
        return;
    }
    endTurn();
}

static void fire(s16 dx, s16 dy)
{
    s32 x, y;
    s16 i, mi, t;

    g.fx = dx;
    g.fy = dy;
    g.shells--;
    g.noise = 10;
    say("The shotgun roars.");
    x = g.px;
    y = g.py;
    for (i = 0; i < 6; i++) {
        x += dx;
        y += dy;
        mi = monAt(x, y);
        if (mi >= 0) {
            g.mon[mi].hp = (s16)(g.mon[mi].hp - 2);
            if (g.mon[mi].hp <= 0) {
                g.mon[mi].kind = M_NONE;
                say("Something comes apart in the dust.");
            } else {
                say("It staggers, and straightens.");
            }
            return;
        }
        t = baseTile(x, y);
        if (t == T_WALL || t == T_DRIFT || t == T_WRECK || t == T_TREE) return;
    }
}

void G_Command(s16 cmd)
{
    if (g.state != ST_PLAY) return;
    g.msg[0] = 0;

    if (g.aiming) {
        g.aiming = 0;
        switch (cmd) {
        case CMD_NORTH: fire(0, -1); endTurn(); return;
        case CMD_SOUTH: fire(0, 1);  endTurn(); return;
        case CMD_EAST:  fire(1, 0);  endTurn(); return;
        case CMD_WEST:  fire(-1, 0); endTurn(); return;
        }
        say("You lower the gun.");
        return;
    }

    switch (cmd) {
    case CMD_NORTH: tryMove(0, -1); break;
    case CMD_SOUTH: tryMove(0, 1);  break;
    case CMD_EAST:  tryMove(1, 0);  break;
    case CMD_WEST:  tryMove(-1, 0); break;
    case CMD_WAIT:  endTurn(); break;
    case CMD_LAMP:
        if (g.lamp) { g.lamp = 0; say("You shutter the lantern."); }
        else if (g.oil <= 0) say("The lantern is dry.");
        else { g.lamp = 1; say("The lantern pushes the dust back. Things will see it."); }
        break;
    case CMD_FIRE:
        if (g.shells <= 0) say("No shells.");
        else { g.aiming = 1; say("Fire which way?"); }
        break;
    case CMD_RAG:
        if (g.rags <= 0) { say("No clean rags."); break; }
        g.rags--;
        g.lungs = (s16)(g.lungs - 50);
        if (g.lungs < 0) g.lungs = 0;
        say("You breathe through a clean rag.");
        endTurn();
        break;
    case CMD_BANDAGE:
        if (g.bandages <= 0) { say("No bandages."); break; }
        if (g.health >= MAX_HEALTH) { say("You are not hurt."); break; }
        g.bandages--;
        g.health = (s16)(g.health + 2);
        if (g.health > MAX_HEALTH) g.health = MAX_HEALTH;
        say("You bind the wound.");
        endTurn();
        break;
    }
}

void G_New(u32 seed)
{
    Lot L;
    char *p;
    s32 n;
    s16 side, d, o;

    p = (char *)&g;
    for (n = 0; n < (s32)sizeof(Game); n++) p[n] = 0;
    lotCache.valid = 0;

    g.seed = seed;
    g.rng = seed ^ (u32)0xA5A5A5A5UL;

    /* The depot is five or six blocks out, in a direction that changes. */
    side = (s16)(rnd() % 4);
    d = (s16)(5 + rnd() % 2);
    o = (s16)((s16)(rnd() % (u32)(2 * d + 1)) - d);
    switch (side) {
    case 0:  g.depotLX = d;      g.depotLY = o;      break;
    case 1:  g.depotLX = -d;     g.depotLY = o;      break;
    case 2:  g.depotLX = o / 2;  g.depotLY = 2 * d;  break;
    default: g.depotLX = o / 2;  g.depotLY = -2 * d; break;
    }

    getLot(0, 0, &L);
    g.px = (L.bx0 + L.bx1) / 2;
    g.py = (L.by0 + L.by1) / 2;
    g.fx = 0;
    g.fy = 1;
    g.health = MAX_HEALTH;
    g.oil = 20;
    g.shells = 2;
    g.rags = 1;
    g.vis = 4;
    g.state = ST_PLAY;
    markLot(0, 0);
    refresh();
    say("Nine days of dust.");
    whistle();
}

/* ------------------------------------------------------------ drawing */

s16 G_Cell(s16 vx, s16 vy)
{
    s32 x, y;
    s16 t, mi;
    u16 u;

    x = g.viewX + vx;
    y = g.viewY + vy;
    if (x == g.px && y == g.py) return T_PLAYER;

    if (!cellVisible(x, y)) {
        /* Dust. A cheap 16-bit mix picks one of three textures, and a few
           cells change texture each turn so the storm never sits still. */
        u = (u16)((u16)x * 31u + (u16)y * 17u);
        u = (u16)(u ^ (u >> 5));
        if (((u >> 4) & 15) == (u16)(g.turn & 15)) u++;
        return (s16)(T_DUST0 + u % 3);
    }

    t = G_TileAt(x, y);
    if (!g.indoors && isInside(t)) return T_ROOF;   /* roofs hide what is inside */
    mi = monAt(x, y);
    if (mi >= 0) return (s16)(T_HUSK + g.mon[mi].kind - 1);
    return t;
}

void G_Status(char *buf)
{
    buf[0] = 0;
    sCat(buf, "HEALTH ", 80);   sNum(buf, g.health, 80);
    sCat(buf, "  LUNGS ", 80);  sNum(buf, g.lungs, 80);
    sCat(buf, "%  OIL ", 80);   sNum(buf, g.oil, 80);
    sCat(buf, "  SHELLS ", 80); sNum(buf, g.shells, 80);
    sCat(buf, "  RAGS ", 80);   sNum(buf, g.rags, 80);
    sCat(buf, "  BANDAGES ", 80); sNum(buf, g.bandages, 80);
    sCat(buf, g.lamp ? "  LAMP ON" : "  LAMP OFF", 80);
    sCat(buf, g.indoors ? "  (sheltered)" : "", 80);
}
