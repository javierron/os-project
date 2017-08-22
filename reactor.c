#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include <semaphore.h>

#define WAITING_USECS 100000
#define NUMBER_OF_PISTONS 16
#define THREAD_NUM 100
#define EPSILON 0.0001

typedef struct {
	int moving;
	int direction;
	float depth;
} piston_t;

typedef struct {
	piston_t * piston;
	float * moving_delta;
	int direction;
} thread_params_t;


pthread_t threads[100];
pthread_mutex_t moving_delta_mutex;
sem_t piston_sem;

piston_t pistons[NUMBER_OF_PISTONS];

int float_equal(float a, float b){

	float ab = fabs(a - b);
	return ab < EPSILON;

}


float get_piston_k_contribution(piston_t * p){

	float depth = p->depth;
	if(depth == 0.0){
		return 0.0;
	}else if(depth == 10.0){
		return 0.1;
	}else if(depth == 20.0){
		return 0.4;
	}else if(depth >= 30.0){
		return 0.55;
	}
}

float calculate_piston_movement_delta_k_contribution(piston_t * p, int direction){
	float depth = p->depth;
	
	if(direction == 1){
		if(depth == 0.0){
			return 0.1;
		}else if(depth == 10.0){
			return 0.3;
		}else if(depth == 20.0){
			return 0.15;
		}else if(depth == 30.0){
			return 0.0;
		}
	}else if(direction == -1){
		if(depth == 0.0){
			return 0.0;
		}else if(depth == 10.0){
			return 0.1;
		}else if(depth == 20.0){
			return 0.3;
		}else if(depth == 30.0){
			return 0.15;
		}
	}
}

void *piston_thread(void * params){

	sem_wait(&piston_sem);

	float delta;

	thread_params_t * p = (thread_params_t *) params;
	piston_t * piston = p->piston;

	if(piston->direction != p->direction){
		printf("started changing piston direction\n");
		usleep(WAITING_USECS);
		piston->direction = p->direction;
		printf("finished changing piston direction\n");
	}
	
	printf("started moving piston\n");
	piston->moving = 1;
	delta = calculate_piston_movement_delta_k_contribution(piston, p->direction);

	usleep(WAITING_USECS);
	piston->depth += 10.0 * (float)(p->direction);
	
	pthread_mutex_lock(&moving_delta_mutex);
	*(p->moving_delta) -= delta;
	//printf("moving_delta -%f\n", delta);
	pthread_mutex_unlock(&moving_delta_mutex);
	piston->moving = 0;
	printf("finished moving piston\n");

	sem_post(&piston_sem);
}

float sign(float x) {
    return (x >= 0) - (x < 0);
}

int main(int argc, char* argv){
	
	char log[5000];

	sem_init(&piston_sem, 0, NUMBER_OF_PISTONS);

	thread_params_t thread_params[THREAD_NUM];
	
	pthread_attr_t attr;

	pthread_attr_init(&attr);
  	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	pthread_mutex_init(&moving_delta_mutex, NULL); 

	
	float base_k;
	float optimal_k = 1.0;
	float current_k = base_k;

	float distance_k = 0.0;
	float piston_k_contribution = 0.0;
	float moving_delta = 0.0;
	
	int direction;
	int pistons_to_move;
	int i;

	int current_piston = 0;
	int twin_piston = (current_piston + (NUMBER_OF_PISTONS / 2));

	for (i = 0; i < NUMBER_OF_PISTONS; ++i){
		pistons[i].depth = 0.0;
		pistons[i].direction = 1;
		pistons[i].moving = 0;
	}
	
	while(1){
		
		FILE * k_file = fopen("config","r");
		fscanf(k_file, "%f", &base_k);
		fclose(k_file);

		int thread_index = 0;
		
		piston_k_contribution = 0.0;
		for (i = 0; i < NUMBER_OF_PISTONS; ++i)
		{
			piston_k_contribution += get_piston_k_contribution(&pistons[i]);
		}

		pthread_mutex_lock(&moving_delta_mutex);
		current_k = base_k - piston_k_contribution - moving_delta;
		pthread_mutex_unlock(&moving_delta_mutex);

		distance_k = current_k - optimal_k;
		direction = sign(distance_k);
		distance_k = fabs(distance_k);
		
		if(fabs(distance_k) >= fabs(calculate_piston_movement_delta_k_contribution(&pistons[current_piston], direction))
			&& calculate_piston_movement_delta_k_contribution(&pistons[current_piston], direction) != 0.0){
			
			printf("direction: %d\n", direction );
			printf("distance_k: %.16f\n", fabs(distance_k) );
			printf("calculated k: %.16f\n", fabs(calculate_piston_movement_delta_k_contribution(&pistons[current_piston], direction) ));
			//printf("moving_delta +%f\n", calculate_piston_movement_delta_k_contribution(&pistons[0], direction) * 2.0);
			
			distance_k = current_k - optimal_k;
			if( float_equal(distance_k, 0.0f)){ distance_k = 0.0f;}

			direction = sign(distance_k);
			distance_k = fabs(distance_k);

			pthread_mutex_lock(&moving_delta_mutex);
			moving_delta += calculate_piston_movement_delta_k_contribution(&pistons[current_piston], direction) * 2.0;
			current_k = base_k - piston_k_contribution - moving_delta;
			pthread_mutex_unlock(&moving_delta_mutex);

			int current_index = thread_index % THREAD_NUM;
			
			thread_params[current_index].piston = &pistons[current_piston];
			thread_params[current_index].moving_delta = &moving_delta;
			thread_params[current_index].direction = direction;

			pthread_create(&threads[current_index], &attr, piston_thread, (void *)(&thread_params[current_index]));
			
			thread_index++;
			current_index = thread_index % THREAD_NUM;
			
			thread_params[current_index].piston = &pistons[twin_piston];
			thread_params[current_index].moving_delta = &moving_delta;
			thread_params[current_index].direction = direction;

			thread_index++;

			pthread_create(&threads[current_index], &attr, piston_thread, (void *)(&thread_params[current_index]));
			//sprintf(log, "moving piston");

			current_piston = (current_piston + 1) % (NUMBER_OF_PISTONS / 2);
			twin_piston = current_piston + (NUMBER_OF_PISTONS / 2);
		}

		
		//system("clear");
		// print_reactor_status(target_k);
		printf("base_k %f\n", base_k);
		printf("current_k %f\n", current_k);
		printf("pistons_k %f\n", piston_k_contribution);
		printf("moving_k %f\n", moving_delta);
		printf("distance_k: %f\n", distance_k);
		//printf("\n%s\n", log);
		printf("\n\n");

		int p;

		for ( p = 0; p < NUMBER_OF_PISTONS; ++p)
		{
			printf("depth: %.16f | contribution: %.16f | moving: %d\n",  pistons[p].depth, get_piston_k_contribution(&pistons[p]), pistons[p].moving);
		}

		if(distance_k > 3.0f) exit(1);

		printf("\n\n");

		usleep(250000);
	}
	
	
}

