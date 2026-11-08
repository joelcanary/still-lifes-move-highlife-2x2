/*
 * Strict still lifes with n cells in a Life-like cellular automaton (rule B.../S...), counted up
 * to rotation and reflection: the definition used by A019473 (Conway), A175000 (HighLife),
 * A175001 (Move), A175002 (Maze) and A166476 (2x2).
 *
 *   stilllife B36/S23 n [Hmax]     -> number of strict still lifes with n cells
 *
 * METHOD (every step can be checked by hand):
 *  1. A still life with n cells fits in a w x h box that it touches on all four sides. The boxes
 *     with w <= h <= Hmax are enumerated (those with h < w are the same ones rotated by 90 degrees,
 *     and are counted in w x h).
 *  2. Inside the box, cells are decided row by row, left to right. As soon as a cell of the row
 *     ABOVE has all 8 neighbours decided, the rule is checked on it: if alive, its neighbour count
 *     is in S; if dead (including the cells outside the box), it is not in B. If the check fails,
 *     the branch is pruned there and then.
 *  3. Pruning that loses no STRICT still life: never more than n cells; never two empty rows in a
 *     row, nor two empty columns in a row (what lies on each side never interacts, so each part
 *     would be stable on its own and the pattern would not be strict).
 *  4. At the end: n cells, the box touched on all four sides and no births below. It is stable.
 *  5. Symmetry: a pattern is counted only if it is the smallest (row by row) of its transforms that
 *     fit in the same box (4 if w < h; all 8 if w == h).
 *  6. Strict: it cannot be split into two or more non-empty pieces, each stable on its own. Such a
 *     split is searched for exactly: every cell gets a piece label and, as soon as a square has all
 *     its neighbours labelled, the rule is checked for each piece separately. Pieces are unions of
 *     whole ISLANDS (8-connected components), as LifeWiki defines it ("pseudo still life": two or
 *     more islands that can be split into stable subpatterns). A single island is always strict.
 *
 * Hmax: maximum box height. With no two empty rows in a row and at least one cell in every
 * non-empty row, h <= 2n - 1 always: that is the default (exhaustive search).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned BIRTH, SURV;            /* bit k: k neighbours => birth / survival */
static int N, W, H;
static uint64_t row[132];               /* row[1..H]: the box, in bits 1..W; bits 0 and W+1 are 0 */
static long long count_strict, count_stable;

/* Does the square in column x (0..W+1) of row mid obey the rule, given its neighbouring rows? */
static inline int cell_ok(uint64_t up, uint64_t mid, uint64_t down, int x) {
    uint64_t win = x ? (7ull << (x - 1)) : 3ull;
    int alive = (int)((mid >> x) & 1);
    int cnt = __builtin_popcountll(up & win) + __builtin_popcountll(mid & win) + __builtin_popcountll(down & win) - alive;
    return alive ? ((SURV >> cnt) & 1) : !((BIRTH >> cnt) & 1);
}

static int row_ok(uint64_t up, uint64_t mid, uint64_t down) {
    for (int x = 0; x <= W + 1; x++) if (!cell_ok(up, mid, down, x)) return 0;
    return 1;
}

/* ---------------------------------------------------------------- strict */

static int n_cells;
static int cx[64], cy[64];
/* Squares to watch: the cells of the pattern and the dead squares touching one. For each: which
 * pattern cells lie in its 3x3 (excluding itself), itself (or -1), and at which cell its
 * neighbourhood becomes complete. */
static int n_sq;
static int sq_self[64 * 9];
static int sq_nb[64 * 9][8], sq_nbn[64 * 9];
static int sq_done[64 * 9];
static int done_head[64], done_next[64 * 9];
static int label[64];
static int island[64];                  /* first cell (in order) of each cell's island */

