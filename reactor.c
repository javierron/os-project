#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define NUMBER_OF_PISTONS 16
#define THREAD_NUM 100
#define EPSILON 0.0001
#define MAXSIZE 128

typedef struct msgbuf{
    long mtype;
    char mtext[MAXSIZE];
};

typedef struct {
	int moving;
	int direction;
	float depth;
} piston_t;

typedef struct {
	piston_t * piston;
	pthread_mutex_t * mutex;
	float current_piston_delta;
	int direction;
	long time_to_wait_ns;
} thread_params_t;


pthread_t threads[100];
pthread_t listener;
pthread_mutex_t piston_mutexes[NUMBER_OF_PISTONS];
pthread_mutex_t base_k_mutex;

piston_t pistons[NUMBER_OF_PISTONS];
piston_t pistons_f[NUMBER_OF_PISTONS];

float base_k;


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

	if(p->moving){
		depth += 10.0 * (float)p->direction;
	}

	if(direction == 1){
		if(float_equal(depth, 0.0)){
			return 0.1;
		}else if(float_equal(depth, 10.0)){
			return 0.3;
		}else if(float_equal(depth, 20.0)){
			return 0.15;
		}else if(float_equal(depth, 30.0)){
			return 0.0;
		}else{
			return 0.0;
		}
	}else if(direction == -1){
		if(float_equal(depth, 0.0)){
			return 0.0;
		}else if(float_equal(depth, 10.0)){
			return -0.1;
		}else if(float_equal(depth, 20.0)){
			return -0.3;
		}else if(float_equal(depth, 30.0)){
			return -0.15;
		}else{
			return 0.0;
		}
	}
}

void *piston_thread(void * params){

	thread_params_t * p = (thread_params_t *) params;
	pthread_mutex_t * m = p->mutex;
	//pthread_mutex_t * d = p->delta_mutex;

	pthread_mutex_lock(m);

	static struct timespec wait_time;
	//wait_time.tv_nsec = WAITING_USECS * 1000;
	wait_time.tv_sec = 1;

	float delta;

	piston_t * piston = p->piston;
	
	piston->moving = 1;

	if(piston->direction != p->direction){
		printf("started changing piston direction\n");
		nanosleep(&wait_time, NULL);
		piston->direction = p->direction;
		printf("finished changing piston direction\n");
	}
	
	printf("started moving piston\n");
	delta = p->current_piston_delta;

	nanosleep(&wait_time, NULL);

	piston->depth += 10.0 * (float)(p->direction);
	printf("dir dir %d\n", p->direction);
	
	piston->moving = 0;
	printf("finished moving piston\n");

	pthread_mutex_unlock(m);
}

void * listener_thread(void * params){

	//MESSAGE QUEUE MANAGMENT////
	int msqid;
    int msgflg = IPC_CREAT | 0666;
    key_t key = 1234;
    struct msgbuf rcvbuffer;
    ////////////////////////////

    if ((msqid = msgget(key, msgflg)) < 0){
    	perror("msgget()");
	}

	while(1){
		if (msgrcv(msqid, &rcvbuffer, MAXSIZE, 1, 0) < 0){
			perror("msgrcv");
		}
 	
    	
    	float perturbation = atof(rcvbuffer.mtext);
		pthread_mutex_lock(&base_k_mutex);
    	base_k += perturbation;
		pthread_mutex_unlock(&base_k_mutex);
	}
}

float sign(float x) {
    return (x >= 0) - (x < 0);
}

