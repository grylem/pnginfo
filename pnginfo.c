/*
 * Copyright (c) 2018 Tristan Le Guern <tleguern@bouledef.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

const char *malloc_options = "GCFJR<<";

#include "config.h"

#include <arpa/inet.h>

#if HAVE_ERR
# include <err.h>
#endif
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lgpng.h"

void info_IHDR(struct lgpng *, uint8_t *, size_t);
void info_PLTE(struct lgpng *, uint8_t *, size_t);
void info_tRNS(struct lgpng *, uint8_t *, size_t);
void info_cHRM(struct lgpng *, uint8_t *, size_t);
void info_gAMA(struct lgpng *, uint8_t *, size_t);
void info_sBIT(struct lgpng *, uint8_t *, size_t);
void info_sRGB(struct lgpng *, uint8_t *, size_t);
void info_tEXt(struct lgpng *, uint8_t *, size_t);
void info_bKGD(struct lgpng *, uint8_t *, size_t);
void info_hIST(struct lgpng *, uint8_t *, size_t);
void info_pHYs(struct lgpng *, uint8_t *, size_t);
void info_sPLT(struct lgpng *, uint8_t *, size_t);
void info_tIME(struct lgpng *, uint8_t *, size_t);

struct chunktypemap {
	const char *const name;
	void (*fn)(struct lgpng *, uint8_t *, size_t);
} chunktypemap[CHUNK_TYPE__MAX] = {
	{ "IHDR", lgpng_parse_IHDR },
	{ "PLTE", lgpng_parse_PLTE },
	{ "IDAT", NULL },
	{ "IEND", NULL },
	{ "tRNS", lgpng_parse_tRNS },
	{ "cHRM", lgpng_parse_cHRM },
	{ "gAMA", lgpng_parse_gAMA },
	{ "iCCP", NULL },
	{ "sBIT", lgpng_parse_sBIT },
	{ "sRGB", lgpng_parse_sRGB },
	{ "iTXt", NULL },
	{ "tEXt", lgpng_parse_tEXt },
	{ "zTXt", NULL },
	{ "bKGD", lgpng_parse_bKGD },
	{ "hIST", lgpng_parse_hIST },
	{ "pHYs", lgpng_parse_pHYs },
	{ "sPLT", lgpng_parse_sPLT },
	{ "tIME", lgpng_parse_tIME },
};

int
main(int argc, char *argv[])
{
	int		 ch;
	long		 offset;
	bool		 lflag, sflag;
	enum chunktype	 cflag;
	FILE		*fflag;
	struct lgpng	*lgpng;

	cflag = CHUNK_TYPE__MAX;
	fflag = stdin;
	lflag = true;
	sflag = false;
#if HAVE_PLEDGE
	pledge("stdio rpath", NULL);
#endif
	while (-1 != (ch = getopt(argc, argv, "c:f:ls")))
		switch (ch) {
		case 'c':
			for (int i = 0; i < CHUNK_TYPE__MAX; i++) {
				if (strcmp(optarg, chunktypemap[i].name) == 0) {
					cflag = i;
					break;
				}
			}
			if (CHUNK_TYPE__MAX == cflag)
				errx(EXIT_FAILURE, "%s: invalid chunk", optarg);
			lflag = false;
			break;
		case 'f':
			if (NULL == (fflag = fopen(optarg, "r"))) {
				err(EXIT_FAILURE, "%s", optarg);
			}
			break;
		case 'l':
			lflag = true;
			break;
		case 's':
			sflag = true;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	switch (cflag) {
	case CHUNK_TYPE_IHDR:
		chunktypemap[cflag].fn = info_IHDR;
		break;
	case CHUNK_TYPE_PLTE:
		chunktypemap[cflag].fn = info_PLTE;
		break;
	case CHUNK_TYPE_tRNS:
		chunktypemap[cflag].fn = info_tRNS;
		break;
	case CHUNK_TYPE_cHRM:
		chunktypemap[cflag].fn = info_cHRM;
		break;
	case CHUNK_TYPE_gAMA:
		chunktypemap[cflag].fn = info_gAMA;
		break;
	case CHUNK_TYPE_sBIT:
		chunktypemap[cflag].fn = info_sBIT;
		break;
	case CHUNK_TYPE_sRGB:
		chunktypemap[cflag].fn = info_sRGB;
		break;
	case CHUNK_TYPE_tEXt:
		chunktypemap[cflag].fn = info_tEXt;
		break;
	case CHUNK_TYPE_bKGD:
		chunktypemap[cflag].fn = info_bKGD;
		break;
	case CHUNK_TYPE_hIST:
		chunktypemap[cflag].fn = info_hIST;
		break;
	case CHUNK_TYPE_pHYs:
		chunktypemap[cflag].fn = info_pHYs;
		break;
	case CHUNK_TYPE_sPLT:
		chunktypemap[cflag].fn = info_sPLT;
		break;
	case CHUNK_TYPE_tIME:
		chunktypemap[cflag].fn = info_tIME;
		break;
	case CHUNK_TYPE__MAX:
		break;
	default:
		errx(EXIT_FAILURE, "Unsupported chunk type");
	}

	offset = 0;
	if (false == sflag) {
		if (false == is_png(fflag)) {
			errx(EXIT_FAILURE, "not a PNG file");
		}
	} else {
		do {
			if (0 != feof(fflag) || 0 != ferror(fflag)) {
				errx(EXIT_FAILURE, "not a PNG file");
			}
			if (0 != fseek(fflag, offset, SEEK_SET)) {
				errx(EXIT_FAILURE, "not a PNG file");
			}
			offset += 1;
		} while (false == is_png(fflag));
	}
	if (NULL == (lgpng = lgpng_new())) {
		fclose(fflag);
		errx(EXIT_FAILURE, "malloc");
	}
	do {
		enum chunktype	 type;
		int32_t		 chunkz;
		uint32_t	 chunkcrc;
		uint8_t		*chunkdata;
		char		*unknown;

		chunkdata = NULL;
		unknown = NULL;
		if (-1 == (chunkz = read_next_chunk_len(fflag)))
			errx(EXIT_FAILURE, "Invalid chunk len");
		type = read_next_chunk_type(fflag, &unknown);
		if (CHUNK_TYPE__MAX == type && NULL == unknown) {
			errx(EXIT_FAILURE, "Invalid chunk type");
		}
		if (-1 == read_next_chunk_data(fflag, &chunkdata, chunkz))
			errx(EXIT_FAILURE, "Invalid chunk data");
		if (0 == (chunkcrc = read_next_chunk_crc(fflag)))
			errx(EXIT_FAILURE, "Invalid chunk crc");
		if (lflag) {
			if (NULL != unknown) {
				printf("%s (unknown %s chunk)\n", unknown,
				    is_chunk_public(unknown)
				    ? "public" : "private");
			} else {
				printf("%s\n", chunktypemap[type].name);
			}
		} else {
			if (NULL != chunktypemap[type].fn) {
				chunktypemap[type].fn(lgpng, chunkdata, chunkz);
			}
		}
		free(chunkdata);
		free(unknown);
		if (type == CHUNK_TYPE_IEND) {
			break;
		}
	} while(1);
	fclose(fflag);
	lgpng_free(lgpng);
	return(EXIT_SUCCESS);
}

int
read_next_chunk_data(FILE *f, uint8_t **data, int32_t chunkz)
{
	size_t	 bufz;
	uint8_t	*buf;

	bufz = chunkz + 1;
	if (NULL == (buf = malloc(bufz)))
		return(-1);
	if (chunkz != (int32_t)fread(buf, 1, chunkz, f)) {
		free(buf);
		return(-1);
	}
	buf[chunkz] = '\0';
	(*data) = buf;
	return(0);
}

uint32_t
read_next_chunk_crc(FILE *f)
{
	uint32_t crc = 0;

	if (1 != fread(&crc, 4, 1, f))
		return(0);
	crc = ntohl(crc);
	return(crc);
}

enum chunktype
read_next_chunk_type(FILE *f, char **unknown)
{
	size_t	i;
	char	chunk[4] = {0, 0, 0, 0};

	if (sizeof(chunk) != fread(chunk, 1, sizeof(chunk), f)) {
		warnx("Can't read a full chunk type");
		return(CHUNK_TYPE__MAX);
	}
	for (i = 0; i < sizeof(chunk); i++) {
		if (isalpha(chunk[i]) == 0)
			return(CHUNK_TYPE__MAX);
	}
	for (i = 0; i < CHUNK_TYPE__MAX; i++) {
		if (memcmp(chunk, chunktypemap[i].name, sizeof(chunk)) == 0)
			return(i);
	}
	if (NULL != unknown) {
		if (NULL == (*unknown = strndup(chunk, sizeof(chunk))))
			return(CHUNK_TYPE__MAX); /* Bof */
	}
	return(CHUNK_TYPE__MAX);
}