static void build_squares(void) {
    n_cells = 0;
    for (int y = 1; y <= H; y++)
        for (int x = 1; x <= W; x++)
            if ((row[y] >> x) & 1) { cx[n_cells] = x; cy[n_cells] = y; n_cells++; }
    /* islands: 8-connected components; island[i] = the smallest cell of i's island (minimum propagation) */
    for (int i = 0; i < n_cells; i++) island[i] = i;
    for (int changed = 1; changed; ) {
        changed = 0;
        for (int i = 0; i < n_cells; i++)
            for (int j = 0; j < n_cells; j++) {
                int dx = cx[i] - cx[j], dy = cy[i] - cy[j];
                if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1 && island[j] < island[i]) { island[i] = island[j]; changed = 1; }
            }
    }
    n_sq = 0;
    for (int i = 0; i < n_cells; i++) done_head[i] = -1;
    for (int y = 0; y <= H + 1; y++)
        for (int x = 0; x <= W + 1; x++) {
            int self = -1, nn = 0, last = -1;
            for (int k = 0; k < n_cells; k++) {
                int dx = cx[k] - x, dy = cy[k] - y;
                if (dx < -1 || dx > 1 || dy < -1 || dy > 1) continue;
                if (dx == 0 && dy == 0) self = k; else sq_nb[n_sq][nn++] = k;
                if (k > last) last = k;
            }
            if (last < 0) continue;                       /* does not touch the pattern: nothing is ever born */
            sq_self[n_sq] = self; sq_nbn[n_sq] = nn; sq_done[n_sq] = last;
            done_next[n_sq] = done_head[last]; done_head[last] = n_sq;
            n_sq++;
        }
}

/* With cells 0..i labelled, do the squares completed at i obey the rule, piece by piece? */
static int check_done(int i, int nlabels) {
    for (int s = done_head[i]; s >= 0; s = done_next[s]) {
        int cnt[64] = {0};
        for (int k = 0; k < sq_nbn[s]; k++) cnt[label[sq_nb[s][k]]]++;
        int me = sq_self[s] >= 0 ? label[sq_self[s]] : -1;
        for (int p = 0; p < nlabels; p++) {
            if (p == me) { if (!((SURV >> cnt[p]) & 1)) return 0; }
            else if ((BIRTH >> cnt[p]) & 1) return 0;
        }
    }
    return 1;
}

/* Label cells i..n-1 (new labels in order: cell 0 carries label 0). Is there a split into at
 * least two pieces, each stable on its own? An island goes whole to one piece: its first cell
 * chooses and the rest follow. */
static int split_from(int i, int nlabels) {
    if (i == n_cells) return nlabels >= 2;
    if (island[i] != i) { label[i] = label[island[i]]; return check_done(i, nlabels) && split_from(i + 1, nlabels); }
    for (int p = 0; p <= nlabels && p < 63; p++) {
        label[i] = p;
        int nl = p == nlabels ? nlabels + 1 : nlabels;
        if (check_done(i, nl) && split_from(i + 1, nl)) return 1;
    }
    return 0;
}

static int strict(void) {
    build_squares();
    int one = 1;
    for (int i = 0; i < n_cells; i++) if (island[i] != 0) { one = 0; break; }
    if (one) return 1;                 /* a single island: strict by definition */
    label[0] = 0;
    if (!check_done(0, 1)) return 1;   /* cannot happen: the whole pattern (one piece) is stable */
    return !split_from(1, 1);
}

/* ---------------------------------------------------------------- symmetry */

static uint64_t rev_bits(uint64_t r) {
    uint64_t o = 0;
    for (int x = 1; x <= W; x++) if ((r >> x) & 1) o |= 1ull << (W + 1 - x);
    return o;
}

static int less(const uint64_t * t) {     /* is transform t smaller than the current box? */
    for (int y = 1; y <= H; y++) if (t[y] != row[y]) return t[y] < row[y];
    return 0;
}

static int canonical(void) {
    uint64_t t[132], tr[132];
    for (int y = 1; y <= H; y++) t[y] = rev_bits(row[y]);
    if (less(t)) return 0;
    for (int y = 1; y <= H; y++) t[y] = row[H + 1 - y];
    if (less(t)) return 0;
    for (int y = 1; y <= H; y++) t[y] = rev_bits(row[H + 1 - y]);
    if (less(t)) return 0;
    if (W != H) return 1;
    for (int y = 1; y <= H; y++) {
        tr[y] = 0;
        for (int x = 1; x <= W; x++) if ((row[x] >> y) & 1) tr[y] |= 1ull << x;
    }
    if (less(tr)) return 0;
    for (int y = 1; y <= H; y++) t[y] = rev_bits(tr[y]);
    if (less(t)) return 0;
    for (int y = 1; y <= H; y++) t[y] = tr[H + 1 - y];
    if (less(t)) return 0;
    for (int y = 1; y <= H; y++) t[y] = rev_bits(tr[H + 1 - y]);
    if (less(t)) return 0;
    return 1;
}

