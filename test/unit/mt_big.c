
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <pthread_barrier.h>
#include <shmem.h>
#include <shmemx.h>

#define T 2
#define BIG_BUFF_SIZE 500000000

int dest[BIG_BUFF_SIZE] = { 0 };

int me, npes;
int errors = 0;
int smallDest = 0;
int done = 0;
pthread_barrier_t fencebar;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void * thread_main(void *arg)
{
	int tid = * (int *) arg;
	int i, val, expected;

	val = 7;
	if(tid == 0)
	{
		printf("PE0, thread 1 starting large put\n");
		shmem_int_put(dest, dest, BIG_BUFF_SIZE, 1);
	}
	else if (tid == 1)
	{
		shmem_int_put(&smallDest, &val, 1, 1);
		printf("PE0, thread 2 starting small put\n");
	}


	if(tid == 0)
	{
		shmem_quiet();
		int z = 1;
		shmem_int_put(&done,&z,1,1);
	}
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
		printf("This Test requires 2 PEs, one with mutiple threads.\n");
		shmem_finalize();
		return 0;
	}

	me = shmem_my_pe();
	npes = shmem_n_pes();

	pthread_barrier_init(&fencebar, NULL, T);

	if (me == 0) 
	{
		printf("Starting...\n");
		
		for(i = 0; i< BIG_BUFF_SIZE; i++)
		{
			dest[i] = i;
		}
		printf("Dest initialized to 1 by Pe 0 \n");

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
	}
	else
	{
		printf("PE 1 waiting for done...\n");
		
		shmem_int_wait_until(&done, SHMEM_CMP_EQ, 1);

		printf("PE 1 found done, checking dest\n");
		for(i = 0; i<BIG_BUFF_SIZE; i++)
		{
			if(dest[i] != i)
			{
				errors++;
			}
		}
	}

	pthread_barrier_destroy(&fencebar);

	if (me == 0) 
	{
		printf("Finalizing....");
		for(i = 1; i<npes; i++)
		{
			int e = 0;
			shmem_int_get(&e, &errors, 1, i);
			errors += e;
		}
		if (errors) printf("Encountered %d errors\n", errors);
		else printf("Success\n");
	}

	shmem_finalize();

	if(me == 0)printf("Done \n");
	return (errors == 0) ? 0 : 1;
}
