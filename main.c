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

static struct nand_chip chips[] = {
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
#define CHIP_COUNT (sizeof(chips)/sizeof(struct nand_chip))

static void usage()
{
	fprintf(stderr,
		"Usage: nandbch [OPTION] <INFILE> <OUTFILE>\n"
		"Generate OOB data which include BCH code for NAND Flash production image\n"
		"\n"
		"Options:\n"
		"  -m, --model=n     Use predefined NAND Flash model, model number start from 1\n"
		"  -i, --input       Use input NAND Flash parameter\n"
		"      --page-size   NAND Flash page size\n"
		"      --spare-size  NAND Flash spare size\n"
		"      --ecc-sector  ECC sector size\n"
		"      --ecc-bytes   ECC bytes per sector\n"
		"      --ecc-addr    ECC code address in spare area\n"
		"      --boot-header NAND Flash boot header, check SAMA5Dx datasheet\n"
		"  -p, --pmecc       Generate SAMA5Dx PMECC format BCH code\n"
		"  -b, --boot        Add boot header for AT91 Bootstrap\n"
		"  -l, --list        List predefined NAND Flash models\n");
}

static void dump_chips(struct nand_chip (*chips)[], int count, int index)
{
	int i;

	for (i=0; i<count; i++) {
		if (index)
			fprintf(stderr, "%d:\n", i);
		fprintf(stderr,"  %s\n"
			"  page_size  : %d\n"
			"  spare_size : %d\n"
			"  ecc_sector : %d\n"
			"  ecc_bytes  : %d\n"
			"  ecc_addr   : %d\n"
			"  boot_header: 0x%08x\n",
			(*chips)[i].name,
			(*chips)[i].page_size,
			(*chips)[i].spare_size,
			(*chips)[i].ecc_sector,
			(*chips)[i].ecc_bytes,
			(*chips)[i].ecc_addr,
			(*chips)[i].boot_header);
	}
}

int main(int argc, char **argv) {
	int ret;
	int chip_no   = 0;
	int pmecc     = 0;
	int header    = 0;
	int use_input = 0;
	int use_model = 0;
	static int lopt;
	struct nand_chip chip = {"NAND Flash parameter"};

	static struct option options[] = {
		{"model"      , required_argument, NULL , 'm'},
		{"input"      , no_argument      , NULL , 'i'},
		{"page-size"  , required_argument, &lopt,  1 },
		{"spare-size" , required_argument, &lopt,  2 },
		{"ecc-sector" , required_argument, &lopt,  3 },
		{"ecc-bytes"  , required_argument, &lopt,  4 },
		{"ecc-addr"   , required_argument, &lopt,  5 },
		{"boot-header", required_argument, &lopt,  6 },
		{"pmecc"      , no_argument      , NULL , 'p'},
		{"boot"       , no_argument      , NULL , 'b'},
		{"list"       , no_argument      , NULL , 'l'},
		{"help"       , no_argument      , NULL , 'h'},
		{0, 0, 0, 0}
	};
	char *opt_string   = "m:ipblh";

	if (argc < 2) {
		usage();
		return -1;
	}

	while ((ret = getopt_long(argc, argv, opt_string, options, NULL)) >= 0) {
		switch (ret) {
			case 'm':
				use_model = 1;
				chip_no = strtol(optarg, NULL, 10);
				if ((chip_no <= 0) || (chip_no > CHIP_COUNT)) {
					fprintf(stderr, "%s: Error NAND Flash model number, Use -l for help.\n", argv[0]);
					exit(EXIT_FAILURE);
				}
				break;
			case 'i':
				use_input = 1;
				break;
			case 'p':
				pmecc = 1;
				fprintf(stderr, "Use PMECC format\n");
				break;
			case 'b':
				header = 1;
				fprintf(stderr, "Add boot header\n");
				break;
			case 'l':
				fprintf(stderr, "NAND Flash models:\n");
				dump_chips(&chips, CHIP_COUNT, 1);
				return 0;
			case 'h':
				usage();
				return 0;
			case 0:
				switch (lopt) {
					case 1:
						chip.page_size = strtol(optarg, NULL, 10);
						break;
					case 2:
						chip.spare_size = strtol(optarg, NULL, 10);
						break;
					case 3:
						chip.ecc_sector = strtol(optarg, NULL, 10);
						break;
					case 4:
						chip.ecc_bytes = strtol(optarg, NULL, 10);
						break;
					case 5:
						chip.ecc_addr = strtol(optarg, NULL, 10);
						break;
					case 6:
						chip.boot_header = strtol(optarg, NULL, 16);
						break;
					default:
						return -1;
				}
				break;
			default:
				return -1;
		}
	}

	if (argc < (optind+2)) {
		fprintf(stderr, "%s: Error in/out file name missed, Use -h for help.\n", argv[0]);
		return -1;
	}

	if (!use_model && !use_input) {
		fprintf(stderr, "Unspecified input source of parameters for NAND Flash, use -m or -i to specify it\n");
		return -1;
	} else if (use_model && use_input) {
		fprintf(stderr, "Confused input source of parameters for NAND Flash, -m and -i couldn't be used in the same time\n");
		return -1;
	} else if (use_model) {
		fprintf(stderr, "Use predefined NAND Flash model %d:\n", chip_no);
		chip = chips[--chip_no];
	} else if (use_input) {
		fprintf(stderr, "Use input NAND Flash parameter:\n");
	}

	if (chip.ecc_addr == -1) { // Use right-aligned
		chip.ecc_addr = chip.spare_size - (chip.page_size/chip.ecc_sector*chip.ecc_bytes);
	}

	dump_chips((struct nand_chip (*)[])&chip, 1, 0);

	ret = nandbch(&chip, argv[optind], argv[optind + 1], pmecc, header);
	if (!ret)
		fprintf(stderr, "Done.\n");

	return ret;
}
