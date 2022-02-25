#include <stdlib.h>
#include "genetic_algorithm_par.h"
#include <pthread.h>
#include <stdio.h>
#include <math.h>

int main(int argc, char *argv[]) {
	// array with all the objects that can be placed in the sack

	sack_object *objects = NULL;

	// number of objects
	int object_count = 0;

	// maximum weight that can be carried in the sack
	int sack_capacity = 0;

	// number of generations
	int generations_count = 0;

	//number of threads
	int P = 0;

	if (!read_input(&objects, &object_count, &sack_capacity, &generations_count, &P, argc, argv)) {
		return 0;
	}

	tstruct t[P];
	pthread_t tid[P];
	
	pthread_barrier_init(&barrier, NULL, P);

	individual *current_generation = (individual*) calloc(object_count, sizeof(individual));
	individual *next_generation = (individual*) calloc(object_count, sizeof(individual));

	for (int i = 0; i < P; i++){
			t[i].id = i;
			t[i].P = P;
			t[i].object_count = object_count;
			t[i].objects = objects;
			t[i].generations_count = generations_count;
			t[i].sack_capacity = sack_capacity;
			t[i].current_generation = current_generation;
			t[i].next_generation = next_generation;
		    pthread_create(&tid[i], NULL, run_genetic_algorithm, (void *) &t[i]);
		}

	for (int i = 0; i < P; i++){
		pthread_join(tid[i], NULL);
	}


	free(objects);

	pthread_barrier_destroy(&barrier);

	return 0;
}
