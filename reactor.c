#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>

#define WAITING_USECS 

typedef struct {
	int moving;
	int direction;
	float depth;
} piston_t;

typedef struct {
	piston_t * piston;
} thread_params_t;


pthread_t threads[8];

piston_t pistons[8];


void *piston_thread(void * params){
	printf("hello thread!\n");
}


void print_reactor_status(float k){
	printf("Reactor state variable k=%.4f\n", k);
}

int main(int argc, char* argv){
	
	thread_params_t thread_params[8];
	
	/*pthread_attr_t attr;

	pthread_attr_init(&attr);
  	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	pthread_create(&thread_1, &attr, lol_thread, NULL);
	
	pthread_join(thread_1, NULL);
	*/
	
	float base_k;
	
	float optimal_k = 1.0;
	
	float target_k = 1.0f;
	float distance_k = 0.0f;
	
	int pistons_to_move;
	
	while(1){
		
		FILE * k_file = fopen("config","r");
		fscanf(k_file, "%f", &base_k);
		fclose(k_file);
		
		distance_k = fabs(base_k - optimal_k);
		
		pistons_to_move = (distance_k + 0.1) / 0.2; 
		
		
		system("clear");
		print_reactor_status(base_k);
		printf("distance_k: %f\n", distance_k);
		printf("Pistons to move: %d\n", pistons_to_move);
		usleep(250000);
	}
	
	
}