int32_t
read_next_chunk_len(FILE *f)
{
	uint32_t len = 0;

	if (1 != fread(&len, 4, 1, f))
		return(-1);
	len = ntohl(len);
	if (len > INT32_MAX)
		return(-1);
	return((int32_t)len);
}

bool
is_png(FILE *f)
{
	char sig[8] = {  0,  0,  0,  0,  0,  0,  0,  0};
	char png[8] = {137, 80, 78, 71, 13, 10, 26, 10};

	fread(sig, 1, sizeof(sig), f);
	if (memcmp(sig, png, sizeof(sig)) == 0)
		return(true);
	return(false);
}

void
info_IHDR(struct lgpng *ctx, uint8_t *data, size_t dataz)
{
	struct IHDR	*ihdr;

	lgpng_parse_IHDR(ctx, data, dataz);
	ihdr = ctx->ihdr;

	if (0 == ihdr->width) {
		warnx("IHDR: Invalid width 0");
	}
	if (0 == ihdr->height) {
		warnx("IHDR: Invalid height 0");
	}
	switch (ihdr->colourtype) {
	case COLOUR_TYPE_GREYSCALE:
		if (1 != ihdr->bitdepth && 2 != ihdr->bitdepth
		    && 4 != ihdr->bitdepth && 8 != ihdr->bitdepth
		    && 16 != ihdr->bitdepth) {
			warnx("IHDR: Invalid bit depth %i, should be 1, 2, 4, 8 or 16", ihdr->bitdepth);
		}
		break;
	case COLOUR_TYPE_TRUECOLOUR_ALPHA:
		/* FALLTHROUGH */
	case COLOUR_TYPE_GREYSCALE_ALPHA:
		/* FALLTHROUGH */
	case COLOUR_TYPE_TRUECOLOUR:
		if ( 8 != ihdr->bitdepth && 16 != ihdr->bitdepth) {
			warnx("IHDR: Invalid bit depth %i, should be 8 or 16",
			    ihdr->bitdepth);
		}
		break;
	case COLOUR_TYPE_INDEXED:
		if (1 != ihdr->bitdepth && 2 != ihdr->bitdepth
		    && 4 != ihdr->bitdepth && 8 != ihdr->bitdepth) {
			warnx("IHDR: Invalid bit depth %i, should be 1, 2, 4 or 8", ihdr->bitdepth);
		}
		break;
	case COLOUR_TYPE_FILLER1:
	case COLOUR_TYPE_FILLER5:
	case COLOUR_TYPE__MAX:
	default:
		warnx("IHDR: Invalid colour type %i", ihdr->colourtype);
	}
	if (COMPRESSION_TYPE_DEFLATE != ihdr->compression) {
		warnx("IHDR: Invalid compression type %i", ihdr->compression);
	}
	if (FILTER_TYPE_ADAPTIVE != ihdr->filter) {
		warnx("IHDR: Invalid filter type %i", ihdr->filter);
	}
	if (INTERLACE_METHOD_STANDARD != ihdr->interlace
	    && INTERLACE_METHOD_ADAM7 != ihdr->interlace) {
		warnx("IHDR: Invalid interlace method %i", ihdr->interlace);
	}
	printf("IHDR: width: %u\n", ihdr->width);
	printf("IHDR: height: %u\n", ihdr->height);
	printf("IHDR: bitdepth: %i\n", ihdr->bitdepth);
	printf("IHDR: colourtype: %s\n", colourtypemap[ihdr->colourtype]);
	printf("IHDR: compression: %s\n",
	    compressiontypemap[ihdr->compression]);
	printf("IHDR: filter: %s\n", filtertypemap[ihdr->filter]);
	printf("IHDR: interlace method: %s\n", interlacemap[ihdr->interlace]);
}

