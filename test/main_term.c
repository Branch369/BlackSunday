/* Host-side test harness for BLACK SUNDAY. Not part of the Mac build.
 *   harness art            check tile art dimensions
 *   harness reach N        BFS: is the handcar reachable for N seeds?
 *   harness fuzz N         N games of random input, checking invariants
 *   harness bot N          N games played by a simple bot; win rate
 *   harness dump SEED T    play bot T turns, dump the screen cell codes
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/game.h"

static const char GLYPH[T_COUNT + 1] = ".,=#+_^|T%~t\"-H0olsb@ZhY:;'";

static void render(void)
{
    char st[96];
    int vx, vy;
    for (vy = 0; vy < VIEW_H; vy++) {
        for (vx = 0; vx < VIEW_W; vx++) putchar(GLYPH[G_Cell((s16)vx, (s16)vy)]);
        putchar('\n');
    }
    G_Status(st);
    printf("%s\n%s\n", st, g.msg);
}

static void checkArt(void)
{
    int t, r;
    for (t = 0; t < T_COUNT; t++)
        for (r = 0; r < 16; r++)
            if (strlen(gTileArt[t][r]) != 16) {
                printf("BAD ART tile %d row %d len %d\n", t, r, (int)strlen(gTileArt[t][r]));
                exit(1);
            }
    printf("art ok: %d tiles\n", T_COUNT);
}

/* ---- BFS distance map from the handcar over a box around start+depot */
static int bx0, by0, bw, bh;
static int *dist;

static int buildDist(void)
{
    int hx = g.depotLX * LOT_W + 8, hy = g.depotLY * LOT_H + 4;
    int x0 = g.px < hx ? g.px : hx, x1 = g.px > hx ? g.px : hx;
    int y0 = g.py < hy ? g.py : hy, y1 = g.py > hy ? g.py : hy;
    int *q, head = 0, tail = 0, i;
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    bx0 = x0 - 40; by0 = y0 - 30; bw = x1 - x0 + 81; bh = y1 - y0 + 61;
    free(dist);
    dist = malloc(sizeof(int) * bw * bh);
    q = malloc(sizeof(int) * bw * bh);
    for (i = 0; i < bw * bh; i++) dist[i] = -1;
    dist[(hy - by0) * bw + (hx - bx0)] = 0;
    q[tail++] = (hy - by0) * bw + (hx - bx0);
    while (head < tail) {
        int c = q[head++], cx = c % bw, cy = c / bw, k;
        for (k = 0; k < 4; k++) {
            int nx = cx + DX[k], ny = cy + DY[k];
            if (nx < 0 || ny < 0 || nx >= bw || ny >= bh) continue;
            if (dist[ny * bw + nx] >= 0) continue;
            if (!G_Passable(G_TileAt(nx + bx0, ny + by0))) continue;
            dist[ny * bw + nx] = dist[c] + 1;
            q[tail++] = ny * bw + nx;
        }
    }
    free(q);
    return dist[(g.py - by0) * bw + (g.px - bx0)];
}

static int dAt(int x, int y)
{
    x -= bx0; y -= by0;
    if (x < 0 || y < 0 || x >= bw || y >= bh) return -1;
    return dist[y * bw + x];
}

static void invariants(unsigned seed)
{
    int i;
    if (!G_Passable(G_TileAt(g.px, g.py))) { printf("seed %u: player in solid cell\n", seed); exit(1); }
    if (g.health < 0 || g.health > MAX_HEALTH || g.lungs < 0 || g.lungs > 100 ||
        g.oil < 0 || g.shells < 0 || g.rags < 0 || g.bandages < 0) {
        printf("seed %u: bad stats\n", seed); exit(1);
    }
    if (strlen(g.msg) >= MSG_LEN) { printf("seed %u: msg overflow\n", seed); exit(1); }
    for (i = 0; i < MAX_MON; i++) {
        if (g.mon[i].kind == M_NONE) continue;
        if (!G_Passable(G_TileAt(g.mon[i].x, g.mon[i].y))) { printf("seed %u: monster in solid cell\n", seed); exit(1); }
        if (g.mon[i].x == g.px && g.mon[i].y == g.py) { printf("seed %u: monster on player\n", seed); exit(1); }
    }
    for (i = 0; i < VIEW_W * VIEW_H; i++) {
        int c = G_Cell((s16)(i % VIEW_W), (s16)(i / VIEW_W));
        if (c < 0 || c >= T_COUNT) { printf("seed %u: bad cell code %d\n", seed, c); exit(1); }
    }
}

