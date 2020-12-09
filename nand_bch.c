#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "os_swap.h"
#include "bch.h"
#include "nand_bch.h"

#define REV_TABLE_SIZE 256
#define REPEAT_TIMES	52

static unsigned char bit_reverse(unsigned char b);
static int nand_bch_calculate_ecc(struct nand_bch_control *nand, const u_char *buf, int len, u_char *code, int no_mask);
static struct nand_bch_control *nand_bch_init(struct nand_chip *nand);
static void nand_bch_free(struct nand_bch_control *nbc);

static unsigned char bit_reverse(unsigned char b)
{
	unsigned char r = 0;

	r |= (b & 0x01)<<7;
	r |= (b & 0x02)<<5;
	r |= (b & 0x04)<<3;
	r |= (b & 0x08)<<1;
	r |= (b & 0x10)>>1;
	r |= (b & 0x20)>>3;
	r |= (b & 0x40)>>5;
	r |= (b & 0x80)>>7;

	return r;
}

#if 0
static void dump(char *name, void *buf, int size)
{
	int i;
	
	printf("%s.\n\r", name);
	for(i=0; i<size; i++)
		printf("%02x ", ((char *)buf)[i]&0xff);
	printf("\n\r");
}
#endif

static int nand_bch_calculate_ecc(struct nand_bch_control *nbc, const u_char *buf, int len, u_char *code, int no_mask)
{
	unsigned int i;

	memset(code, 0, nbc->bch->ecc_bytes);
	encode_bch(nbc->bch, buf, len, code);

	/* apply mask so that an erased page is a valid codeword */
	if (!no_mask) {
		for (i = 0; i < nbc->bch->ecc_bytes; i++)
			code[i] ^= nbc->eccmask[i];
	}

	return 0;
}

static struct nand_bch_control *nand_bch_init(struct nand_chip *nand)
{
	unsigned int m, t, i;
	struct nand_bch_control *nbc = NULL;
	unsigned char *erased_page;

	if (nand == NULL)
		return NULL;

	nbc = malloc(sizeof(*nbc));
	if (nbc == NULL)
		return NULL;
	memset(nbc, 0, sizeof(*nbc));

	m = fls(1+8*nand->ecc_sector);
	t = (nand->ecc_bytes*8)/m;

	nbc->bch = init_bch(m, t, 0);
	if (nbc->bch == NULL)
		goto FAIL;

	if (nbc->bch->ecc_bytes != nand->ecc_bytes) {
		printk(KERN_WARNING "invalid eccbytes %u, should be %u\n",
			nand->ecc_bytes, nbc->bch->ecc_bytes);
		goto FAIL;
	}

	nbc->eccmask = malloc(nand->ecc_bytes);
	nbc->errloc = malloc(t*sizeof(*nbc->errloc));
	if (!nbc->eccmask || !nbc->errloc)
		goto FAIL;

	/*
	 * compute and store the inverted ecc of an erased ecc block
	 */
	erased_page = kmalloc(nand->ecc_sector, GFP_KERNEL);
	if (!erased_page)
		goto FAIL;

	memset(erased_page, 0xff, nand->ecc_sector);
	memset(nbc->eccmask, 0, nand->ecc_bytes);
	encode_bch(nbc->bch, erased_page, nand->ecc_sector, nbc->eccmask);
	kfree(erased_page);

	for (i = 0; i < nand->ecc_bytes; i++)
		nbc->eccmask[i] ^= 0xff;

	return nbc;
FAIL:
	nand_bch_free(nbc);
	return NULL;
}

static void nand_bch_free(struct nand_bch_control *nbc)
{
	if (nbc) {
		free_bch(nbc->bch);
		free(nbc->errloc);
		free(nbc->eccmask);
		free(nbc);
	}
}