void
info_PLTE(struct lgpng *lgpng, uint8_t *data, size_t dataz)
{
	struct PLTE	*plte;

	lgpng_parse_PLTE(lgpng, data, dataz);
	plte = lgpng->plte;
	printf("PLTE: %zu entries\n", plte->entries);
	for (size_t i = 0; i < plte->entries; i++) {
		printf("PLTE: entry %zu: 0x%x%x%x\n", i,
		    plte->entry[i].red,
		    plte->entry[i].green,
		    plte->entry[i].blue);
	}
}

void
info_tRNS(struct lgpng *lgpng, uint8_t *data, size_t dataz)
{
	struct tRNS	*trns;

	lgpng_parse_tRNS(lgpng, data, dataz);
	trns = lgpng->trns;
	switch(lgpng->ihdr->colourtype) {
	case COLOUR_TYPE_GREYSCALE:
		printf("tRNS: gray: %u\n", lgpng->trns->gray);
		break;
	case COLOUR_TYPE_TRUECOLOUR:
		printf("tRNS: red: %u\n", lgpng->trns->red);
		printf("tRNS: green: %u\n", lgpng->trns->green);
		printf("tRNS: blue: %u\n", lgpng->trns->blue);
		break;
	case COLOUR_TYPE_INDEXED:
		for (size_t i = 0; i < lgpng->plte->entries; i++) {
			printf("tRNS: palette index %zu: %u\n", i, lgpng->trns->palette[i]);
		}
		break;
	}
}

