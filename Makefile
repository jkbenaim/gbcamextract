all: 	gbcamextract


clean:
	rm -f gbcamextract

gbcamextract:	Makefile gbcamextract.c
	gcc -Wall gbcamextract.c -lpng -o gbcamextract