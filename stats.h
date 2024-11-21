#pragma once

/* **** */

#include "../libbse/include/dtime.h"

/* **** */

#include <stdint.h>

/* **** */

typedef struct {
	int64_t t;
	uint64_t start_t;
	struct {
		double ratio;
		uint64_t start;
	}dtime;
	struct {
		uint64_t start;
		uint64_t end;
	}frame;
	struct {
		uint64_t start;
		uint64_t end;
	}render;
}DTime;

const double kKipsRatio = 1.0 / KHz(1.0);
const double kMipsRatio = 1.0 / MHz(1.0);

/* **** */

#ifndef KHz
	#define KHz(_x) ((_x) * 1000)
#endif

#ifndef MHz
	#define MHz(_x) KHz(KHz(_x))
#endif

/* **** */

static void _stats_frame_start(DTime *const stats)
{
	stats->start_t = stats->t;
	stats->frame.start = get_dtime();
}

static void _stats_frame_end_render_start(DTime *const stats)
{ stats->render.start = (stats->frame.end = get_dtime()); }

static void _stats_render_end(DTime *const stats)
{ stats->render.end = get_dtime(); }

/* **** */

void render_end(DTime *const stats)
{
	_stats_render_end(stats);

	const uint64_t frame_dtime = stats->frame.end - stats->frame.start;
	const uint64_t render_dtime = stats->render.end - stats->render.start;

	printf("frame_dtime: 0x%016" PRIx64, frame_dtime);
	printf(", render_dtime: 0x%016" PRIx64, render_dtime);

	const uint64_t frame_cycles = stats->t - stats->start_t;

	printf(", frame_cycles: 0x%016" PRIx64, frame_cycles);

	const uint64_t run_dtime = stats->render.end - stats->dtime.start;
	const double run_seconds = run_dtime * stats->dtime.ratio;

	const uint64_t icount_second = stats->t / run_seconds;

	if(icount_second < MHz(1))
		printf(", KHz: %03.010f", kKipsRatio * icount_second);
	else
		printf(", MHz: %03.010f", kMipsRatio * icount_second);

	printf("\r");

	_stats_frame_start(stats);
}

void stats_init(DTime *const stats)
{
	stats->dtime.ratio = 1.0 / dtime_calibrate();
	stats->dtime.start = get_dtime();
}