void
info_cHRM(struct lgpng *lgpng, uint8_t *data, size_t dataz)
{
	struct cHRM	*chrm;
	double whitex, whitey, redx, redy, greenx, greeny, bluex, bluey;

	lgpng_parse_cHRM(lgpng, data, dataz);
	chrm = lgpng->chrm;
	whitex = (double)chrm->whitex / 100000.0;
	whitey = (double)chrm->whitey / 100000.0;
	redx = (double)chrm->redx / 100000.0;
	redy = (double)chrm->redy / 100000.0;
	greenx = (double)chrm->greenx / 100000.0;
	greeny = (double)chrm->greeny / 100000.0;
	bluex = (double)chrm->bluex / 100000.0;
	bluey = (double)chrm->bluey / 100000.0;
	printf("cHRM: white point x: %f\n", whitex);
	printf("cHRM: white point y: %f\n", whitey);
	printf("cHRM: red x: %f\n", redx);
	printf("cHRM: red y: %f\n", redy);
	printf("cHRM: green x: %f\n", greenx);
	printf("cHRM: green y: %f\n", greeny);
	printf("cHRM: blue x: %f\n", bluex);
	printf("cHRM: blue y: %f\n", bluey);
}

void
info_gAMA(struct lgpng *lgpng, uint8_t *data, size_t dataz)
{
	struct gAMA	*gama;
	double		 imagegama;

	lgpng_parse_gAMA(lgpng, data, dataz);
	gama = lgpng->gama;
	imagegama = (double)gama->gama / 100000.0;
	if (0 == imagegama) {
		warnx("gAMA: invalid value of 0");
	}
	printf("gAMA: image gama: %f\n", imagegama);
}

void
info_sBIT(struct lgpng *lgpng, uint8_t *data, size_t dataz)
{
	struct sBIT	*sbit;

	lgpng_parse_sBIT(lgpng, data, dataz);
	sbit = lgpng->sbit;
	switch (sbit->type) {
	case sBIT_TYPE_0:
		printf("sBIT: significant greyscale bits: %i\n",
		    sbit->sgreyscale);
		break;
	case sBIT_TYPE_4:
		printf("sBIT: significant greyscale bits: %i\n",
		    sbit->sgreyscale);
		printf("sBIT: significant alpha bits: %i\n",
		    sbit->salpha);
		break;
	case sBIT_TYPE_23:
		printf("sBIT: significant red bits: %i\n",
		    sbit->sred);
		printf("sBIT: significant green bits: %i\n",
		    sbit->sgreen);
		printf("sBIT: significant blue bits: %i\n",
		    sbit->sblue);
		break;
	case sBIT_TYPE_6:
		printf("sBIT: significant red bits: %i\n",
		    sbit->sred);
		printf("sBIT: significant green bits: %i\n",
		    sbit->sgreen);
		printf("sBIT: significant blue bits: %i\n",
		    sbit->sblue);
		printf("sBIT: significant alpha bits: %i\n",
		    sbit->salpha);
		break;
	}
}

void
info_sRGB(struct lgpng *ctx, uint8_t *data, size_t dataz)
{
	struct sRGB	*srgb;

	lgpng_parse_sRGB(ctx, data, dataz);
	srgb = ctx->srgb;
	if (srgb->intent >= RENDERING_INTENT__MAX) {
		warnx("sRGB: invalid rendering intent value");
		return;
	}
	printf("sRGB: rendering intent: %s\n",
	    rendering_intentmap[srgb->intent]);
}

void
info_tEXt(struct lgpng *lgpng, uint8_t *data, size_t dataz)
{
	struct tEXt	*text;

	lgpng_parse_tEXt(lgpng, data, dataz);
	text = lgpng->text[lgpng->textz - 1];
	printf("%s: %s\n", text->keyword, text->text);
}