int main(int argc, char* argv){
	
	char log[5000];
	int exit = 0;

	thread_params_t thread_params[THREAD_NUM];
	
	pthread_attr_t attr;

	pthread_attr_init(&attr);
  	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	
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
	
	int thread_index = 0;

	clock_t start;
	clock_t end;
    float cpu_time_used;
    double timer;

    float time_to_die;
    long time_to_wait_ns;


    FILE * f = fopen("config","r");
    fscanf(f, "%f, %ld", &time_to_die, &time_to_wait_ns);
    fclose(f);


	for (i = 0; i < NUMBER_OF_PISTONS; ++i){
	
		pistons[i].depth = 0.0;
		pistons[i].direction = 1;
		pistons[i].moving = 0;

		pistons_f[i].depth = 0.0;
		pistons_f[i].direction = 1;
		pistons_f[i].moving = 0;
	
		pthread_mutex_init(&piston_mutexes[i], NULL);
	}
	pthread_mutex_init(&base_k_mutex, NULL);

	

	base_k = 1.0;
	pthread_create(&listener, &attr, listener_thread, NULL);
	


	while(exit == 0){
		
		start = clock();

		pthread_mutex_lock(&base_k_mutex);
		
		piston_k_contribution = 0.0;
		for (i = 0; i < NUMBER_OF_PISTONS; ++i)
		{
			piston_k_contribution += get_piston_k_contribution(&pistons_f[i]);
		}

		current_k = base_k - piston_k_contribution;// - moving_delta;

		distance_k = current_k - optimal_k;
		direction = sign(distance_k);
		distance_k = fabs(distance_k);

		float current_piston_delta = calculate_piston_movement_delta_k_contribution(&pistons_f[current_piston], direction); 
		
		if(distance_k > 0.0 && current_piston_delta == 0.0){
			current_piston = (current_piston + 1) % (NUMBER_OF_PISTONS / 2);
			twin_piston = current_piston + (NUMBER_OF_PISTONS / 2);
			current_piston_delta = calculate_piston_movement_delta_k_contribution(&pistons_f[current_piston], direction); 
		

			piston_k_contribution = 0.0;
			for (i = 0; i < NUMBER_OF_PISTONS; ++i)
			{
				piston_k_contribution += get_piston_k_contribution(&pistons_f[i]);
			}

			current_k = base_k - piston_k_contribution;// - moving_delta;

			distance_k = current_k - optimal_k;
			direction = sign(distance_k);
			distance_k = fabs(distance_k);
		}

		if(fabs(distance_k) >= fabs(current_piston_delta) && current_piston_delta != 0.0){
			
			distance_k = current_k - optimal_k;
			if( float_equal(distance_k, 0.0f)){ distance_k = 0.0f;}

			direction = sign(distance_k);
			distance_k = fabs(distance_k);

			printf("d2 %d\n", direction);


			current_k = base_k - piston_k_contribution;// - moving_delta;

			int current_index = thread_index % THREAD_NUM;
			
			thread_params[current_index].piston = &pistons[current_piston];
			thread_params[current_index].direction = direction;
			thread_params[current_index].current_piston_delta = current_piston_delta;
			thread_params[current_index].mutex = &piston_mutexes[current_piston];
			thread_params[current_index].time_to_wait_ns = time_to_wait_ns;


			printf("creating thread: %d\n", current_piston );
			pistons_f[current_piston].depth += 10.0 * (float)direction;
			pistons_f[current_piston].direction = direction;
			pthread_create(&threads[current_index], &attr, piston_thread, (void *)(&thread_params[current_index]));
			

			thread_index++;
			current_index = thread_index % THREAD_NUM;
			
			thread_params[current_index].piston = &pistons[twin_piston];
			thread_params[current_index].direction = direction;
			thread_params[current_index].current_piston_delta = current_piston_delta;
			thread_params[current_index].mutex = &piston_mutexes[twin_piston];
			thread_params[current_index].time_to_wait_ns = time_to_wait_ns;

			thread_index++;

			printf("creating thread: %d\n", twin_piston );
			pistons_f[twin_piston].depth += 10.0 * (float)direction;
			pistons_f[twin_piston].direction = direction;
			pthread_create(&threads[current_index], &attr, piston_thread, (void *)(&thread_params[current_index]));
			//sprintf(log, "moving piston");

			current_piston = (current_piston + 1) % (NUMBER_OF_PISTONS / 2);
			twin_piston = current_piston + (NUMBER_OF_PISTONS / 2);
		}

		printf("base_k %f\n", base_k);
		printf("pistons_k %f\n", piston_k_contribution);
		printf("moving_k %f\n", moving_delta);
		printf("distance_k: %f\n", distance_k);
		printf("\n\n");

		int p;

		for ( p = 0; p < NUMBER_OF_PISTONS; ++p)
		{
			printf("depth: %.4f | contribution: %.4f | dir: %d | moving: %d\n", pistons[p].depth, get_piston_k_contribution(&pistons[p]), pistons[p].direction, pistons[p].moving);
		}


		printf("\n\n");
		system("clear");
		pthread_mutex_unlock(&base_k_mutex);

		end = clock();

		cpu_time_used = end - start;

		timer += cpu_time_used;

		float real_contribution = 0.0;
		for (i = 0; i < NUMBER_OF_PISTONS; ++i){
			real_contribution += get_piston_k_contribution(&pistons[i]);
		}
		float real_current_k = base_k - real_contribution;
		
		if(real_current_k < 1.5 && real_current_k > 0.5){
			timer = 0.0;
		}

		printf("time to die: %.4f | current_k_value: %.4f\n", timer / CLOCKS_PER_SEC, real_current_k);


		if(timer / CLOCKS_PER_SEC > time_to_die){
			if(real_current_k > 1.5){
				printf("Explosion de reactor.\n");
			}else if(real_current_k < 0.5){
				printf("Reactor apagado.\n");
			}
			exit = 1;
		}
	}
	
}

