
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <pthread_barrier.h>
#include <shmem.h>
#include <shmemx.h>

#define T 8

int dest[T] = { 0 };
int flag[T] = { 0 };

int me, npes;
int errors = 0;
pthread_barrier_t fencebar;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void * thread_main(void *arg) {
	int tid = * (int *) arg;
	int i, val, expected;

	val = 1;
	int val2 = 2;

 	shmem_int_put(&dest[tid], &val2, 1, (me + 1) % npes);

 	pthread_barrier_wait(&fencebar);
 	if(0 == tid)
 	{
		shmem_fence();
	    shmem_quiet();
	}
	pthread_barrier_wait(&fencebar);

 	shmem_int_put(&dest[tid], &val, 1, (me + 1) % npes);

	pthread_barrier_wait(&fencebar);
	if (0 == tid) shmem_barrier_all();
	pthread_barrier_wait(&fencebar);
	
	expected = 1;

	if(dest[tid] != expected)
	{
		printf("Put/get test error: [PE = %d | TID = %d] -- From PE %d, got %d expected %d\n",me, tid, (me + 1) % npes, dest[tid], expected);
		pthread_mutex_lock(&mutex);
		++errors;
		pthread_mutex_unlock(&mutex);
	}

	pthread_barrier_wait(&fencebar);
	if (0 == tid) shmem_barrier_all();
	pthread_barrier_wait(&fencebar);

	return NULL;
}

int main(int argc, char **argv) {
	int tl, i;
	pthread_t threads[T];
	int       t_arg[T];

	shmemx_init_thread(SHMEMX_THREAD_MULTIPLE, &tl);

	/* If OpenSHMEM doesn't support multithreading, exit gracefully */
	if (SHMEMX_THREAD_MULTIPLE != tl) {
		printf("Warning: Exiting because threading is disabled, tested nothing\n");
		shmem_finalize();
		return 0;
	}

	me = shmem_my_pe();
	npes = shmem_n_pes();

	pthread_barrier_init(&fencebar, NULL, T);

	if (me == 0) printf("Starting multithreaded test on %d PEs, %d threads/PE\n", npes, T);

	for (i = 0; i < T; i++) {
		int err;
		t_arg[i] = i;
		err = pthread_create(&threads[i], NULL, thread_main, (void*) &t_arg[i]);
		assert(0 == err);
	}

	for (i = 0; i < T; i++) {
		int err;
		err = pthread_join(threads[i], NULL);
		assert(0 == err);
	}

	pthread_barrier_destroy(&fencebar);

	if (me == 0) {
		for(i = 1; i<npes; i++)
		{
			int e = 0;
			shmem_int_get(&e, &errors, 1, i);
			errors += e;
		}
	}

	shmem_finalize();
	if(me == 0)
	{
		if (errors) printf("Encountered %d errors\n", errors);
		else printf("Success\n");
	}
	return (errors == 0) ? 0 : 1;
}
