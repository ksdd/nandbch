#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "os_swap.h"
#include "nand_bch.h"

static struct nand_chip nand_chips[] = {
	{
		.name = "MT29F4G08ABADAWP",
		.page_size   = 2048,
		.spare_size  = 64,
		.ecc_sector  = 512,
		.ecc_bytes   = 7,
		.ecc_addr    = -1,
		.boot_header = 0xc0902405
	}
};
#define CHIP_COUNT (sizeof(nand_chips)/sizeof(struct nand_chip))

static void usage()
{
	fprintf(stderr,
		"Usage: nandbch [OPTION] <INFILE> <OUTFILE>\n"
		"Generate OOB data which include BCH code for Nandflash production image\n"
		"\n"
		"Options:\n"
		"  -m, --model=n   Predefined Nandflash model number, start from 1\n"
		"  -p, --pmecc     Generate SAMA5Dx PMECC format BCH code\n"
		"  -b, --boot      Add boot header for AT91 Bootstrap\n"
		"  -l, --list      List predefined Nandflash models\n");
}

static void list_chips()
{
	int i;

	fprintf(stderr,
		"Nandflash models:\n");
	for (i=0; i<(sizeof(nand_chips)/sizeof(struct nand_chip)); i++) {
		fprintf(stderr,"  %d: %s\n"
			"       page_size  : %d\n"
			"       spare_size : %d\n"
			"       ecc_sector : %d\n"
			"       ecc_bytes  : %d\n"
			"       ecc_addr   : %d\n"
			"       header     : 0x%08x\n",
			i+1,
			nand_chips[i].name,
			nand_chips[i].page_size,
			nand_chips[i].spare_size,
			nand_chips[i].ecc_sector,
			nand_chips[i].ecc_bytes,
			nand_chips[i].ecc_addr,
			nand_chips[i].boot_header);
	}
}

int main(int argc, char **argv) {
	int ret;
	int chip_no = 0;
	int pmecc = 0;
	int header = 0;

	static struct option options[] = {
		{"model", required_argument, NULL, 'm'},
		{"pmecc", no_argument      , NULL, 'p'},
		{"boot" , no_argument      , NULL, 'b'},
		{"list" , no_argument      , NULL, 'l'},
		{"help" , no_argument      , NULL, 'h'},
		{0, 0, 0, 0}
	};
	char *opt_string   = "m:pblh";

	if (argc < 2) {
		usage();
		return 0;
	}

	while ((ret = getopt_long(argc, argv, opt_string, options, NULL)) >= 0) {
		switch(ret) {
			case 'm':
				chip_no = strtol(optarg, NULL, 10);
				break;
			case 'p':
				pmecc = 1;
				printf("Use PMECC format\n");
				break;
			case 'b':
				header = 1;
				printf("Add boot header\n");
				break;
			case 'l':
				list_chips();
				return 0;
			case 'h':
				usage();
				return 0;
			default:
				return 0;
		}
	}

	if ((chip_no <= 0) || (chip_no > CHIP_COUNT)) {
		fprintf(stderr, "%s: Error Nandflash model number, Use -l for help.\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	chip_no--; // Used as array index

	if (argc < (optind+2)) {
		fprintf(stderr, "%s: Error in/out file name missed, Use -h for help.\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	ret =  nandbch(&nand_chips[chip_no], argv[optind], argv[optind + 1], pmecc, header);
	if (!ret)
		printf("Done.\n");

	return ret;
}
