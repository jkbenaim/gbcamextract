#include <errno.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>
#include "mapfile.h"
#include "err_shim.h"

void usage() {
	fprintf(stderr, "usage:  " __progname " [-f] [-r romfile] -s savefile\n");
	fprintf(stderr, "        " __progname " -h\n");
	exit(1);
}

void help() {
	fprintf(stderr, __progname " " __progversion "\n");
	fprintf(stderr, "Copoyright (C) 2013-2020 Jason Benaim et al.\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int ch;
	bool forceflag = false;
	struct MappedFile_s romfile;
	romfile.data = NULL;
	struct MappedFile_s savefile;
	savefile.data = NULL;

	while ((ch = getopt(argc, argv, "fr:s:h")) != -1)
		switch (ch) {
		case 'f':
			forceflag = true;
			break;
		case 'r':
			if (romfile.data)
				errx(1, "only one romfile may be specified");
			romfile = MappedFile_Open(optarg, false);
			if (!romfile.data)
				err(1, "couldn't open romfile: %s", optarg);
			break;
		case 's':
			if (savefile.data)
				errx(1, "only one savefile may be specified");
			savefile = MappedFile_Open(optarg, false);
			if (!savefile.data)
				err(1, "couldn't open savefile: %s", optarg);
			break;
		case 'h':
			help();
			break;
		default:
			usage();
			break;
		}
	argc -= optind;
	argv += optind;
	if (*argv != NULL)
		usage();
	if (!savefile.data) {
		warnx("must specify a savefile");
		usage();
	}

	if (savefile.data)
		MappedFile_Close(savefile);
	if (romfile.data)
		MappedFile_Close(romfile);

	return EXIT_SUCCESS;
}
