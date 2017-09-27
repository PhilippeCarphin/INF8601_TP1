/*
 * dragon_tbb.c
 *
 *  Created on: 2011-08-17
 *      Author: Francis Giraldeau <francis.giraldeau@gmail.com>
 */

#include <iostream>

extern "C" {
#include "dragon.h"
#include "color.h"
#include "utils.h"
}
#include "dragon_tbb.h"
#include "tbb/tbb.h"
#include "TidMap.h"

using namespace std;
using namespace tbb;

class DragonLimits {

public:
	piece_t _piece;

	DragonLimits(unsigned int nb_thread)
	{
		piece_init(&_piece);
	}

	// Splitting constructor. They will all get a copy of the root instance of
	// DragonLimits which is a piece on which piece_init has been called.
	// Same as thread_data[i].piece = master.
	DragonLimits(const DragonLimits& dl, split)
	{
		piece_init(&_piece);
	}

	// Thread worker Arguments passed to piece_limit are calculated by TBB
	// automatically
	void operator()(const tbb::blocked_range<uint64_t>& r) const
	{
		piece_limit(r.begin(), r.end(),(piece_t *)&_piece);
	}

	// Join rhs with myself
	void join(DragonLimits& rhs)
	{
		piece_merge(&_piece, rhs._piece);
	}
};

uint64_t max(uint64_t a, uint64_t b)
{
	return ( a < b ? b : a);
}
uint64_t min(uint64_t a, uint64_t b)
{
	return (a < b ? a : b);
}

class DragonDraw {
	public:
	struct draw_data _data;
	TidMap *_tidMap;
	DragonDraw(struct draw_data data, TidMap *tidMap)
	:_tidMap(tidMap)
	{
		_data = data;
	}
	// Threads : 5
	//               |<-inte_size->|
	//               |  r.begin()  |                 r.end()
	//               |     |       |                   |
	// #-------------|-------------|-------------|-------------|-------------#
	//start          1     |       2             3     |       4           end
	//               |     |       |             |     |       |
	//               |     |       |             |     |       |
	//               |     |       |             |     |       |
	//color=1: color_start |       |             |     |       |
	//                draw_start   |             |     |       |
	//                          color_end        |     |       |
	//                          draw_end         |     |       |
	//                                           |     |       |
	//                                           |     |       |
	//                                           |     |       |
	//color=3:                            start_color  |       |
	//                                    draw_start   |       |
	//                                              draw_end   |
	//                                                    color_end
	//  start_color = r.begin()/interval_size = 1
	//  end_color   = r.end()/interval_size   = 3
	void operator()(const tbb::blocked_range<uint64_t>& r) const
	{
		uint64_t interval_size = _data.size / _data.nb_thread;

		unsigned int  start_color = r.begin() / interval_size;
		unsigned int  end_color = r.end() / interval_size;

		for(unsigned int color = start_color; color <= end_color; ++color)
		{
			uint64_t color_start = interval_size * color;
			uint64_t color_end = (color + 1) * interval_size;

			if( color_end < r.begin() || r.end() < color_start)
				continue;

			uint64_t draw_start = max(color_start, r.begin());
			uint64_t draw_end = min(color_end, r.end());

			dragon_draw_raw(draw_start, draw_end, _data.dragon,
						_data.dragon_width, _data.dragon_height,
						_data.limits, color);
		}
	}
};

class DragonRender {
	public:
	struct draw_data _data;
	DragonRender(struct draw_data data)
	{
		_data = data;
	}

	void operator()(const tbb::blocked_range<uint64_t>& r) const
	{
		scale_dragon(r.begin(), r.end(), _data.image, _data.image_width, _data.image_height,
										_data.dragon, _data.dragon_width, _data.dragon_height,
										_data.palette);
	}

};

class DragonClear {
	public:
	struct draw_data _data;
	DragonClear(struct draw_data data)
	{
		_data = data;
	}

	void operator()(const tbb::blocked_range<uint64_t>& r) const
	{
		init_canvas(r.begin(), r.end(), _data.dragon, -1);
	}
};

int dragon_draw_tbb(char **canvas, struct rgb *image, int width, int height, uint64_t size, int nb_thread)
{
	struct draw_data data;
	limits_t limits;
	char *dragon = NULL;
	int dragon_width;
	int dragon_height;
	int dragon_surface;
	int scale_x;
	int scale_y;
	int scale;
	int deltaJ;
	int deltaI;

	struct palette *palette = init_palette(nb_thread);
	if (palette == NULL)
		return -1;

	/* 1. Calculer les limites du dragon */
	dragon_limits_tbb(&limits, size, nb_thread);

	task_scheduler_init init(nb_thread);

	dragon_width = limits.maximums.x - limits.minimums.x;
	dragon_height = limits.maximums.y - limits.minimums.y;
	dragon_surface = dragon_width * dragon_height;
	scale_x = dragon_width / width + 1;
	scale_y = dragon_height / height + 1;
	scale = (scale_x > scale_y ? scale_x : scale_y);
	deltaJ = (scale * width - dragon_width) / 2;
	deltaI = (scale * height - dragon_height) / 2;

	dragon = (char *) malloc(dragon_surface);
	if (dragon == NULL) {
		free_palette(palette);
		return -1;
	}

	data.nb_thread = nb_thread;
	data.dragon = dragon;
	data.image = image;
	data.size = size;
	data.image_height = height;
	data.image_width = width;
	data.dragon_width = dragon_width;
	data.dragon_height = dragon_height;
	data.limits = limits;
	data.scale = scale;
	data.deltaI = deltaI;
	data.deltaJ = deltaJ;
	data.palette = palette;
	data.tid = (int *) calloc(nb_thread, sizeof(int));

	/* 2. Initialiser la surface : DragonClear */
	uint64_t area = data.dragon_width * data.dragon_height;
	DragonClear dc = DragonClear(data);
	parallel_for(blocked_range<uint64_t>(0,area), dc);

	/* 3. Dessiner le dragon : DragonDraw */
	TidMap *tidMap = new TidMap(nb_thread);
	DragonDraw dd = DragonDraw(data, tidMap);
	parallel_for(blocked_range<uint64_t>(0,size), dd);

	/* 4. Effectuer le rendu final */
	DragonRender dr = DragonRender(data);
	parallel_for(blocked_range<uint64_t>(0,height), dr);

	init.terminate();

	free_palette(palette);
	FREE(data.tid);
	*canvas = dragon;
	// *canvas = NULL;
	return 0;
}

/*
 * Calcule les limites en terme de largeur et de hauteur de
 * la forme du dragon. Requis pour allouer la matrice de dessin.
 */
int dragon_limits_tbb(limits_t *limits, uint64_t size, int nb_thread)
{
	DragonLimits lim = DragonLimits(nb_thread);

	tbb::parallel_reduce(tbb::blocked_range<uint64_t>(0, size), lim);

	*limits = lim._piece.limits;
	return 0;
}