/* ---------------------------------------------------------------- search */

static void finish(void) {
    uint64_t all = 0;
    for (int y = 1; y <= H; y++) all |= row[y];
    if (!((all >> 1) & 1) || !((all >> W) & 1)) return;
    for (int x = 1; x < W; x++) if (!((all >> x) & 3ull)) return;
    if (!canonical()) return;
    count_stable++;
    if (strict()) count_strict++;
}

static void place_row(int y, int used);

/* Decide bit x of row y (1..W). Once it is decided, square x-1 of row y-1 has all 8 neighbours
 * and is checked. */
static void place_bit(int y, int x, int used) {
    if (x > W) {
        /* the last two squares of row y-1: column W and the one outside (W+1) */
        uint64_t up = y >= 2 ? row[y - 2] : 0;
        if (!cell_ok(up, row[y - 1], row[y], W) || !cell_ok(up, row[y - 1], row[y], W + 1)) return;
        int pc = __builtin_popcountll(row[y]);
        if ((y == 1 || y == H) && pc == 0) return;            /* the box touches top and bottom */
        if (pc == 0 && row[y - 1] == 0) return;                /* two empty rows in a row */
        place_row(y + 1, used);
        return;
    }
    for (int b = 0; b <= 1; b++) {
        if (b && used >= N) break;
        if (b) row[y] |= 1ull << x; else row[y] &= ~(1ull << x);
        /* square x-1 of row y-1: its neighbours are now known (columns x-2..x of rows y-2, y-1, y) */
        if (cell_ok(y >= 2 ? row[y - 2] : 0, row[y - 1], row[y], x - 1))
            place_bit(y, x + 1, used + b);
    }
    row[y] &= ~(1ull << x);
}

static void place_row(int y, int used) {
    if (y > H) {
        if (used != N) return;
        /* the last row of the box (with the empty outside row below) and the outside row itself */
        if (!row_ok(row[H - 1], row[H], 0) || !row_ok(row[H], 0, 0)) return;
        finish();
        return;
    }
    int rows_left = H - y + 1;
    if (N - used < (rows_left + 1) / 2) return;               /* at least one cell every two rows */
    row[y] = 0;
    place_bit(y, 1, used);
}

static unsigned parse_digits(const char * s) {
    unsigned m = 0;
    for (; *s >= '0' && *s <= '8'; s++) m |= 1u << (*s - '0');
    return m;
}

int main(int argc, char ** argv) {
    if (argc < 3) { fprintf(stderr, "usage: stilllife B36/S23 n [Hmax]\n"); return 2; }
    const char * r = argv[1];
    const char * s = strchr(r, 'S');
    if (r[0] != 'B' || !s) { fprintf(stderr, "rule: B.../S...\n"); return 2; }
    BIRTH = parse_digits(r + 1);
    SURV = parse_digits(s + 1);
    if (BIRTH & 1) { fprintf(stderr, "rules with B0 are not supported\n"); return 2; }
    N = atoi(argv[2]);
    if (N < 1 || N > 63) { fprintf(stderr, "n must be between 1 and 63\n"); return 2; }
    int hmax = argc > 3 ? atoi(argv[3]) : 2 * N - 1;
    if (hmax > 62) hmax = 62;
    /* BOX=w,h: only that box (to split the work; the counts per box add up) */
    int bw = 0, bh = 0;
    if (getenv("BOX") && sscanf(getenv("BOX"), "%d,%d", &bw, &bh) != 2) { fprintf(stderr, "BOX=w,h\n"); return 2; }
    for (H = 1; H <= hmax; H++)
        for (W = 1; W <= H; W++) {
            if (bw && (W != bw || H != bh)) continue;
            row[0] = 0;
            place_row(1, 0);
        }
    printf("%lld\n", count_strict);
    fprintf(stderr, "stable patterns counted (strict and pseudo): %lld\n", count_stable);
    return 0;
}
