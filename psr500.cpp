// Copyright (C) 2025 Theo Niessink <theo@taletn.com>
// This work is free. You can redistribute it and/or modify it under the
// terms of the Do What The Fuck You Want To Public License, Version 2,
// as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WDL/heapbuf.h"
#include "WDL/wdlendian.h"
#include "WDL/wavwrite.h"

int read_firmware(const char* const filename, WDL_HeapBuf* const buf, const int size)
{
	void* const ptr = buf->ResizeOK(size);
	if (!ptr) return 0;

	FILE* const fp = fopen(filename, "rb");
	if (!fp) return 0;

	const int n = (int)fread(ptr, 1, size, fp);
	fclose(fp);

	return n == size ? size : 0;
}

int read_wavetbl(const char* const filename, WDL_HeapBuf* const buf, const int size)
{
	return read_firmware(filename, buf, size);
}

/* Wavetables

Offset  | Size | Data            | Parameter                    | Description              | Default
--------+------+-----------------+------------------------------+--------------------------+---------
+0      | 1    | Bit 6  - 7      | Sample format                | 3:12-bit                 |
+0      | 3    | 000000 - 3FFFFF | Start address                | 2 MB wave ROM address    | 000000
+3      | 2    | 0000   - FFFF   | Loop address                 | 0 - 65535 [samples]      | 0000
+5      | 2    | 0000   - FFFF   | End address                  | 1 - 65536 [samples]      | 0000
+7      | 1    | Bit 3  - 5      | LFO speed                    | 0 - 7                    | 00
        |      | Bit 0  - 2      | Vibrato depth                | 0 - 7                    |
+8      | 1    | Bit 4  - 7      | Attack rate                  | 0 - 15                   | 00
        |      | Bit 0  - 3      | Decay 1 rate                 | 0 - 15                   |
+9      | 1    | Bit 4  - 7      | Decay level                  | 0 - 15                   | 00
        |      | Bit 0  - 3      | Decay 2 rate                 | 0 - 15                   |
+10     | 1    | Bit 4  - 7      | Rate correction              | 0 - 15                   | 00
        |      | Bit 0  - 3      | Release rate                 | 0 - 15                   |
+11     | 1    | Bit 0  - 2      | Tremolo depth                | 0 - 7                    | 00

*/
void print_wavetbl(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Wavetables (+%d)\n", ofs);

	static const char* const hdr  = "Offset  | F | Addr   | Loop  | End   | Sp | PM | AR | D1R | DL | D2R | RC | RR | AM ";
	static const char* const line = "--------+---+--------+-------+-------+----+----+----+-----+----+-----+----+----+----";

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;
	ofs = 0;

	for (int i = 0; i < num; ++i)
	{
		if (!(i % 25))
		{
			if (!i) putchar('\n'); else puts(line);
			puts(hdr);
			puts(line);
		}

		printf("%-+7d | ", ofs);

		const int format = ptr[0] >> 6;
		const int addr = ((ptr[0] & 0x3F) << 16) | (ptr[1] << 8) | ptr[2];
		const int loop = (ptr[3] << 8) | ptr[4];

		int end = (ptr[5] << 8) | ptr[6];
		end ^= addr ? 0xFFFF : 0;

		printf("%d | %06X | ", format, addr);
		printf("%-5d | %-5d | ", loop, end);

		printf("%d  | %d  | ", (ptr[7] >> 3) & 0x07, ptr[7] & 0x07);
		printf("%-2d | %-2d  | ", ptr[8] >> 4, ptr[8] & 0x0F);
		printf("%-2d | %-2d  | ", ptr[9] >> 4, ptr[9] & 0x0F);
		printf("%-2d | %-2d | ", ptr[10] >> 4, ptr[10] & 0x0F);
		printf("%-2d \n", ptr[11] & 0x07);

		ptr += 12;
		ofs += 12;
	}
}

// https://bleepsandpops.com/post/37792760450/adding-cue-points-to-wav-files-in-c