/* ---- a simple bot: follow the BFS map, shelter when choking, fight back */
static int resting;

static int stepToward(int tx, int ty, int limit)
{
    /* local BFS from (tx,ty) to the player inside a small box */
    enum { R = 24, W = 2 * R + 1 };
    static int d[W * W], q[W * W];
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    static const int CMD[4] = {CMD_EAST, CMD_WEST, CMD_SOUTH, CMD_NORTH};
    int head = 0, tail = 0, i, k, best = -1, bd = 1 << 30;
    int ox = g.px - R, oy = g.py - R;
    (void)limit;
    for (i = 0; i < W * W; i++) d[i] = -1;
    if (abs(tx - g.px) > R || abs(ty - g.py) > R) return 0;
    d[(ty - oy) * W + (tx - ox)] = 0;
    q[tail++] = (ty - oy) * W + (tx - ox);
    while (head < tail) {
        int c = q[head++], cx = c % W, cy = c / W;
        for (k = 0; k < 4; k++) {
            int nx = cx + DX[k], ny = cy + DY[k];
            if (nx < 0 || ny < 0 || nx >= W || ny >= W || d[ny * W + nx] >= 0) continue;
            if (!G_Passable(G_TileAt(nx + ox, ny + oy))) continue;
            d[ny * W + nx] = d[c] + 1;
            q[tail++] = ny * W + nx;
        }
    }
    for (k = 0; k < 4; k++) {
        int v = d[(R + DY[k]) * W + (R + DX[k])];
        if (v >= 0 && v < bd) { bd = v; best = k; }
    }
    if (best < 0) return 0;
    G_Command((s16)CMD[best]);
    return 1;
}

static int findShelter(int *sx, int *sy)
{
    int r, x, y;
    for (r = 1; r <= 14; r++)
        for (y = g.py - r; y <= g.py + r; y++)
            for (x = g.px - r; x <= g.px + r; x++) {
                int t;
                if (abs(x - g.px) != r && abs(y - g.py) != r) continue;
                t = G_TileAt(x, y);
                if (t == T_FLOOR) { *sx = x; *sy = y; return 1; }
            }
    return 0;
}

static void botTurn(void)
{
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    static const int CMD[4] = {CMD_EAST, CMD_WEST, CMD_SOUTH, CMD_NORTH};
    int i, k, best = -1, bd = 1 << 30, sx, sy;

    if (g.health <= 2 && g.bandages > 0) { G_Command(CMD_BANDAGE); return; }
    if (g.lungs >= 70 && g.rags > 0) { G_Command(CMD_RAG); return; }

    /* shoot anything lined up within 5 cells */
    if (g.shells > 0) {
        for (i = 0; i < MAX_MON; i++) {
            int dx, dy;
            if (g.mon[i].kind == M_NONE) continue;
            dx = g.mon[i].x - g.px; dy = g.mon[i].y - g.py;
            if ((dx == 0 && abs(dy) <= 5 && abs(dy) >= 2) || (dy == 0 && abs(dx) <= 5 && abs(dx) >= 2)) {
                G_Command(CMD_FIRE);
                G_Command((s16)(dx > 0 ? CMD_EAST : dx < 0 ? CMD_WEST : dy > 0 ? CMD_SOUTH : CMD_NORTH));
                return;
            }
        }
    }
    /* hit anything adjacent */
    for (i = 0; i < MAX_MON; i++) {
        int dx, dy;
        if (g.mon[i].kind == M_NONE) continue;
        dx = g.mon[i].x - g.px; dy = g.mon[i].y - g.py;
        if (abs(dx) + abs(dy) == 1) {
            G_Command((s16)(dx > 0 ? CMD_EAST : dx < 0 ? CMD_WEST : dy > 0 ? CMD_SOUTH : CMD_NORTH));
            return;
        }
    }
    if (g.indoors && resting) {
        if (g.lungs <= 4) resting = 0;
        else { G_Command(CMD_WAIT); return; }
    }
    if (g.lungs >= 60 && !g.indoors && findShelter(&sx, &sy)) {
        resting = 1;
        if (stepToward(sx, sy, 0)) return;
    }
    if (g.indoors && g.lungs >= 30 && G_TileAt(g.px, g.py) != T_DOOR) { resting = 1; G_Command(CMD_WAIT); return; }

    for (k = 0; k < 4; k++) {
        int v = dAt(g.px + DX[k], g.py + DY[k]);
        if (v >= 0 && v < bd) { bd = v; best = k; }
    }
    if (best < 0) { G_Command(CMD_WAIT); return; }
    G_Command((s16)CMD[best]);
}

