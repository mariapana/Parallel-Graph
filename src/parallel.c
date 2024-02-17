// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS		4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;

static pthread_mutex_t sum_mutex;

/* Define graph task argument. */
struct graph_task_arg {
	unsigned int idx;
};

static void process_node(void *arg)
{
	/* Implement thread-pool based processing of graph. */
	unsigned int idx = *(unsigned int *) arg;

	// Update node status
	pthread_mutex_lock(&tp->mutex);
	if (graph->visited[idx] == NOT_VISITED) {
		graph->visited[idx] = PROCESSING;
	} else {
		pthread_mutex_unlock(&tp->mutex);
		return;
	}
	pthread_mutex_unlock(&tp->mutex);

	// Process node
	pthread_mutex_lock(&sum_mutex);
	sum += graph->nodes[idx]->info;
	pthread_mutex_unlock(&sum_mutex);

	// Update node status
	pthread_mutex_lock(&tp->mutex);
	graph->visited[idx] = DONE;
	pthread_mutex_unlock(&tp->mutex);

	// Create tasks for the neighbours
	for (unsigned int i = 0; i < graph->nodes[idx]->num_neighbours; i++) {
		unsigned int neighbour_idx = graph->nodes[idx]->neighbours[i];

		if (graph->visited[neighbour_idx] == NOT_VISITED) {
			// Create task
			struct graph_task_arg *task_arg = malloc(sizeof(struct graph_task_arg));

			DIE(task_arg == NULL, "malloc");
			task_arg->idx = neighbour_idx;
			os_task_t *task = create_task((void (*)(void *)) process_node, task_arg, free);

			// Add task to queue
			enqueue_task(tp, task);
		}
	}
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	/* Initialize graph synchronization mechanisms. */
	sum = 0;

	pthread_mutex_init(&sum_mutex, NULL);

	tp = create_threadpool(NUM_THREADS);

	unsigned int idx = 0;

	process_node(&idx);

	wait_for_completion(tp);
	destroy_threadpool(tp);

	pthread_mutex_destroy(&sum_mutex);

	printf("%d", sum);

	return 0;
}
