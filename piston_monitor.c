#include <stdio.h>
#include <sys/file.h>

int main(int argc, char const *argv[]){
	
	char output[100];

	while(1){

		system("clear");

		FILE * f = fopen("pistons","r");
		flock(fileno(f), LOCK_SH);

		while(fgets(output, 100, f) != NULL){
			printf("%s", output);
		}

		flock(fileno(f), LOCK_UN);
		
		fclose(f); 
	}	
}