/* Mirrors the front end's drawMap: redraw only last turn's box and this
   turn's box, then check the result against a full scan of the screen. */
static short shadow[VIEW_H][VIEW_W];
static s16 pb[4];
static s32 sVX, sVY;
static int haveShadow;

static void incDraw(unsigned seed)
{
    s16 n[4], x0, y0, x1, y1, vx, vy;
    int full = !haveShadow || sVX != g.viewX || sVY != g.viewY;
    G_DirtyBox(&n[0], &n[1], &n[2], &n[3]);
    if (full) { x0 = 0; y0 = 0; x1 = VIEW_W - 1; y1 = VIEW_H - 1; }
    else {
        x0 = n[0] < pb[0] ? n[0] : pb[0]; y0 = n[1] < pb[1] ? n[1] : pb[1];
        x1 = n[2] > pb[2] ? n[2] : pb[2]; y1 = n[3] > pb[3] ? n[3] : pb[3];
    }
    memcpy(pb, n, sizeof pb); sVX = g.viewX; sVY = g.viewY; haveShadow = 1;
    for (vy = y0; vy <= y1; vy++) for (vx = x0; vx <= x1; vx++) shadow[vy][vx] = G_Cell(vx, vy);
    for (vy = 0; vy < VIEW_H; vy++) for (vx = 0; vx < VIEW_W; vx++)
        if (shadow[vy][vx] != G_Cell(vx, vy)) {
            printf("seed %u turn %d: stale cell %d,%d (shown %d, should be %d)\n",
                   seed, (int)g.turn, vx, vy, shadow[vy][vx], G_Cell(vx, vy));
            exit(1);
        }
}

int main(int argc, char **argv)
{
    int n, i, t;
    if (argc < 2) return 1;
    n = argc > 2 ? atoi(argv[2]) : 100;

    if (!strcmp(argv[1], "art")) { checkArt(); return 0; }

    if (!strcmp(argv[1], "reach")) {
        int ok = 0, lo = 1 << 30, hi = 0; long sum = 0;
        for (i = 1; i <= n; i++) {
            int d;
            G_New((u32)i * 2654435761u);
            d = buildDist();
            if (d > 0) { ok++; sum += d; if (d < lo) lo = d; if (d > hi) hi = d; }
            else printf("seed %d unreachable\n", i);
        }
        printf("reachable %d/%d, path length min %d avg %ld max %d\n", ok, n, lo, ok ? sum / ok : 0, hi);
        return 0;
    }

    if (!strcmp(argv[1], "fuzz")) {
        int dead = 0, won = 0;
        srand(12345);
        for (i = 1; i <= n; i++) {
            G_New((u32)i * 40503u + 7u);
            invariants(i);
            haveShadow = 0; incDraw(i);
            for (t = 0; t < 4000 && g.state == ST_PLAY; t++) {
                G_Command((s16)(1 + rand() % 9));
                invariants(i);
                incDraw(i);
            }
            if (g.state == ST_DEAD) dead++;
            if (g.state == ST_WON) won++;
        }
        printf("fuzz: %d games, %d dead, %d won, no invariant failures\n", n, dead, won);
        return 0;
    }

    if (!strcmp(argv[1], "bot")) {
        int won = 0; long turns = 0, wturns = 0;
        for (i = 1; i <= n; i++) {
            G_New((u32)i * 7919u + 13u);
            buildDist();
            resting = 0;
            haveShadow = 0; incDraw(i);
            for (t = 0; t < 5000 && g.state == ST_PLAY; t++) { botTurn(); invariants(i); incDraw(i); }
            turns += g.turn;
            if (g.state == ST_WON) { won++; wturns += g.turn; }
        }
        printf("bot: won %d/%d, avg turns %ld, avg winning turns %ld\n", won, n, turns / n, won ? wturns / won : 0);
        return 0;
    }

    if (!strcmp(argv[1], "dump")) {
        int vx, vy, T = argc > 3 ? atoi(argv[3]) : 0;
        G_New((u32)n);
        buildDist();
        resting = 0;
        for (t = 0; t < T && g.state == ST_PLAY; t++) botTurn();
        if (argc > 4) { render(); return 0; }
        for (vy = 0; vy < VIEW_H; vy++) {
            for (vx = 0; vx < VIEW_W; vx++) printf("%d ", G_Cell((s16)vx, (s16)vy));
            printf("\n");
        }
        { char st[96]; G_Status(st); printf("%s\n%s\n", st, g.msg); }
        return 0;
    }
    return 1;
}