void
info_bKGD(struct lgpng *lgpng, uint8_t *data, size_t dataz)
{
	struct bKGD	*bkgd;

	lgpng_parse_bKGD(lgpng, data, dataz);
	bkgd = lgpng->bkgd;
	switch (lgpng->ihdr->colourtype) {
	case COLOUR_TYPE_GREYSCALE:
		/* FALLTHROUGH */
	case COLOUR_TYPE_GREYSCALE_ALPHA:
		printf("bKGD: greyscale %u\n", bkgd->greyscale);
		break;
	case COLOUR_TYPE_TRUECOLOUR:
		/* FALLTHROUGH */
	case COLOUR_TYPE_TRUECOLOUR_ALPHA:
		printf("bKGD: rgb value 0x%x%x%x\n",
		    bkgd->rgb.red, bkgd->rgb.green, bkgd->rgb.blue);
		break;
	case COLOUR_TYPE_INDEXED:
		printf("bKGD: palette index %u\n", bkgd->paletteindex);
		break;
	}
}

void
info_hIST(struct lgpng *lgpng, uint8_t *data, size_t dataz)
{
	struct hIST	*hist;

	lgpng_parse_hIST(lgpng, data, dataz);
	hist = lgpng->hist;
	printf("hIST: %zu entries\n", lgpng->plte->entries);
	for (size_t i = 0; i < lgpng->plte->entries; i++) {
		printf("hIST: entry %zu: %i\n", i,
		    hist->frequency[i]);
	}
}

void
info_pHYs(struct lgpng *lgpng, uint8_t *data, size_t dataz)
{
	struct pHYs	*phys;

	lgpng_parse_pHYs(lgpng, data, dataz);
	phys = lgpng->phys;
	printf("pHYs: pixel per unit, X axis: %i\n", phys->ppux);
	printf("pHYs: pixel per unit, Y axis: %i\n", phys->ppuy);
	printf("pHYs: unit specifier: %s\n",
	    unitspecifiermap[phys->unitspecifier]);
}

void
info_sPLT(struct lgpng *lgpng, uint8_t *data, size_t dataz)
{
	struct sPLT	*splt;

	lgpng_parse_sPLT(lgpng, data, dataz);
	for (size_t i = 0; i < lgpng->spltz; i++) {
		splt = lgpng->splt[i];
		printf("sPLT: palette name: %s\n", splt->palettename);
		printf("sPLT: sample depth: %i\n", splt->sampledepth);
		printf("sPLT: %zu entries\n", splt->entries);
		for (size_t j = 0; j < splt->entries; j++) {
			if (8 == splt->sampledepth) {
				printf("sPLT: entry %3zu: red: 0x%02X, "
				    "green 0x%02X, blue: 0x%02X, "
				    "alpha: 0x%02X, frequency: %hi\n", j,
				    splt->entry[j].depth8.red,
				    splt->entry[j].depth8.green,
				    splt->entry[j].depth8.blue,
				    splt->entry[j].depth8.alpha,
				    splt->entry[j].depth8.frequency);
			} else {
				printf("sPLT: entry %3zu: red: 0x%04X, "
				    "green 0x%04X, blue: 0x%04X, "
				    "alpha: 0x%04X, frequency: %hi\n", j,
				    splt->entry[j].depth16.red,
				    splt->entry[j].depth16.green,
				    splt->entry[j].depth16.blue,
				    splt->entry[j].depth16.alpha,
				    splt->entry[j].depth16.frequency);
			}
		}
	}
}

void
info_tIME(struct lgpng *lgpng, uint8_t *data, size_t dataz)
{
	struct tIME	*time;

	lgpng_parse_tIME(lgpng, data, dataz);
	time = lgpng->time;
	if (time->month == 0 || time->month > 12) {
		warnx("tIME: invalid month value");
	}
	if (time->day == 0 || time->day > 31) {
		warnx("tIME: invalid day value");
	}
	if (time->hour > 23) {
		warnx("tIME: invalid hour value");
	}
	if (time->minute > 59) {
		warnx("tIME: invalid minute value");
	}
	if (time->second > 60) {
		warnx("tIME: invalid second value");
	}
	printf("tIME: %i-%i-%i %i:%i:%i\n",
	    time->year, time->month, time->day,
	    time->hour, time->minute, time->second);
}

bool
is_chunk_public(const char *type)
{
	if (islower(type[1]))
		return(false);
	return(true);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-l] [-c chunk] [-f file]\n", getprogname());
	exit(EXIT_FAILURE);
}

