/*
 * SECOND METHOD, independent of stilllife.c, for counting the same strict still lifes.
 *
 *   crosscheck B36/S23 n
 *
 * All it shares with stilllife.c is the idea of searching inside boxes; the rest is done another way:
 *  - the box is a matrix of squares (not bits) and is decided square by square in COLUMN order
 *    (top to bottom, then left to right), with boxes of width >= height;
 *  - symmetries are not handled by looking for "the smallest": every pattern is reduced to its
 *    canonical form (the smallest of its 8 transforms as a sorted list of cells) and stored in a hash
 *    table; the distinct forms are counted;
 *  - strictness is checked by brute force: the islands (8-connected components) are found with a
 *    breadth-first search, and EVERY split of the islands into two or more groups is tried, simulating
 *    one generation of each group on a separate board.
 * Pruning it shares (a fact, not a heuristic): never two empty columns in a row inside the box, nor two
 * empty rows in a row (checked at the end), because then the pattern falls into two halves that do not
 * touch and it is not strict. That gives a bound: k remaining columns, with no two empty in a row and
 * the last one non-empty, hold at least (k + 1) / 2 cells.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXS 64
static unsigned BIRTH, SURV;
static int N, BW, BH;                       /* box: BW columns, BH rows, BW >= BH */
/* Board with two rings of zeros: AT(a, x, y) is valid for x, y from -1 to MAXS+2. The box is
 * 1..BW x 1..BH, ring 0 / BW+1 is the border that gets checked and ring -1 / BW+2 is only there for
 * counting neighbours. */
#define STRIDE (MAXS + 4)
#define AT(a, x, y) (a)[((x) + 1) * STRIDE + (y) + 1]
static unsigned char g[STRIDE * STRIDE], sub[STRIDE * STRIDE];

static int nb(const unsigned char * a, int x, int y) {
    int c = 0;
    for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
            if ((dx || dy) && AT(a, x + dx, y + dy)) c++;
    return c;
}

/* Is square (x, y) (coordinates including the border, 0..BW+1 x 0..BH+1) stable on board a? */
static int ok(const unsigned char * a, int x, int y) {
    int c = nb(a, x, y);
    return AT(a, x, y) ? (SURV >> c) & 1 : !((BIRTH >> c) & 1);
}

/* ---------------------------------------------------------------- canonical form and table */

typedef struct { int n; unsigned char c[MAXS][2]; } Shape;

static int cmp2(const void * a, const void * b) {
    const unsigned char * p = a, * q = b;
    return p[0] != q[0] ? p[0] - q[0] : p[1] - q[1];
}

static void canon(int cnt, const int (*cells)[2], Shape * best) {
    int first = 1;
    for (int t = 0; t < 8; t++) {
        Shape s; s.n = cnt;
        int mx = 1 << 30, my = 1 << 30, tx[MAXS], ty[MAXS];
        for (int i = 0; i < cnt; i++) {
            int x = cells[i][0], y = cells[i][1], u, v;
            switch (t) {
                case 0: u = x; v = y; break;   case 1: u = -x; v = y; break;
                case 2: u = x; v = -y; break;  case 3: u = -x; v = -y; break;
                case 4: u = y; v = x; break;   case 5: u = -y; v = x; break;
                case 6: u = y; v = -x; break;  default: u = -y; v = -x; break;
            }
            tx[i] = u; ty[i] = v;
            if (u < mx) mx = u;
            if (v < my) my = v;
        }
        for (int i = 0; i < cnt; i++) { s.c[i][0] = (unsigned char)(tx[i] - mx); s.c[i][1] = (unsigned char)(ty[i] - my); }
        qsort(s.c, cnt, 2, cmp2);
        if (first || memcmp(s.c, best->c, 2 * cnt) < 0) { *best = s; first = 0; }
    }
}

static Shape * table; static size_t tcap, tlen;
static uint64_t hash(const Shape * s) {
    uint64_t h = 1469598103934665603ull;
    for (int i = 0; i < s->n; i++) { h = (h ^ s->c[i][0]) * 1099511628211ull; h = (h ^ s->c[i][1]) * 1099511628211ull; }
    return h;
}
static int insert(const Shape * s) {         /* 1 if it was new */
    if (2 * (tlen + 1) > tcap) {
        size_t oc = tcap; Shape * old = table;
        tcap = tcap ? 2 * tcap : 1024;
        table = calloc(tcap, sizeof(Shape));
        tlen = 0;
        for (size_t i = 0; i < oc; i++) if (old[i].n) insert(&old[i]);
        free(old);
    }
    size_t i = hash(s) & (tcap - 1);
    while (table[i].n) {
        if (table[i].n == s->n && !memcmp(table[i].c, s->c, 2 * s->n)) return 0;
        i = (i + 1) & (tcap - 1);
    }
    table[i] = *s; tlen++;
    return 1;
}

/* ---------------------------------------------------------------- strict, by brute force */

static int isl[MAXS + 2][MAXS + 2], nisl;
static int grp[MAXS];

static void islands(void) {
    static int qx[MAXS * MAXS], qy[MAXS * MAXS];
    memset(isl, -1, sizeof isl);
    nisl = 0;
    for (int x = 1; x <= BW; x++)
        for (int y = 1; y <= BH; y++)
            if (AT(g, x, y) && isl[x][y] < 0) {
                int h = 0, t = 0;
                qx[t] = x; qy[t] = y; t++; isl[x][y] = nisl;
                while (h < t) {
                    int a = qx[h], b = qy[h]; h++;
                    for (int dx = -1; dx <= 1; dx++)
                        for (int dy = -1; dy <= 1; dy++)
                            if (AT(g, a + dx, b + dy) && isl[a + dx][b + dy] < 0) { isl[a + dx][b + dy] = nisl; qx[t] = a + dx; qy[t] = b + dy; t++; }
                }
                nisl++;
            }
}

