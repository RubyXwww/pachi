#define DEBUG
#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "board.h"
#include "debug.h"
#include "engine.h"
#include "replay/replay.h"
#include "montecarlo/montecarlo.h"
#include "random/random.h"
#include "patternscan/patternscan.h"
#include "t-unit/test.h"
#include "uct/uct.h"
#include "gtp.h"
#include "timeinfo.h"
#include "random.h"
#include "version.h"

int debug_level = 3;
int seed;


enum engine_id {
	E_RANDOM,
	E_REPLAY,
	E_PATTERNSCAN,
	E_MONTECARLO,
	E_UCT,
	E_MAX,
};

static struct engine *(*engine_init[E_MAX])(char *arg, struct board *b) = {
	engine_random_init,
	engine_replay_init,
	engine_patternscan_init,
	engine_montecarlo_init,
	engine_uct_init,
};

static struct engine *init_engine(enum engine_id engine, char *e_arg, struct board *b)
{
	char *arg = e_arg? strdup(e_arg) : e_arg;
	assert(engine < E_MAX);
	struct engine *e = engine_init[engine](arg, b);
	if (arg) free(arg);
	return e;
}

static void done_engine(struct engine *e)
{
	if (e->done) e->done(e);
	if (e->data) free(e->data);
	free(e);
}

bool engine_reset = false;


int main(int argc, char *argv[])
{
	enum engine_id engine = E_UCT;
	struct time_info ti_default = { .period = TT_NULL };
	char *testfile = NULL;

	seed = time(NULL) ^ getpid();

	int opt;
	while ((opt = getopt(argc, argv, "e:d:s:t:u:")) != -1) {
		switch (opt) {
			case 'e':
				if (!strcasecmp(optarg, "random")) {
					engine = E_RANDOM;
				} else if (!strcasecmp(optarg, "patternscan")) {
					engine = E_PATTERNSCAN;
				} else if (!strcasecmp(optarg, "replay")) {
					engine = E_REPLAY;
				} else if (!strcasecmp(optarg, "montecarlo")) {
					engine = E_MONTECARLO;
				} else if (!strcasecmp(optarg, "uct")) {
					engine = E_UCT;
				} else {
					fprintf(stderr, "%s: Invalid -e argument %s\n", argv[0], optarg);
					exit(1);
				}
				break;
			case 'd':
				debug_level = atoi(optarg);
				break;
			case 's':
				seed = atoi(optarg);
				break;
			case 't':
				/* Time settings to follow; if specified,
				 * GTP time information is ignored. Useful
				 * e.g. when you want to force your bot to
				 * play weaker while giving the opponent
				 * reasonable time to play, or force play
				 * by number of simulations in timed games. */
				/* Please see timeinfo.h:time_parse()
				 * description for syntax details. */
				if (!time_parse(&ti_default, optarg)) {
					fprintf(stderr, "%s: Invalid -t argument %s\n", argv[0], optarg);
					exit(1);
				}
				ti_default.ignore_gtp = true;
				/* Check for unsupported combinations. */
				assert(ti_default.period != TT_NULL);
				if (ti_default.dim == TD_WALLTIME && ti_default.period == TT_TOTAL) {
					fprintf(stderr, "%s: Spreading fixed time over the whole game without GTP time_left aid is not supported for now\n", argv[0]);
					exit(1);
				}
				break;
			case 'u':
				testfile = strdup(optarg);
				break;
			default: /* '?' */
				fprintf(stderr, "Pachi version %s\n", PACHI_VERSION);
				fprintf(stderr, "Usage: %s [-e random|replay|patternscan|montecarlo|uct] [-d DEBUG_LEVEL] [-s RANDOM_SEED] [-t TIME_SETTINGS] [-u TEST_FILENAME] [ENGINE_ARGS]\n",
						argv[0]);
				exit(1);
		}
	}

	fast_srandom(seed);
	fprintf(stderr, "Random seed: %d\n", seed);

	struct board *b = board_init();
	struct time_info ti[S_MAX];
	ti[S_BLACK] = ti_default;
	ti[S_WHITE] = ti_default;

	char *e_arg = NULL;
	if (optind < argc)
		e_arg = argv[optind];
	struct engine *e = init_engine(engine, e_arg, b);

	if (testfile) {
		unittest(testfile);
		return 0;
	}

	char buf[4096];
	while (fgets(buf, 4096, stdin)) {
		if (DEBUGL(1))
			fprintf(stderr, "IN: %s", buf);
		gtp_parse(b, e, ti, buf);
		if (engine_reset) {
			if (!e->keep_on_clear) {
				b->es = NULL;
				done_engine(e);
				e = init_engine(engine, e_arg, b);
				ti[S_BLACK] = ti_default;
				ti[S_WHITE] = ti_default;
			}
			engine_reset = false;
		}
	}
	done_engine(e);
	return 0;
}
