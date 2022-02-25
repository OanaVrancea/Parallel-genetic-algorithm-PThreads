#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "genetic_algorithm_par.h"
#include <math.h>

int read_input(sack_object **objects, int *object_count, int *sack_capacity, int *generations_count, int *P, int argc, char *argv[])
{
	FILE *fp;

	if (argc < 3) {
		fprintf(stderr, "Usage:\n\t./tema1 in_file generations_count\n");
		return 0;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		return 0;
	}

	if (fscanf(fp, "%d %d", object_count, sack_capacity) < 2) {
		fclose(fp);
		return 0;
	}

	if (*object_count % 10) {
		fclose(fp);
		return 0;
	}

	sack_object *tmp_objects = (sack_object *) calloc(*object_count, sizeof(sack_object));

	for (int i = 0; i < *object_count; ++i) {
		if (fscanf(fp, "%d %d", &tmp_objects[i].profit, &tmp_objects[i].weight) < 2) {
			free(objects);
			fclose(fp);
			return 0;
		}
	}

	fclose(fp);

	*generations_count = (int) strtol(argv[2], NULL, 10);
	
	if (*generations_count == 0) {
		free(tmp_objects);

		return 0;
	}

	*objects = tmp_objects;

	*P = (int) strtol(argv[3], NULL, 10);

	return 1;
}

void print_objects(const sack_object *objects, int object_count)
{
	for (int i = 0; i < object_count; ++i) {
		printf("%d %d\n", objects[i].weight, objects[i].profit);
	}
}

void print_generation(const individual *generation, int limit)
{
	for (int i = 0; i < limit; ++i) {
		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			printf("%d ", generation[i].chromosomes[j]);
		}

		printf("\n%d - %d\n", i, generation[i].fitness);
	}
}

void print_best_fitness(const individual *generation)
{
	printf("%d\n", generation[0].fitness);
}

void compute_fitness_function(const sack_object *objects, individual *generation, int object_count, int sack_capacity, int start, int end)
{
	int weight;
	int profit;

	//parallelization of the first for
	for (int i = start; i < end; ++i) {
		weight = 0;
		profit = 0;

		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			if (generation[i].chromosomes[j]) {
				weight += objects[j].weight;
				profit += objects[j].profit;
			}
		}

		generation[i].fitness = (weight <= sack_capacity) ? profit : 0;
	}
}

int cmpfunc(const void *a, const void *b)
{
	individual *first = (individual *) a;
	individual *second = (individual *) b;

	int res = second->fitness - first->fitness; // decreasing by fitness
	if (res == 0) {
		res = first->sum - second->sum; // increasing by number of objects in the sack
		if (res == 0) {
			return second->index - first->index;
		}
	}

	return res;
}

void mutate_bit_string_1(const individual *ind, int generation_index)
{
	int i, mutation_size;
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	if (ind->index % 2 == 0) {
		// for even-indexed individuals, mutate the first 40% chromosomes by a given step
		mutation_size = ind->chromosome_length * 4 / 10;
		for (i = 0; i < mutation_size; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	} else {
		// for even-indexed individuals, mutate the last 80% chromosomes by a given step
		mutation_size = ind->chromosome_length * 8 / 10;
		for (i = ind->chromosome_length - mutation_size; i < ind->chromosome_length; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	}
}

void mutate_bit_string_2(const individual *ind, int generation_index)
{
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	// mutate all chromosomes by a given step
	for (int i = 0; i < ind->chromosome_length; i += step) {
		ind->chromosomes[i] = 1 - ind->chromosomes[i];
	}
}

void crossover(individual *parent1, individual *child1, int generation_index)
{
	individual *parent2 = parent1 + 1;
	individual *child2 = child1 + 1;
	int count = 1 + generation_index % parent1->chromosome_length;

	memcpy(child1->chromosomes, parent1->chromosomes, count * sizeof(int));
	memcpy(child1->chromosomes + count, parent2->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));

	memcpy(child2->chromosomes, parent2->chromosomes, count * sizeof(int));
	memcpy(child2->chromosomes + count, parent1->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));
}

void copy_individual(const individual *from, const individual *to)
{
	memcpy(to->chromosomes, from->chromosomes, from->chromosome_length * sizeof(int));
}

void free_generation(individual *generation)
{
	int i;

	for (i = 0; i < generation->chromosome_length; ++i) {
		free(generation[i].chromosomes);
		generation[i].chromosomes = NULL;
		generation[i].fitness = 0;
	}
}