static int group_stable(int k) {
    memset(sub, 0, sizeof sub);                   /* the outer ring too: nothing left over from another box */
    for (int x = 0; x <= BW + 1; x++)
        for (int y = 0; y <= BH + 1; y++) AT(sub, x, y) = AT(g, x, y) && grp[isl[x][y]] == k;
    for (int x = 0; x <= BW + 1; x++)
        for (int y = 0; y <= BH + 1; y++) if (!ok(sub, x, y)) return 0;
    return 1;
}

/* Every split of islands i..nisl-1 into groups (restricted growth form). */
static int partition(int i, int ngroups) {
    if (i == nisl) {
        if (ngroups < 2) return 0;
        for (int k = 0; k < ngroups; k++) if (!group_stable(k)) return 0;
        return 1;
    }
    for (int k = 0; k <= ngroups; k++) {
        grp[i] = k;
        if (partition(i + 1, k == ngroups ? ngroups + 1 : ngroups)) return 1;
    }
    return 0;
}

static int is_strict(void) {
    islands();
    if (nisl == 1) return 1;
    grp[0] = 0;
    return !partition(1, 1);
}

/* ---------------------------------------------------------------- column-order search */

static long long n_strict;

static void found(void) {
    /* touches all four sides; never two empty rows in a row */
    int top = 0, bot = 0, left = 0, right = 0;
    for (int x = 1; x <= BW; x++) { top |= AT(g, x, 1); bot |= AT(g, x, BH); }
    for (int y = 1; y <= BH; y++) { left |= AT(g, 1, y); right |= AT(g, BW, y); }
    if (!top || !bot || !left || !right) return;
    for (int y = 1; y < BH; y++) {
        int e = 1;
        for (int x = 1; x <= BW; x++) if (AT(g, x, y) || AT(g, x, y + 1)) { e = 0; break; }
        if (e) return;
    }
    /* every square, the outer border too */
    for (int x = 0; x <= BW + 1; x++)
        for (int y = 0; y <= BH + 1; y++) if (!ok(g, x, y)) return;
    if (!is_strict()) return;
    int cells[MAXS][2], c = 0;
    for (int x = 1; x <= BW; x++)
        for (int y = 1; y <= BH; y++) if (AT(g, x, y)) { cells[c][0] = x; cells[c][1] = y; c++; }
    Shape s; canon(c, (const int (*)[2])cells, &s);
    if (insert(&s)) {
        n_strict++;
        if (getenv("DUMP")) {
            fprintf(stderr, "%dx%d:", BW, BH);
            for (int y = 1; y <= BH; y++) { fputc(' ', stderr); for (int x = 1; x <= BW; x++) fputc(AT(g, x, y) ? 'O' : '.', stderr); }
            fputc(10, stderr);
        }
    }
}

/* Decide square (x, y); x from 1 to BW, y from 1 to BH. Once it is decided, (x-1, y-1) has its 8 neighbours. */
static void step(int x, int y, int used, int colpop, int prevpop) {
    if (y > BH) {
        /* column x done: check the squares of column x-1 still pending (row BH and the bottom border) */
        if (x >= 2 && (!ok(g, x - 1, BH) || !ok(g, x - 1, BH + 1))) return;
        if (x >= 2 && colpop == 0 && prevpop == 0) return;           /* two empty columns in a row */
        if ((x == 1 || x == BW) && colpop == 0) return;
        if (x == BW) { if (used == N) found(); return; }
        if (N - used < (BW - x + 1) / 2) return;                      /* the BW - x columns still to come */
        step(x + 1, 1, used, 0, colpop);
        return;
    }
    for (int b = 0; b <= 1; b++) {
        if (b && used == N) break;
        AT(g, x, y) = (unsigned char)b;
        if (x >= 2 && !ok(g, x - 1, y - 1)) continue;                 /* (x-1, y-1): its neighbours are known */
        step(x, y + 1, used + b, colpop + b, prevpop);
    }
    AT(g, x, y) = 0;
}

static unsigned digits(const char * s) {
    unsigned m = 0;
    for (; *s >= '0' && *s <= '8'; s++) m |= 1u << (*s - '0');
    return m;
}

int main(int argc, char ** argv) {
    if (argc < 3) { fprintf(stderr, "usage: crosscheck B36/S23 n\n"); return 2; }
    const char * s = strchr(argv[1], 'S');
    if (argv[1][0] != 'B' || !s) return 2;
    BIRTH = digits(argv[1] + 1); SURV = digits(s + 1);
    if (BIRTH & 1) return 2;
    N = atoi(argv[2]);
    if (N < 1 || N > MAXS - 2) return 2;
    int lim = 2 * N - 1 < MAXS ? 2 * N - 1 : MAXS;
    /* BOX=w,h with w <= h (as in stilllife.c): only that box, which here is the one with BW = h columns and BH = w rows */
    int bw = 0, bh = 0;
    if (getenv("BOX") && sscanf(getenv("BOX"), "%d,%d", &bw, &bh) != 2) { fprintf(stderr, "BOX=w,h\n"); return 2; }
    for (BW = 1; BW <= lim; BW++)
        for (BH = 1; BH <= BW; BH++) {
            if (bw && (BH != bw || BW != bh)) continue;
            memset(g, 0, sizeof g);
            step(1, 1, 0, 0, 1);
        }
    printf("%lld\n", n_strict);
    return 0;
}