int nandbch(struct nand_chip *nand, const char *file_in, const char *file_out, unsigned int flag)
{
	int ret = -1;
	int i;
	int fd_in, fd_out;
	unsigned char *buf_page, *buf_spare;
	unsigned char *rev_table = NULL;
	struct nand_bch_control *nbc_handle = NULL;

	if ((nand == NULL) || (file_in == NULL) || (file_out == NULL))
		return ret;

	fd_in = open(file_in, O_RDONLY);
	if (fd_in < 0) {
		fprintf(stderr, "%s: Error when open input file %s: ", __func__, file_in);
		perror(NULL);
		return ret;
	}

	fd_out = open(file_out, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU|S_IRUSR|S_IXUSR|S_IROTH|S_IXOTH);
	if (fd_out < 0) {
		fprintf(stderr, "%s: Error when create output file %s: ", __func__, file_out);
		perror(NULL);
		goto OUT_1;
	}

	buf_page = malloc(nand->page_size + nand->spare_size);
	if (buf_page == NULL) {
		fprintf(stderr, "%s: Error when malloc page buffer.\n", __func__);
		goto OUT_2;
	}
	buf_spare = buf_page + nand->page_size;

	if (flag & FLAG_PMECC) {
		rev_table = malloc(REV_TABLE_SIZE);
		if (rev_table == NULL) {
			fprintf(stderr, "%s: Error when malloc for reverse table.\n", __func__);
			goto OUT_3;
		}

		for (i=0; i<=REV_TABLE_SIZE; i++)
			rev_table[i] = bit_reverse(i);
	}

	nbc_handle = nand_bch_init(nand);
	if (nbc_handle == NULL) {
		fprintf(stderr, "%s: Error when get nbc handle.\n", __func__);
		goto OUT_4;
	}

	while (1) {
		if (flag & FLAG_HEADER) {
			flag &= ~FLAG_HEADER;
			for (i=0; i<REPEAT_TIMES; i++)
				((unsigned int *)buf_page)[i] = nand->boot_header;

			ret = read(fd_in, buf_page + REPEAT_TIMES*sizeof(unsigned int),
									nand->page_size - REPEAT_TIMES*sizeof(unsigned int));
			if (ret > 0)
				ret += REPEAT_TIMES*sizeof(unsigned int);
		} else
			ret = read(fd_in, buf_page, nand->page_size);

		if (ret < 0) { // Error occur
			fprintf(stderr, "%s: Error when read %s.\n", __func__, file_in);
			perror("read()");
			ret = -1;
			break;
		} else if (ret == 0) // End of file
			break;

		if (ret < nand->page_size) { // Padding 0xff, page size aligned
			memset(buf_page + ret, 0xff, nand->page_size - ret);
		}

		if (flag & FLAG_PMECC) { // PMECC uses inverted bit order
			for (i=0; i<nand->page_size; i++)
				buf_page[i] = rev_table[buf_page[i]&0xff];
		}

		memset(buf_spare, 0xff, nand->spare_size);
		if (flag & FLAG_YAFFS) { // For YAFFS image, read free region data from input file
			ret = read(fd_in, buf_spare + nand->free_offset, nand->ecc_offset - nand->free_offset);
			if (ret != (nand->ecc_offset - nand->free_offset)) {
				fprintf(stderr, "%s: Error read free region from %s.\n", __func__, file_in);
				perror("read()");
				ret = -1;
				break;
			}

			ret = lseek(fd_in, nand->spare_size - nand->ecc_offset + nand->free_offset, SEEK_CUR);
			if (ret < 0) {
				fprintf(stderr, "%s: Error lseek in %s.\n", __func__, file_in);
				perror("lseek()");
				ret = -1;
				break;
			}
		}

		for (i=0; i<nand->page_size/nand->ecc_sector; i++) // Generate ECC codes for every sector
			nand_bch_calculate_ecc(nbc_handle, buf_page+i*nand->ecc_sector,
															nand->ecc_sector, buf_spare + nand->ecc_offset + i*nand->ecc_bytes,
															flag & FLAG_NO_MASK);

		if (flag & FLAG_PMECC) {
			for (i=0; i<nand->page_size; i++) // Recovery the bit order for data area
				buf_page[i] = rev_table[buf_page[i]&0xff];

			for (i=nand->ecc_offset; i<nand->spare_size; i++) // Store ECC codes follow PMECC bit order
				buf_spare[i] = rev_table[buf_spare[i]&0xff];
		}

		ret = write(fd_out, buf_page, nand->page_size + nand->spare_size);
		if (ret != (nand->page_size + nand->spare_size)) {
			fprintf(stderr, "%s: Error when write %s.\n", __func__, file_out);
			perror("write()");
			ret = -1;
			break;
		}
	}
	
	nand_bch_free(nbc_handle);

OUT_4:
	if (flag | FLAG_PMECC)
		free(rev_table);
OUT_3:
	free(buf_page);
OUT_2:
	close(fd_out);
OUT_1:
	close(fd_in);
	return ret;
}