void *run_genetic_algorithm(void *arg)
{

	tstruct t = *(tstruct*)arg;

	int count, cursor;
	individual *tmp = NULL;

	t.start = (t.id * (double)t.object_count/ t.P);
	t.end = fmin((t.id + 1) * (double)t.object_count/ t.P, t.object_count);

	// set initial generation (composed of object_count individuals with a single item in the sack)
	for (int i = t.start; i < t.end; ++i) {
		t.current_generation[i].fitness = 0;
		t.current_generation[i].chromosomes = (int*) calloc(t.object_count, sizeof(int));
		t.current_generation[i].chromosomes[i] = 1;
		t.current_generation[i].index = i;
		t.current_generation[i].chromosome_length = t.object_count;
		t.current_generation[i].sum = 0;

		t.next_generation[i].fitness = 0;
		t.next_generation[i].chromosomes = (int*) calloc(t.object_count, sizeof(int));
		t.next_generation[i].index = i;
		t.next_generation[i].chromosome_length = t.object_count;
		t.next_generation[i].sum = 0;

	}

	pthread_barrier_wait(&barrier);

	// iterate for each generation
	for (int k = 0; k < t.generations_count; ++k) {
		cursor = 0;

		//compute start and end for the parallelization of computer_fitness_function
		t.start = (t.id * (double)t.object_count/ t.P);
		t.end = fmin((t.id + 1) * (double)t.object_count/ t.P, t.object_count);
		compute_fitness_function(t.objects, t.current_generation, t.object_count, t.sack_capacity, t.start, t.end);

		pthread_barrier_wait(&barrier);

		//compute start and end in order to parallelize the computation of chromosomes's values
		t.start = (t.id * (double)t.object_count/ t.P);
		t.end = fmin((t.id + 1) * (double)t.object_count/ t.P, t.object_count);

		for(int i = t.start; i < t.end; i++){
			for(int j = 0; j < t.current_generation[i].chromosome_length; j++){
				t.current_generation[i].sum += t.current_generation[i].chromosomes[j];
			}
		}

		pthread_barrier_wait(&barrier);
		
		if(t.id == 0){
			qsort(t.current_generation, t.object_count, sizeof(individual), cmpfunc);
		}

		pthread_barrier_wait(&barrier);	

		// keep first 30% children (elite children selection)
		count = t.object_count * 3 / 10;

		t.start = (t.id * (double)count/ t.P);
		t.end = fmin((t.id + 1) * (double)count/ t.P, count);

		for (int i = t.start; i < t.end; ++i) {
			copy_individual(t.current_generation + i, t.next_generation + i);
		}
		cursor = count;

		pthread_barrier_wait(&barrier);

		// mutate first 20% children with the first version of bit string mutation

		count = t.object_count * 2 / 10;

		t.start = t.id * (double)count/ t.P;
		t.end = fmin((t.id + 1) * (double)count/ t.P, count);

		for (int i = t.start; i < t.end; ++i) {
			copy_individual(t.current_generation + i, t.next_generation + cursor + i);
			mutate_bit_string_1(t.next_generation + cursor + i, k);
		}
		cursor += count;
		
		pthread_barrier_wait(&barrier);

		// mutate next 20% children with the second version of bit string mutation
		count = t.object_count * 2 / 10;

		t.start = (t.id * (double)count/ t.P);
		t.end = fmin((t.id + 1) * (double)count/ t.P, count);

		for (int i = t.start; i < t.end; ++i) {
			copy_individual(t.current_generation + i + count, t.next_generation + cursor + i);
			mutate_bit_string_2(t.next_generation + cursor + i, k);
		}
		cursor += count;

		pthread_barrier_wait(&barrier);

		// crossover first 30% parents with one-point crossover
		// (if there is an odd number of parents, the last one is kept as such)
		count = t.object_count * 3 / 10;
		if (count % 2 == 1) {
			copy_individual(t.current_generation + t.object_count - 1, t.next_generation + cursor + count - 1);
			count--;
		}
		

		if(t.id == 0){
			for (int i = 0; i < count; i += 2) {
				crossover(t.current_generation + i, t.next_generation + cursor + i, k);
			}
		}
		
		pthread_barrier_wait(&barrier);

		// switch to new generation
		if(t.id == 0){
			tmp = t.current_generation;
			t.current_generation = t.next_generation;
			t.next_generation = tmp;
		}

		pthread_barrier_wait(&barrier);

		t.start = (t.id * (double)t.object_count/ t.P);
		t.end = fmin((t.id + 1) * (double)t.object_count/ t.P, t.object_count);
		for (int i = t.start; i < t.end; ++i) {
			t.current_generation[i].index = i;
		}


		pthread_barrier_wait(&barrier);

		if (k % 5 == 0 && t.id == 0) {
			print_best_fitness(t.current_generation);
		}

		pthread_barrier_wait(&barrier);
	}

	t.start = (t.id * (double)t.object_count/ t.P);
	t.end = fmin((t.id + 1) * (double)t.object_count/ t.P, t.object_count);
	compute_fitness_function(t.objects, t.current_generation, t.object_count, t.sack_capacity, t.start, t.end);

	pthread_barrier_wait(&barrier);

	t.start = (t.id * (double)t.object_count/ t.P);
	t.end = fmin((t.id + 1) * (double)t.object_count/ t.P, t.object_count);

	for(int i = t.start; i < t.end; i++){
		for(int j = 0; j < t.current_generation[i].chromosome_length; j++){
			t.current_generation[i].sum += t.current_generation[i].chromosomes[j];
		}
	}

	pthread_barrier_wait(&barrier);

	if(t.id == 0){
		qsort(t.current_generation, t.object_count, sizeof(individual), cmpfunc);
	}

	if(t.id == 0){
		print_best_fitness(t.current_generation);
	}

	if(t.id == 0){
		free_generation(t.current_generation);
		free_generation(t.next_generation);

		// free resources
		free(t.current_generation);
		free(t.next_generation);
	}

	pthread_exit(NULL);
}