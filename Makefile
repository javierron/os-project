all: reactor perturbation monitor

reactor: reactor.c
	gcc -o reactor reactor.c -pthread
perturbation: reactor_perturbation.c
	gcc -o pert reactor_perturbation.c
monitor: piston_monitor.c
	gcc -o monitor piston_monitor.c

clean:
	rm reactor
	rm pert
	rm monitor