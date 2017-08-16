/*
 *  Copyright (c) 2017 Intel Corporation. All rights reserved.
 *  This software is available to you under the BSD license below:
 *
 *      Redistribution and use in source and binary forms, with or
 *      without modification, are permitted provided that the following
 *      conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Multithreaded Concurrent Puts Test
 * Alex McKinley <alex.mckinley@intel.com>
 * August, 2017
 */


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <pthread_barrier.h>
#include <shmem.h>
#include <shmemx.h>
#include <unistd.h>

#define T 2
#define BIG_BUFF_SIZE 2000000

int dest[BIG_BUFF_SIZE] = {0};

int me, npes;
int errors = 0;
int smallDest = 0;
int done = 0;
pthread_barrier_t fencebar;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void * thread_main(void *arg)
{
	int tid = * (int *) arg;
	int val = 7; //arbitrary

	//Set up a large put and a small put to create the race condition in the put counter.
	if(tid == 0)
	{	
		shmem_int_put(dest, dest, BIG_BUFF_SIZE, 1);
	}
	else if (tid == 1)	
	{
		shmem_int_put(&smallDest, &val, 1, 1);
	}
	pthread_barrier_wait(&fencebar);
	if(tid == 0)
	{
		shmem_quiet();

		//this is the crux of the test. If the put counter is working correctly, this line should have no effect on the execution.
		//If there is an error, this value will be copied over to PE 1 and cause the program to output an error.
		dest[BIG_BUFF_SIZE-1] = -1; 
		
		int z = 1;
		shmem_int_put(&done,&z,1,1);
	}
	pthread_barrier_wait(&fencebar);

	return NULL;
}
int main(int argc, char **argv) 
{
	int tl, i;
	pthread_t threads[T];
	int       t_arg[T];

	shmemx_init_thread(SHMEMX_THREAD_MULTIPLE, &tl);

	/* If OpenSHMEM doesn't support multithreading, exit gracefully */
	if (SHMEMX_THREAD_MULTIPLE != tl) {
		printf("This Test requires 2 PEs, one with two threads.\n");
		shmem_finalize();
		return 0;
	}

	me = shmem_my_pe();
	npes = shmem_n_pes();

	pthread_barrier_init(&fencebar, NULL, T);

	if (me == 0) 
	{
		printf("Starting on size of %i...\n", BIG_BUFF_SIZE);
		
		//initialize the dest array to hold all of the values to send to PE 1
		for(i = 0; i< BIG_BUFF_SIZE; i++)
		{
			dest[i] = i;
		}
		//spin off threads to start test
		for (i = 0; i < T; i++) 
		{
			int err;
			t_arg[i] = i;
			err = pthread_create(&threads[i], NULL, thread_main, (void*) &t_arg[i]);
			assert(0 == err);
		}

		for (i = 0; i < T; i++) 
		{
			int err;
			err = pthread_join(threads[i], NULL);
			assert(0 == err);
		}
	}
	else if(me == 1)
	{	
		//PE 1 waits until PE 0 says its done sending data to it. In theory, PE 1 - dest should just have values from
		//0 to BIG_BUFF_SIZE in it. PE 1 will check its Dest to make sure that everything is correct.	
		shmem_int_wait_until(&done, SHMEM_CMP_EQ, 1);
		//spit out errors to see if test was successful. 
		for(i = BIG_BUFF_SIZE-1; i>=0; i--)
		{
			if(dest[i] != i)
			{
				shmem_int_add(&errors, 1, 0);
				printf("%i: %i!\n",i, dest[i]);			}
		}
	}

	shmem_barrier_all();
	pthread_barrier_destroy(&fencebar);

	//sum up all the errors and print a final report.
	if (me == 0) 
	{
		printf("Finalizing... ");
		if (errors) printf("Encountered %d error(s)!\n", errors);
		else printf("Success\n");
	}

	shmem_finalize();

	if(me == 0)printf("Done \n");
	return (errors == 0) ? 0 : 1;
}