void write_cue_points(WaveWriter* const wav, const int loop)
{
	static const int num = 1;
	int cue[3 + num * 6];

	cue[0] = WDL_bswap32_if_le('cue ');
	cue[1] = WDL_bswap32_if_be((1 + num * 6) * sizeof(int));
	cue[2] = WDL_bswap32_if_be(num);

	cue[3] = 0;
	cue[4] = 0;
	cue[5] = WDL_bswap32_if_le('data');
	cue[6] = 0;
	cue[7] = 0;
	cue[8] = WDL_bswap32_if_be(loop);

	wav->WriteChunk(cue, (3 + num * 6) * sizeof(int));
}

int write_sample(const char* const filename, const WDL_HeapBuf* const wavetbl, const int format, const int addr, int loop, const int end)
{
	const unsigned char* const buf = (const unsigned char*)wavetbl->Get() + addr;
	int len = end + 1;

	WaveWriter wav;
	if (!wav.Open(filename, 16, 1, 41964, 0)) return 0;

	switch (format)
	{
		// 12-bit signed linear PCM
		case 3:
		{
			const unsigned char* ptr = buf;

			for (int i = 0; i < len; ++i)
			{
				short sample;

				if (!(i & 1))
				{
					sample = (ptr[0] << 8) | (ptr[1] & 0xF0);
				}
				else
				{
					sample = ((ptr[1] & 0x0F) << 4) | (ptr[2] << 8);
					ptr += 3;
				}

				sample = WDL_bswap16_if_be(sample);
				wav.WriteRaw(&sample, 2);
			}
			break;
		}

		default: assert(false);
	}

	wav.EndDataChunk();
	if (loop) write_cue_points(&wav, loop);
	wav.Close();

	return len;
}

int extract_sample(const char* const filename, const int ofs, const WDL_HeapBuf* const wavetbl, const unsigned char* const ptr)
{
	const int format = ptr[0] >> 6;
	const int addr = ((ptr[0] & 0x3F) << 16) | (ptr[1] << 8) | ptr[2];

	if (!addr) return 0;

	const int loop = (ptr[3] << 8) | ptr[4];
	const int end = ((ptr[5] << 8) | ptr[6]) ^ 0xFFFF;

	#ifndef MUTABLE_EXTRACT_DUPLICATES

	static const int max_samples = 210;
	static unsigned char sample_list[max_samples][7];

	static int num_samples = 0;
	const unsigned char* const& hash = ptr;

	for (int i = 0; i < num_samples; ++i)
	{
		if (!memcmp(sample_list[i], hash, 7)) return 0;
	}

	assert(num_samples < max_samples);
	memcpy(sample_list[num_samples++], hash, 7);

	#endif

	char fn[128];
	sprintf(fn, filename, ofs);

	return write_sample(fn, wavetbl, format, addr, loop, end);
}

int extract_samples(const char* const filename, const WDL_HeapBuf* const wavetbl, const int ofs, const int num)
{
	const unsigned char* ptr = (const unsigned char*)wavetbl->Get() + ofs;
	int n = 0;

	for (int i = 0; i < num; ++i)
	{
		n += extract_sample(filename, i * 12, wavetbl, ptr) > 0;
		ptr += 12;
	}

	return n;
}

int main(const int argc, const char* const* const argv)
{
	int opt = argc == 2 ? argv[1][0] : 0;
	if (opt == '-') opt = argv[1][1] | ('-' << 8);

	static const char* const roms[] =
	{
		// IC3 XJ426B00 ROM 8M
		"psr500/xj426b0.ic3" // SHA1(864f5689dbaa82bd8a1be4e53bdb21ec71be03cc)
	};

	WDL_HeapBuf wavetbl;

	if (!read_wavetbl(roms[0], &wavetbl, 1*1024*1024))
	{
		return EXIT_FAILURE;
	}

	if (opt == '-t')
	{
		printf("PSR-500 Data Tables\n\n");
		print_wavetbl(&wavetbl, +0, 512);

		return EXIT_SUCCESS;
	}

	if (opt == '-w')
	{
		const int n = extract_samples("wave/psr500/sample_%04d.wav", &wavetbl, +0, 512);
		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	printf("Usage: %s -t | -w\n", argv[0]);
	return EXIT_FAILURE;
}
