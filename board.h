#ifndef ZZGO_BOARD_H
#define ZZGO_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "stone.h"
#include "move.h"

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect((x), 0)


/* Note that "group" is only chain of stones that is solidly
 * connected for us. */

typedef uint16_t group_t;

struct group {
	/* Number of group pseudo-liberties */
	/* Pseudo-liberties count empty-stone edges, not empty positions.
	 * Thus, a single stone will have 4 pseudo-liberties, but so will
	 * an one-eyed group in atari. The advantage is that we can update
	 * the liberties lightning-fast. */
	uint16_t libs;
	coord_t base_stone; /* First stone in group */
};

/* You should treat this struct as read-only. Always call functions below if
 * you want to change it. */

struct board {
	int size; /* Including S_OFFBOARD margin - see below. */
	int captures[S_MAX];
	float komi;
	/* Whether suicide shall be prohibited. Note that this makes
	 * for slower playouts. */
	bool prohibit_suicide;

	int moves;
	struct move last_move;

	/* The following two structures are goban maps and are indexed by
	 * coord.pos. The map is surrounded by a one-point margin from
	 * S_OFFBOARD stones in order to speed up some internal loops.
	 * Some of the foreach iterators below might include these points;
	 * you need to handle them yourselves, if you need to. */

	/* Stones played on the board */
	char *b; /* enum stone */
	/* Group id the stones are part of; 0 == no group */
	group_t *g;
	/* Positions of next stones in the stone group; 0 == last stone */
	uint16_t *p;
	/* Neighboring colors; 4 bits per color, with number of neighbors */
	uint16_t *n;

	/* Positions of free positions - queue (not map) */
	/* Note that free position here is any valid move; including single-point eyes! */
	uint16_t *f; int flen;

	/* Cache of group info, indexed by gid */
	struct group *gi;

	/* --- private */
	int last_gid;
	struct move ko;
};

#define board_at(b_, c) ((b_)->b[(c).pos])
#define board_atxy(b_, x, y) ((b_)->b[(x) + (b_)->size * (y)])

#define group_at(b_, c) ((b_)->g[(c).pos])
#define group_atxy(b_, x, y) ((b_)->g[(x) + (b_)->size * (y)])

#define neighbor_count_at(b_, coord, color) (((b_)->n[(coord).pos] >> (color*4)) & 15)
#define set_neighbor_count_at(b_, coord, color, count) ((b_)->n[(coord).pos] = ((b_)->n[(coord).pos] & ~(15 << (color*4))) | ((count) << (color*4)))
#define inc_neighbor_count_at(b_, coord, color) set_neighbor_count_at(b_, coord, color, neighbor_count_at(b_, coord, color) + 1)
#define dec_neighbor_count_at(b_, coord, color) set_neighbor_count_at(b_, coord, color, neighbor_count_at(b_, coord, color) - 1)

#define groupnext_at(b_, c) ((b_)->p[(c).pos])
#define groupnext_atxy(b_, x, y) ((b_)->p[(x) + (b_)->size * (y)])

#define board_group(b_, g_) ((b_)->gi[(g_)])
#define board_group_libs(b_, g_) (board_group(b_, g_).libs)
#define board_group_captured(b_, g_) (board_group_libs(b_, g_) == 0)

struct board *board_init(void);
struct board *board_copy(struct board *board2, struct board *board1);
void board_done_noalloc(struct board *board);
void board_done(struct board *board);
/* size here is without the S_OFFBOARD margin. */
void board_resize(struct board *board, int size);
void board_clear(struct board *board);

struct FILE;
void board_print(struct board *board, FILE *f);

/* Returns group id, 0 on allowed suicide, -1 on error */
int board_play(struct board *board, struct move *m);
/* Like above, but plays random move; the move coordinate is recorded
 * to *coord. This method will never fill your own eye. pass is played
 * when no move can be played. */
void board_play_random(struct board *b, enum stone color, coord_t *coord);

/* Returns true if given coordinate has all neighbors of given color or the edge. */
bool board_is_eyelike(struct board *board, coord_t *coord, enum stone eye_color);
/* Returns true if given coordinate is a 1-pt eye (checks against false eyes, or
 * at least tries to). */
bool board_is_one_point_eye(struct board *board, coord_t *c, enum stone eye_color);
/* Returns color of a 1pt eye owner, S_NONE if not an eye. */
enum stone board_get_one_point_eye(struct board *board, coord_t *c);

int board_group_capture(struct board *board, int group);
bool board_group_in_atari(struct board *board, int group);

/* Positive: W wins */
/* board_official_score() is the scoring method for yielding score suitable
 * for external presentation. For fast scoring of two ZZGos playing,
 * use board_fast_score(). */
float board_official_score(struct board *board);
float board_fast_score(struct board *board);


/** Iterators */

#define foreach_point(board_) \
	do { \
		coord_t c; coord_pos(c, 0, (board_)); \
		for (; c.pos < c.size * c.size; c.pos++)
#define foreach_point_end \
	} while (0)

#define foreach_in_group(board_, group_) \
	do { \
		struct board *board__ = board_; \
		coord_t c = board_group(board_, group_).base_stone; \
		coord_t c2 = c; c2.pos = groupnext_at(board__, c2); \
		do {
#define foreach_in_group_end \
			c = c2; c2.pos = groupnext_at(board__, c2); \
		} while (c.pos != 0); \
	} while (0)

/* NOT VALID inside of foreach_point() or another foreach_neighbor(), or rather
 * on S_OFFBOARD coordinates. */
#define foreach_neighbor(board_, coord_) \
	do { \
		coord_t q__[4]; int q__i = 0; \
		coord_pos(q__[q__i++], (coord_).pos - 1, (board_)); \
		coord_pos(q__[q__i++], (coord_).pos - (coord_).size, (board_)); \
		coord_pos(q__[q__i++], (coord_).pos + 1, (board_)); \
		coord_pos(q__[q__i++], (coord_).pos + (coord_).size, (board_)); \
		int fn__i; \
		for (fn__i = 0; fn__i < q__i; fn__i++) { \
			coord_t c = q__[fn__i];
#define foreach_neighbor_end \
		} \
	} while (0)

#define foreach_diag_neighbor(board_, coord_) \
	do { \
		coord_t q__[4]; int q__i = 0; \
		coord_pos(q__[q__i++], (coord_).pos - (coord_).size - 1, (board_)); \
		coord_pos(q__[q__i++], (coord_).pos - (coord_).size + 1, (board_)); \
		coord_pos(q__[q__i++], (coord_).pos + (coord_).size - 1, (board_)); \
		coord_pos(q__[q__i++], (coord_).pos + (coord_).size + 1, (board_)); \
		int fn__i; \
		for (fn__i = 0; fn__i < q__i; fn__i++) { \
			coord_t c = q__[fn__i];
#define foreach_diag_neighbor_end \
		} \
	} while (0)


#endif
