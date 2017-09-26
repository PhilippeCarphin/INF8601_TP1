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
	piece_t _master;

	DragonLimits(unsigned int nb_thread)
	{
		piece_init(&_master);
	}

	void operator()(const tbb::blocked_range<uint64_t>& r) const
	{
			// Done in constructor
			// piece_t master;
			// piece_init(&master);

			// calcul des arguments a passer a chaque thread (les
			// trhead_data.{start,end}
			// Ils recoivent tous une copie de la piece master
			// unsigned int piece_size = size/nb_thread;
			// for (unsigned int i = 0; i < nb_thread; ++i)
			// {
			// 	 thread_data[i].piece = master;
			// 	 thread_data[i].id = i;
			// 	 thread_data[i].start = i*piece_size;
			// 	 if (i != nb_thread-1)
			// 	 {
			// 		thread_data[i].end = (i+1)*piece_size;
			// 	 }
			// 	 else
			// 	 {
			// 		thread_data[i].end = size;
			// 	 }


			// caller le worker
			// 	 if(pthread_create(&threads[i],NULL, dragon_limit_worker, &thread_data[i]) != 0)
			// 	 {
			// 		printf_threadsafe("%s(): pthread_create error\n", __FUNCTION__);
			// 		goto err;
			// 	 }
			// }
			// le worker appelle piece_limit() avec son start, son end, et une copie
			// de la piece master propre a chaque thread.
		piece_t task_piece = _master; // Copie (thread_data[i].piece = master;)
		piece_limit(r.begin(), r.end(),(piece_t *)&_master);


			// /* 3. Attendre la fin du traitement. */
			// for (unsigned int i = 0; i < nb_thread; ++i)
			// {
			// 	pthread_join(threads[i], NULL);
			// }

		// TODO Do some kind of synchronization?


			// for (unsigned int i = 0; i < nb_thread; ++i)
			// {
			// 	piece_merge(&master, thread_data[i].piece);
			// }
		piece_merge((piece_t*)&_master, task_piece);
	}

};

class DragonDraw {
};

class DragonRender {
};

class DragonClear {
};

int dragon_draw_tbb(char **canvas, struct rgb *image, int width, int height, uint64_t size, int nb_thread)
{
	TODO("dragon_draw_tbb");
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

	/* 3. Dessiner le dragon : DragonDraw */

	/* 4. Effectuer le rendu final */

	init.terminate();

	free_palette(palette);
	FREE(data.tid);
	//*canvas = dragon;
	*canvas = NULL;
	return 0;
}

/*
 * Calcule les limites en terme de largeur et de hauteur de
 * la forme du dragon. Requis pour allouer la matrice de dessin.
 */
int dragon_limits_tbb(limits_t *limits, uint64_t size, int nb_thread)
{
	TODO("dragon_limits_tbb");
	DragonLimits lim = DragonLimits(nb_thread);


	//initialization part
	tbb::parallel_for(tbb::blocked_range<uint64_t>(0, size), lim);

	// Return out-parameter
	*limits = lim._master.limits;
	return 0;
}
