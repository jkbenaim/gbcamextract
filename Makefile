all: 	gbcamextract


clean:
	rm -f gbcamextract pngtest

gbcamextract:	Makefile gbcamextract.c
	gcc -ggdb -Wall -Werror -lpng -o gbcamextract gbcamextract.c

pngtest: Makefile pngtest.c
	gcc -ggdb -Wall -Werror -lpng -DPNG_NO_SETJMP -o pngtest pngtest.c