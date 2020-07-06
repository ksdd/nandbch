#ifndef _NAND_BCH_H
#define _NAND_BCH_H

struct nand_chip {
	char *name;
	int  page_size;
	int  spare_size;
	int  ecc_sector;
	int  ecc_bytes;
	int  ecc_offset;  /*
                     * ECC region offset address in the OOB area
                     * -1 means right-aligned
                     * used as array index which start from 0
                     */
	int  free_offset; /*
                     * Free region start address in the OOB area
                     * used as array index which start from 0
                     */
	unsigned int boot_header; /* 
                             * NAND Flash and PMECC parameter header
                             * Check SAMA5Dx datasheet for more information.
                             */
};

/**
 * struct nand_bch_control - private NAND BCH control structure
 * @bch:       BCH control structure
 * @errloc:    error location array
 * @eccmask:   XOR ecc mask, allows erased pages to be decoded as valid
 */
struct nand_bch_control {
	struct bch_control   *bch;
	unsigned int         *errloc;
	unsigned char        *eccmask;
};

#define FLAG_PMECC   0x01
#define FLAG_HEADER  0x02
#define FLAG_YAFFS   0x04
#define FLAG_NO_MASK 0x08

int nandbch(struct nand_chip *nand, const char *file_in, const char *file_out, unsigned int flag);

#endif /* _NAND_BCH_H */
