// Copyright (C) 2019-2025 Theo Niessink <theo@taletn.com>
// This work is free. You can redistribute it and/or modify it under the
// terms of the Do What The Fuck You Want To Public License, Version 2,
// as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WDL/heapbuf.h"
#include "WDL/wdlendian.h"
#include "WDL/wdltypes.h"
#include "WDL/wavwrite.h"

int read_firmware(const char* const filenames[2], WDL_HeapBuf* const buf, const int num, const int size)
{
	unsigned char* ptr = (unsigned char*)buf->ResizeOK((num + 1) * size);
	if (!ptr) return 0;

	unsigned char* const tmp = ptr + num * size;
	int total = 0;

	for (int i = 0; i < num; ++i)
	{
		FILE* const fp = fopen(filenames[i], "rb");
		if (!fp) return 0;

		const int n = (int)fread(tmp, 1, size, fp) & ~1;
		fclose(fp);

		for (int j = 0; j < n; j += 2)
		{
			unsigned short word;
			memcpy(&word, &tmp[j], 2);

			word = WDL_bswap16(word);
			memcpy(ptr, &word, 2);

			ptr += num * 2;
		}

		total += n;
		ptr -= n * num - 2;
	}

	buf->Resize(num * size);

	return total == buf->GetSize() ? total : 0;
}

int write_firmware(const char* const filename, const WDL_HeapBuf* const buf)
{
	FILE* const fp = fopen(filename, "wb");
	if (!fp) return 0;

	const int n = (int)fwrite(buf->Get(), 1, buf->GetSize(), fp);
	fclose(fp);

	return n;
}

int write_midi(const char* const filename, const WDL_HeapBuf* const firmware, const int ofs)
{
	FILE* const fp = fopen(filename, "wb");
	if (!fp) return 0;

	static const unsigned char hdr[] =
	{
		'M', 'T', 'h', 'd', 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x01, 0xE0, // 480 TPQN
		'M', 'T', 'r', 'k', 0x00, 0x00, 0x00, 0x00
	};

	fwrite(hdr, sizeof(hdr), 1, fp);

	const unsigned char* buf = (const unsigned char*)firmware->Get() + ofs;
	int size;

	memcpy(&size, buf, 4);
	buf += 8;
	size = WDL_bswap32_if_le(size);

	while (size)
	{
		const int block = wdl_min(size, 252);
		fwrite(buf, block, 1, fp);

		buf += 256;
		size -= block;
	}

	static const int m = sizeof(hdr);
	int n = (int)ftell(fp) - m;
	n = WDL_bswap32_if_le(n);

	fseek(fp, m - 4, SEEK_SET);
	fwrite(&n, 4, 1, fp);
	fclose(fp);

	n = WDL_bswap32_if_le(n);
	n += m;

	return n;
}

void print_drum_banks(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("Drum Banks (+%d)\n", ofs);

	static const char* const name[] =
	{
		"MU Basic",
		"MU100 Native",
		"SFX",
		"TG300B",
		"GM2",
		"GM1"
	};

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		printf("\nDrum Bank %d", i);
		if (i < sizeof(name) / sizeof(name[0])) printf(" (%s)", name[i]);
		puts("\n");

		printf("Prog# "); for (int j = 1; j <= 10; ++j) printf("| %-3d ", j);
		printf("\n------"); for (int j = 1; j <= 10; ++j) printf("+-----");

		for (int j = 0; j < 128; ++j)
		{
			if (!(j % 10)) printf("\n%-5d ", j);

			const int drum_kit_no = *ptr++;
			printf("| %-3d ", drum_kit_no);
		}

		putchar('\n');
	}
}

void print_drum_kits(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("Drum Kits (+%d)\n", ofs);

	static const char* const name[] =
	{
		"XG Standard Kit",
		"XG Standard Kit 2",
		"XG Room Kit",
		"XG Rock Kit",
		"XG Electro Kit",
		"XG Analog Kit",
		"XG Jazz Kit",
		"XG Brush Kit",
		"XG Symphony Kit",
		"XG Dry Kit",
		"XG Jungle Kit",
		"XG Analog Kit 2",
		"XG Hip Hop Kit",
		"XG Dance Kit",
		"XG Bright Kit",
		"XG Dark Room Kit",
		"XG Rock Kit 2",
		"XG Jazz Kit 2",
		"XG Skim Kit",
		"XG Slim Kit",
		"XG Tramp Kit",
		"XG Amber Kit",
		"XG Coffin Kit",
		"XG Rogue Kit",
		"XG Hob Kit",
		"XG Apogee Kit",
		"XG Perigee Kit",
		"XG Brush Kit 2",
		"XG Techno Kit K/S",
		"XG Techno Kit Hi",
		"XG Techno Kit Lo",
		"XG Sakura Kit",
		"XG Small Latin Kit",
		"XG China Kit",
		"XG Natural Kit",
		"XG Natural Funk Kit",
		"MU100 Native Kit",
		"TG300B Standard Kit",
		"TG300B Room Kit",
		"TG300B Power Kit",
		"TG300B Electro Kit",
		"TG300B Analog Kit",
		"TG300B Jazz Kit",
		"TG300B Brush Kit",
		"TG300B Orchestra Kit",
		"TG300B C/M Kit",
		"TG300B SFX Set",
		"XG SFX Kit 1",
		"XG SFX Kit 2",
		"Silent Kit",
		"GM2 Standard Kit",
		"GM Room Kit",
		"GM Rock Kit",
		"GM Electronic Kit",
		"GM Analog Kit",
		"GM Jazz Kit",
		"GM Brush Kit",
		"GM Orchestra Kit",
		"GM SFX Kit",
		"GM1 Standard Kit"
	};

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		printf("\nDrum Kit %d", i);
		if (i < sizeof(name) / sizeof(name[0])) printf(" (%s)", name[i]);
		puts("\n");

		printf("Note# "); for (int j = 0; j < 10; ++j) printf("| %-6d ", j);
		printf("\n------"); for (int j = 0; j < 10; ++j) printf("+--------");

		for (int j = 0; j < 128; ++j)
		{
			if (!(j % 10)) printf("\n%-5d ", j);

			const int drum_voice_ofs = (ptr[0] << 8) | ptr[1];
			printf("| %-+6d ", drum_voice_ofs);

			ptr += 2;
		}

		putchar('\n');
	}
}

/* Drum Voices

Offset  | Size | Data            | Parameter                 | Description                 | Default
--------+------+-----------------+---------------------------+-----------------------------+---------
+0      | 1    | 29     - 43     | ?                         | ?                           | 40
+1      | 1    | 00     - 7F     | Pitch fine                | -64 - +63 [cent]            | 40
+2      | 1    | 00     - 7F     | Instrument level          | 0 - 127                     | 7F
+3      | 1    | 00     - 7F     | Alternate group           | 0:off, 1 - 127              | 00
+4      | 1    | 00     - 7F     | Pan                       | 0:random, 1 - 127           | 40
+5      | 1    | 00     - 7F     | Reverb send               | 0 - 127                     | 7F
+6      | 1    | 00     - 7F     | Chorus send               | 0 - 127                     | 7F
+7      | 1    | 00     - 7F     | Variation send            | 0 - 127                     | 7F
+8      | 1    | 00     - 01     | Key assign                | 0:single, 1:multi           | 00
+9      | 1    | 00     - 01     | Receive Note Off          | 0:off, 1:on                 | 01
+10     | 1    | 00     - 01     | Receive Note On           | 0:off, 1:on                 | 01
+11     | 1    | 00     - 7F     | Filter cutoff             | 0 - 127                     | 7F
+12     | 1    | 00     - 7F     | Filter resonance          | 0 - 127                     | 10
+13     | 1    | 00     - 7F     | Amp EG attack rate        | 0 - 127                     | 7F
+14     | 1    | 00     - 7F     | Amp EG decay 1 rate       | 0 - 127                     | 40
+15     | 1    | 00     - 7F     | Amp EG decay 2 rate       | 0 - 127                     | 20
+16     | 1    | 00     - 7F     | EQ bass gain [?]          | -12 - +12 [dB]              | 40
+17     | 1    | 00     - 7F     | EQ treble gain [?]        | -12 - +12 [dB]              | 40
+18     | 1    | 04     - 28     | EQ bass frequency         | 32 - 2.0k [Hz]              | 0C
+19     | 1    | 1C     - 3A     | EQ treble frequency       | 500 - 16.0k [Hz]            | 36
+20     | 1    | 00     - 5E     | ?                         | ?                           | 40
+21     | 1    | 40     - 48     | ?                         | ?                           | 40
+22     | 1    | 40     - 4F     | ?                         | ?                           | 40
+23     | 1    | 00     - 7F     | Pitch coarse              | 0 - 127 [note]              | 40
--------+------+-----------------+---------------------------+-----------------------------+---------
+24     | 2    | 0000   - FFFF   | SFX voice#                | 0 - 93, 65535:drum          | FFFF
+26     | 1    | 00     - 7F     | Playback rate             | -64 - +63 [semitone]        | 00
+27     | 1    | 80     - 7F     | Pitch fine                | -128 - +127 [cent]          | 00
+28     | 1    | 80     - 7F     | Pitch coarse              | -128 - +127 [semitone]      | 00
+29     | 1    | 80     - 7F     | ?                         | ?                           | 00
+30     | 1    | Bit 6  - 7      | ?                         | ?                           | 00
        |      | Bit 0  - 5      | Loop fraction             | 0 - 63                      |
+31     | 3    | 000000 - FFFFFF | Attack length             | 0 - 16777215 [samples]      | 000000
+34     | 1    | Bit 7           | Reverse playback          | 0:normal, 1:reverse         | 00
        |      | Bit 0  - 6      | ?                         | ?                           |
+35     | 3    | 000000 - FFFFFF | Loop length               | 0 - 16777215 [samples]      | 000000
+38     | 1    | Bit 6  - 7      | Sample format             | 0:16, 1:12, 2:8-bit, 3:DPCM | 00
        |      | Bit 3  - 5      | DPCM scale                | 2^0 - 2^7                   |
        |      | Bit 1  - 2      | DPCM offset               | 0:-7, 1:-6, 2:-4, 3:0       |
        |      | Bit 0           | Loop point address        | Bit 24 (24 MB wave ROM)     |
+39     | 3    | 000000 - FFFFFF | Loop point address        | Bit 0 - 23                  | 000000

*/
void print_drum_voices(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Drum Voices (+%d)\n", ofs);

	static const char* const hdr  = "                                                                  Voice                                                                  |                                          Sample                                          \n"
	                                "Offset  | ?  | PF  | Lvl | Alt | Pan | Rev | Cho | Var | K | 0 | 1 | Cut | Q   | A   | D1  | D2  | LG  | HG  | LF | HF  | ?        | PC  | SFX#  | Rat | PF   | PC   | ?     | Fr | Attack   | R | ?  | Loop     | F | D8 | Address ";
	static const char* const line = "--------+----+-----+-----+-----+-----+-----+-----+-----+---+---+---+-----+-----+-----+-----+-----+-----+-----+----+-----+----------+-----+-------+-----+------+------+-------+----+----------+---+----+----------+---+----+---------";

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

		// Voice
		printf("%02X | %-+3d | ", ptr[0], ptr[1] - 64);
		printf("%-3d | %-3d | %-3d | ", ptr[2], ptr[3], ptr[4]);
		printf("%-3d | %-3d | %-3d | ", ptr[5], ptr[6], ptr[7]);
		printf("%d | %d | %d | ", ptr[8], ptr[9], ptr[10]);
		printf("%-3d | %-3d | ", ptr[11], ptr[12]);
		printf("%-3d | %-3d | %-3d | ", ptr[13], ptr[14], ptr[15]);
		printf("%-3d | %-3d | %-2d | %-3d | ", ptr[16], ptr[17], ptr[18], ptr[19]);

		for (int j = 20; j < 23; ++j) printf("%02X ", ptr[j]);
		printf("| ");

		printf("%-3d | ", ptr[23]);

		// Sample
		const int sfx_no = (ptr[24] << 8) | ptr[25];
		printf("%-5d | ", sfx_no);

		printf("%-3d | ", ptr[26]);
		printf("%-+4d | %-+4d | ", (signed char)ptr[27], (signed char)ptr[28]);
		printf("%02X %02X | ", ptr[29], ptr[30] & 0xC0);

		const int frac = ptr[30] & 0x3F;
		const int attack = (ptr[31] << 16) | (ptr[32] << 8) | ptr[33];
		const int reverse = ptr[34] >> 7;
		const int loop = (ptr[35] << 16) | (ptr[36] << 8) | ptr[37];
		const int format = ptr[38] >> 6, dpcm = (ptr[38] >> 1) & 0x1F;
		const int addr = (((ptr[38] & 0x01) << 24) | (ptr[39] << 16) | (ptr[40] << 8) | ptr[41]) << 2;

		// const double frac_loop = (double)((loop << 6) - (loop ? frac : 0)) * 0.015625;

		printf("%-2d | %-8d | ", frac, attack);
		printf("%d | %02X | %-8d | ", reverse, ptr[34] & 0x7F, loop);
		printf("%d | %02X | %07X \n", format, dpcm, addr);

		ptr += 42;
		ofs += 42;
	}
}

void print_sfx_voices(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("SFX Voices (+%d)\n\n", ofs);

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	printf("SFX#  "); for (int i = 0; i < 10; ++i) printf("| %-7d ", i);
	printf("\n------"); for (int i = 0; i < 10; ++i) printf("+---------");

	for (int i = 0; i < num; ++i)
	{
		if (!(i % 10)) printf("\n%-5d ", i);

		const int normal_voice_ofs = ((ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3]) << 1;
		printf("| %-+7d ", normal_voice_ofs);

		ptr += 4;
	}

	putchar('\n');
}

void print_bank_lists(const WDL_HeapBuf* const firmware, const int ofs, const int num, const char* const title = NULL)
{
	if (title) printf("%s ", title);
	printf("Bank List");
	if (num != 1) putchar('s');
	printf(" (+%d)\n", ofs);

	static const char* const name[] =
	{
		"SFX",
		"MU Basic",
		"MU100 Native",
		"Model Exclusive",
		"TG300B",
		"GM2",
		"GM1"
	};

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;
	const int c = title ? title[0] : 0;

	for (int i = 0, j = c == 'M' ? 0 : c == 'T' ? 4 : c == 'G' ? 5 : 8; i < num; ++i)
	{
		putchar('\n');

		if (num > 1)
		{
			printf("Bank List %d", i);
			if (j < sizeof(name) / sizeof(name[0])) printf(" (%s)", name[j++]);
			puts("\n");
		}

		printf("Bank# "); for (int k = 0; k < 10; ++k) printf("| %-3d ", k);
		printf("\n------"); for (int k = 0; k < 10; ++k) printf("+-----");

		for (int k = 0; k < 128; ++k)
		{
			if (!(k % 10)) printf("\n%-5d ", k);

			const int prog_bank_no = *ptr++;
			printf("| %-3d ", prog_bank_no);
		}

		putchar('\n');
	}
}

void print_program_banks(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("Program Banks (+%d)\n", ofs);

	static const char* const name[] =
	{
		"MU Basic",
		"Key Scale Panning",
		"Stereo",
		"Single",
		"Slow",
		"Fast Decay",
		"Double Attack",
		"Bright 1",
		"Bright 2",
		"Dark 1",
		"Dark 2",
		"Resonant",
		"LFO - Cutoff Freq",
		"Vel - Cutoff Freq",
		"Attack",
		"Release",
		"Sweep",
		"Resonant Sweep",
		"Muted",
		"Complex - FEG",
		"Detune 1",
		"Detune 2",
		"Detune 3",
		"Octave 1",
		"Octave 2",
		"5th 1",
		"5th 2",
		"Bend",
		"Tutti 1",
		"Tutti 2",
		"Tutti 3",
		"Velocity Switch",
		"Velocity Crossfade",
		"Detune 4",
		"Tutti 4",
		"Tutti 5",
		"Tutti 6",
		"Other Waves 1",
		"Other Waves 2",
		"Other Waves 3",
		"Other Waves 4",
		"Other Waves 5",
		"Other Waves 6",
		"Other Waves 7",
		"Other Waves 8",
		"Other Waves 9",
		"Other Waves 10",
		"Other Waves 11",
		"Other Waves 12",
		"Other Waves 13",
		"Other Waves 14",
		"Other Waves 15",
		"Other Waves 16",
		"Other Waves 17",
		"Other Waves 18",
		"Other Waves 19",
		"Other Waves 20",
		"Other Waves 21",
		"Other Waves 22",
		"Other Waves 23",
		"Other Waves 24",
		"Other Waves 25",
		"Other Waves 26",
		"Other Waves 27",
		"Other Waves 28",
		"Other Waves 29",
		"Other Waves 30",
		"Other Waves 31",
		"Other Instrument 1",
		"Other Instrument 2",
		"Other Instrument 3",
		"Other Instrument 4",
		"Other Instrument 5",
		"Other Instrument 6",
		"Capital Voices MU100 Native",
		"Capital Voices MU Basic",
		"Silence",
		"Timbre",
		"Timbre, Poly",
		"Timbre, Looped",
		"Timbre, Looped, Poly",
		"Phrase, Looped",
		"Phrase, Looped, Poly",
		"SFX, Timbre",
		"SFX, Timbre, Poly",
		"SFX, Phrase",
		"SFX, Phrase, Poly",
		"Rhythm, Timbre",
		"Rhythm, Timbre, Poly",
		"Rhythm, Phrase, Poly",
		"SFX",
		"MU100 Native",
		"Key Scale Panning",
		"Stereo",
		"Single",
		"Slow",
		"Fast Decay",
		"Double Attack",
		"Bright 1",
		"Bright 2",
		"Dark 1",
		"Dark 2",
		"Resonant",
		"LFO - Cutoff Freq",
		"Vel - Cutoff Freq",
		"Attack",
		"Release",
		"Sweep",
		"Resonant Sweep",
		"Muted",
		"Complex - FEG",
		"Detune 1",
		"Detune 2",
		"Detune 3",
		"Octave 1",
		"Octave 2",
		"5th 1",
		"5th 2",
		"Bend",
		"Tutti 1",
		"Tutti 2",
		"Tutti 3",
		"Velocity Switch",
		"Velocity Crossfade",
		"Detune 4",
		"Tutti 4",
		"Tutti 5",
		"Tutti 6",
		"Other Waves 1",
		"Other Waves 2",
		"Other Waves 3",
		"Other Waves 4",
		"Other Waves 5",
		"Other Waves 6",
		"Other Waves 7",
		"Other Waves 8",
		"Other Waves 9",
		"Other Waves 10",
		"Other Waves 11",
		"Other Waves 12",
		"Other Waves 13",
		"Other Waves 14",
		"Other Waves 15",
		"Other Waves 16",
		"Other Waves 17",
		"Other Waves 18",
		"Other Waves 19",
		"Other Waves 20",
		"Other Waves 21",
		"Other Waves 22",
		"Other Waves 23",
		"Other Waves 24",
		"Other Waves 25",
		"Other Waves 26",
		"Other Waves 27",
		"Other Waves 28",
		"Other Waves 29",
		"Other Waves 30",
		"Other Waves 31",
		"Other Instrument 1",
		"Other Instrument 2",
		"Other Instrument 3",
		"Other Instrument 4",
		"Other Instrument 5",
		"Other Instrument 6",
		"Capital Voices MU100 Native",
		"Capital Voices MU Basic",
		"TG300B Bank 0",
		"TG300B Bank 1",
		"TG300B Bank 2",
		"TG300B Bank 3",
		"TG300B Bank 4",
		"TG300B Bank 5",
		"TG300B Bank 6",
		"TG300B Bank 7",
		"TG300B Bank 8",
		"TG300B Bank 9",
		"TG300B Bank 10",
		"TG300B Bank 11",
		"TG300B Bank 12",
		"TG300B Bank 14",
		"TG300B Bank 15",
		"TG300B Bank 16",
		"TG300B Bank 17",
		"TG300B Bank 18",
		"TG300B Bank 19",
		"TG300B Bank 24",
		"TG300B Bank 25",
		"TG300B Bank 26",
		"TG300B Bank 27",
		"TG300B Bank 29",
		"TG300B Bank 30",
		"TG300B Bank 31",
		"TG300B Bank 32",
		"TG300B Bank 33",
		"TG300B Bank 34",
		"TG300B Bank 35",
		"TG300B Bank 40",
		"TG300B Bank 41",
		"TG300B Bank 126",
		"TG300B Bank 127",
		"GM2 Bank 0",
		"GM2 Bank 1",
		"GM2 Bank 2",
		"GM2 Bank 3",
		"GM2 Bank 4",
		"GM2 Bank 5",
		"GM2 Bank 6",
		"GM2 Bank 7",
		"GM2 Bank 8",
		"GM2 Bank 9",
		"GM1 Bank 0",
		"GM1 Bank 1",
		"GM1 Bank 2",
		"GM1 Bank 3",
		"GM1 Bank 4",
		"GM1 Bank 5",
		"GM1 Bank 6",
		"GM1 Bank 7",
		"GM1 Bank 8",
		"GM1 Bank 9"
	};

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		printf("\nBank %d", i);
		if (i < sizeof(name) / sizeof(name[0])) printf(" (%s)", name[i]);
		puts("\n");

		printf("Prog# "); for (int j = 1; j <= 10; ++j) printf("| %-7d ", j);
		printf("\n------"); for (int j = 1; j <= 10; ++j) printf("+---------");

		for (int j = 0; j < 128; ++j)
		{
			if (!(j % 10)) printf("\n%-5d ", j);

			const int normal_voice_ofs = ((ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3]) << 1;
			printf("| %-+7d ", normal_voice_ofs);

			ptr += 4;
		}

		putchar('\n');
	}
}

int ilog2(unsigned int n)
{
	// Source: https://stackoverflow.com/a/31718095

	static const char tbl[32] =
	{
		0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
		8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
	};

	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;

	return tbl[(n * 0x07C4ACDD) >> 27];
}

/* Normal Voices

Offset  | Size | Data            | Parameter                 | Description                 | Default
--------+------+-----------------+---------------------------+-----------------------------+---------
+0      | 1    | 00     - 01     | Element switch            | 1:1, 3:2, 7:3, 15:4         | 01
+1      | 1    | 00     - 7F     | Voice level               | y =~ (x/127)^2              |
+2      | 10   | 20     - 7F     | Voice name                | ASCII, space padded         |
+12     | 1    | 00              | ?                         | ?                           | 00
+13     | 1    | 7F              | ?                         | ?                           | 7F
+14     | 84   | See below       | Element 1                 | See below                   |
+98     | 84   | See below       | Element 2 (optional)      | See below                   |
+182    | 84   | See below       | Element 3 (optional)      | See below                   |
+266    | 84   | See below       | Element 4 (optional)      | See below                   |

Element 1/2/3/4

Offset  | Size | Data            | Parameter                 | Description                 | Default
--------+------+-----------------+---------------------------+-----------------------------+---------
+0      | 1    | 00     - 7F     | Wave# MSB                 | 0 - 460                     |
+1      | 1    | 00     - 7F     | Wave# LSB                 |                             |
+2      | 1    | 00     - 7F     | Note limit low            | 0 - 127 [note]              | 00
+3      | 1    | 00     - 7F     | Note limit high           | 0 - 127 [note]              | 7F
+4      | 1    | 01     - 7F     | Velocity limit low        | 1 - 127                     | 01
+5      | 1    | 01     - 7F     | Velocity limit high       | 1 - 127                     | 7F
--------+------+-----------------+---------------------------+-----------------------------+---------
+6      | 1    | 00     - 01     | LFO phase init            | 0:off, 1:on                 | 00
+7      | 1    | 00     - 02     | LFO wave                  | 0:saw, 1:tri, 2:s&h         | 01
+8      | 1    | 00     - 01     | Filter EG velocity curve  | 0:linear, 1:exponential     | 01
+9      | 1    | 00     - 3F     | LFO speed                 | 0 - 63                      | 1F
+10     | 1    | 00     - 7F     | Vibrato delay time        | 0 - 127                     | 00
+11     | 1    | 00     - 7F     | Vibrato fade time         | 0 - 127                     | 00
+12     | 1    | 00     - 3F     | LFO pitch mod depth       | 0 - 63                      | 00
+13     | 1    | 00     - 0F     | LFO filter mod depth      | 0 - 15                      | 00
+14     | 1    | 00     - 1F     | LFO amp mod depth         | 0 - 31                      | 00
--------+------+-----------------+---------------------------+-----------------------------+---------
+15     | 1    | 20     - 60     | Note shift                | -32 - +32 [semitone]        | 40
+16     | 1    | 0E     - 72     | Detune                    | -50 - +50 [cent]            | 40
+17     | 1    | 00     - 05     | Pitch scaling depth       | 0 - 5: 0/5/10/20/50/100%    | 00
+18     | 1    | 00     - 7F     | Pitch scaling center note | 0 - 127 [note]              | 3C
+19     | 1    | 00     - 03     | Pitch EG depth            | 0:0.5, 1:1, 2:2, 3:4 [oct]  | 01
+20     | 1    | 39     - 47     | PEG velocity level sens.  | -7 - +7                     | 40
+21     | 1    | 39     - 47     | PEG velocity rate sens.   | -7 - +7                     | 40
+22     | 1    | 39     - 47     | Pitch EG rate scaling     | -7 - +7                     | 40
+23     | 1    | 00     - 7F     | Pitch EG RS center note   | 0 - 127 [note]              | 3C
+24     | 1    | 00     - 3F     | Pitch EG attack rate      | 0 - 63                      | 3F
+25     | 1    | 00     - 3F     | Pitch EG decay 1 rate     | 0 - 63                      | 3F
+26     | 1    | 00     - 3F     | Pitch EG decay 2 rate     | 0 - 63                      | 3F
+27     | 1    | 00     - 3F     | Pitch EG release rate     | 0 - 63                      | 3F
+28     | 1    | 00     - 7F     | Pitch EG initial level    | -64 - +63                   | 40
+29     | 1    | 00     - 7F     | Pitch EG attack level     | -64 - +63                   | 40
+30     | 1    | 00     - 7F     | Pitch EG decay 1 level    | -64 - +63                   | 40
+31     | 1    | 00     - 7F     | Pitch EG decay 2 level    | -64 - +63                   | 40
+32     | 1    | 00     - 7F     | Pitch EG release level    | -64 - +63                   | 40
--------+------+-----------------+---------------------------+-----------------------------+---------
+33     | 1    | 00     - 3F     | Filter resonance          | 0 - 63                      | 00
+34     | 1    | 00     - 07     | Velocity sensitivity      | 0 - 7                       | 00
+35     | 1    | 00     - 7F     | Filter cutoff             | 0 - 127                     | 7F
+36     | 6    | 00     - FF     | [Cutoff scaling]          | FF 00 00 00 00 00           |
+42     | 1    | 00     - 05     | Cutoff scaling# MSB       | 0 - 1376                    |
+43     | 1    | 00     - FF     | Cutoff scaling# LSB       |                             |
+44     | 1    | 39     - 47     | FEG velocity level sens.  | -7 - +7                     | 40
+45     | 1    | 39     - 47     | FEG velocity rate sens.   | -7 - +7                     | 40
+46     | 1    | 39     - 47     | Filter EG rate scaling    | -7 - +7                     | 40
+47     | 1    | 00     - 7F     | Filter EG RS center note  | 0 - 127 [note]              | 3C
+48     | 1    | 00     - 3F     | Filter EG attack rate     | 0 - 63                      | 3F
+49     | 1    | 00     - 3F     | Filter EG decay 1 rate    | 0 - 63                      | 3F
+50     | 1    | 00     - 3F     | Filter EG decay 2 rate    | 0 - 63                      | 3F
+51     | 1    | 00     - 3F     | Filter EG release rate    | 0 - 63                      | 3F
+52     | 1    | 00     - 7F     | Filter EG initial level   | -64 - +63                   | 40
+53     | 1    | 00     - 7F     | Filter EG attack level    | -64 - +63                   | 40
+54     | 1    | 00     - 7F     | Filter EG decay 1 level   | -64 - +63                   | 40
+55     | 1    | 00     - 7F     | Filter EG decay 2 level   | -64 - +63                   | 40
+56     | 1    | 00     - 7F     | Filter EG release level   | -64 - +63                   | 40
--------+------+-----------------+---------------------------+-----------------------------+---------
+57     | 1    | 00     - 7F     | Element level             | 0 - 127, y =~ (x/127)^2     |
+58     | 6    | 00     - FF     | [Level scaling]           | FF 00 00 00 00 00           |
+64     | 1    | 00     - 05     | Level scaling# MSB        | 0 - 1376                    |
+65     | 1    | 00     - FF     | Level scaling# LSB        |                             |
+66     | 1    | 00     - 06     | Velocity curve            | 0 - 6                       | 00
+67     | 1    | 00     - 0F     | Pan                       | 0 - 14, 15:scaling          | 07
+68     | 1    | 39     - 47     | Amp EG rate scaling       | -7 - +7                     | 40
+69     | 1    | 00     - 7F     | Amp EG RS center note     | 0 - 127 [note]              | 3C
+70     | 1    | 00     - 0F     | Amp EG key on delay       | 0 - 15                      | 00
+71     | 1    | 00     - 3F     | Amp EG attack rate        | 0 - 63                      | 3F
+72     | 1    | 00     - 3F     | Amp EG decay 1 rate       | 0 - 63                      | 3F
+73     | 1    | 00     - 3F     | Amp EG decay 2 rate       | 0 - 63                      | 3F
+74     | 1    | 00     - 3F     | Amp EG release rate       | 0 - 63                      | 3F
+75     | 1    | 00     - 7F     | Amp EG decay 1 level      | -64 - +63                   | 40
+76     | 1    | 00     - 7F     | Amp EG decay 2 level      | -64 - +63                   | 40
+77     | 1    | 00     - 7F     | Address offset MSB        | Attack offset [samples]     | 00
+78     | 1    | 00     - 7F     | Address offset LSB        |                             | 00
+79     | 1    | 39     - 47     | Resonance sensitivity     | -7 - +7                     | 40
+80     | 1    | 00     - 7F     | ?                         | ?                           | 00
+81     | 1    | 40              | ?                         | ?                           | 40
+82     | 1    | 7F              | ?                         | ?                           | 7F
+83     | 1    | 40              | ?                         | ?                           | 40

*/
void print_normal_voices(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Normal Voices (+%d)\n", ofs);

	static const char* const hdr  = "                 Voice                 |            Element            |                    LFO                    |                                               Pitch                                               |                                             Filter                                             |                                               Amplitude                                                \n"
	                                "Offset  | E | Lvl | Name       | ?     | Wave# | N1  | N2  | V1  | V2  | I | W | F | Sp | Del | Fad | PM | FM | AM | NS  | Det | S | Ns  | D | VL  | VR  | RS  | Nrs | R1 | R2 | R3 | R4 | L0  | L1  | L2  | L3  | L4  | Q  | VS | Cut | LS#  | VL  | VR  | RS  | Nrs | R1 | R2 | R3 | R4 | L0  | L1  | L2  | L3  | L4  | Lvl | LS#  | VC | P  | RS  | Nrs | R0 | R1  | R2  | R3  | R4  | L2  | L3  | Addr   | QS  | ?           ";
	static const char* const line = "--------+---+-----+------------+-------+-------+-----+-----+-----+-----+---+---+---+----+-----+-----+----+----+----+-----+-----+---+-----+---+-----+-----+-----+-----+----+----+----+----+-----+-----+-----+-----+-----+----+----+-----+------+-----+-----+-----+-----+----+----+----+----+-----+-----+-----+-----+-----+-----+------+----+----+-----+-----+----+-----+-----+-----+-----+-----+-----+--------+-----+-------------";
	static const char* const skip = "        |   |     |            |       | ";

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;
	ofs = 0;

	for (int i = 0, j = 0; i < num; ++i)
	{
		if (!j)
		{
			if (!i) putchar('\n'); else puts(line);
			puts(hdr);
			puts(line);
		}

		printf("%-+7d | ", ofs);

		// Voice
		const int el = ilog2(ptr[0]) + 1;
		printf("%d | %-3d | ", el, ptr[1]);

		for (int k = 2; k < 12; ++k) putchar(ptr[k]);
		printf(" | ");

		for (int k = 12; k < 14; ++k) printf("%02X ", ptr[k]);
		printf("| ");
		ptr += 14;

		for (int k = 0; k < el; ++k)
		{
			if (k) printf(skip);

			// Element
			printf("%-5d | ", (ptr[0] << 7) | ptr[1]);
			printf("%-3d | %-3d | ", ptr[2], ptr[3]);
			printf("%-3d | %-3d | ", ptr[4], ptr[5]);

			// LFO
			printf("%d | %d | %d | ", ptr[6], ptr[7], ptr[8]);
			printf("%-2d | %-3d | %-3d | ", ptr[9], ptr[10], ptr[11]);
			printf("%-2d | %-2d | %-2d | ", ptr[12], ptr[13], ptr[14]);

			// Pitch
			printf("%-+3d | %-+3d | ", ptr[15] - 64, ptr[16] - 64);
			printf("%d | %-3d | ", ptr[17], ptr[18]);
			printf("%d | %-+3d | %-+3d | ", ptr[19], ptr[20] - 64, ptr[21] - 64);
			printf("%-+3d | %-3d | ", ptr[22] - 64, ptr[23]);
			for (int l = 0; l < 4; ++l) printf("%-2d | ", ptr[24 + l]);
			for (int l = 0; l < 5; ++l) printf("%-+3d | ", ptr[28 + l] - 64);

			// Filter
			printf("%-2d | %-2d | %-3d | ", ptr[33], ptr[34], ptr[35]);
			printf("%-4d | ", (ptr[42] << 8) | ptr[43]);
			printf("%-+3d | %-+3d | ", ptr[44] - 64, ptr[45] - 64);
			printf("%-+3d | %-3d | ", ptr[46] - 64, ptr[47]);
			for (int l = 0; l < 4; ++l) printf("%-2d | ", ptr[48 + l]);
			for (int l = 0; l < 5; ++l) printf("%-+3d | ", ptr[52 + l] - 64);

			// Amplitude
			printf("%-3d | ", ptr[57]);
			printf("%-4d | ", (ptr[64] << 8) | ptr[65]);
			printf("%-2d | ", ptr[66]);
			printf("%-2d | %-+3d | %-3d | ", ptr[67], ptr[68] - 64, ptr[69]);
			for (int l = 0; l < 5; ++l) printf("%-*d | ", !!l + 2, ptr[70 + l]);
			for (int l = 0; l < 2; ++l) printf("%-+3d | ", ptr[75 + l] - 64);
			printf("%-+6d | %-+3d | ", (ptr[77] << 7) | ptr[78], ptr[79] - 64);

			for (int l = 80; l < 84; ++l) printf("%02X ", ptr[l]);
			putchar('\n');

			ptr += 84;
		}

		ofs += 14 + el * 84;
		if ((j += el) >= 25) j = 0;
	}
}

void print_level_scales(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Cutoff/Level Scaling (+%d)\n", ofs);

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		printf("\nLevel Scale %d\n\n", i);

		printf("Note# "); for (int j = 1; j <= 10; ++j) printf("| %-3d ", j);
		printf("\n------"); for (int j = 1; j <= 10; ++j) printf("+-----");

		for (int j = 0; j < 128; ++j)
		{
			if (!(j % 10)) printf("\n%-5d ", j);

			const int level_scale_ofs = (signed char)*ptr++;
			printf("| %-+3d ", level_scale_ofs);
		}

		putchar('\n');
	}
}

void print_sample_sets(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("Sample Sets (+%d)\n\n", ofs);

	printf("Wave# "); for (int i = 0; i < 10; ++i) printf("| %-6d ", i);
	printf("\n------"); for (int i = 0; i < 10; ++i) printf("+--------");

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		if (!(i % 10)) printf("\n%-5d ", i);

		const int sample_ofs = (ptr[0] << 8) | ptr[1];
		printf("| %-+6d ", sample_ofs);

		ptr += 2;
	}

	putchar('\n');
}

/* Samples

Offset  | Size | Data            | Parameter                 | Description                 | Default
--------+------+-----------------+---------------------------+-----------------------------+---------
+0      | 1    | 00     - 7F     | Attenuation               | y =~ 2^(-x/8)               | 00
+1      | 1    | 80     - 7F     | Pitch coarse              | -128 - 127 [note]           | 3C
+2      | 1    | 80     - 7F     | Pitch fine                | -128 - +127 [cent]          | 40
+3      | 1    | 00     - 7F     | Note limit high           | 0 - 127 [note]              | 7F
+4      | 1    | Bit 6  - 7      | ?                         | ?                           | 00
        |      | Bit 0  - 5      | Loop fraction             | 0 - 63                      |
+5      | 3    | 000000 - FFFFFF | Attack length             | 0 - 16777215 [samples]      | 000000
+8      | 1    | Bit 7           | Reverse playback          | 0:normal, 1:reverse         | 00
        |      | Bit 0  - 6      | ?                         | ?                           |
+9      | 3    | 000000 - FFFFFF | Loop length               | 0 - 16777215 [samples]      | 000000
+12     | 1    | Bit 6  - 7      | Sample format             | 0:16, 1:12, 2:8-bit, 3:DPCM | 00
        |      | Bit 3  - 5      | DPCM scale                | 2^0 - 2^7                   |
        |      | Bit 1  - 2      | DPCM offset               | 0:-7, 1:-6, 2:-4, 3:0       |
        |      | Bit 0           | Loop point address        | Bit 24 (24 MB wave ROM)     |
+13     | 3    | 000000 - FFFFFF | Loop point address        | Bit 0 - 23                  | 000000

*/
void print_samples(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Samples (+%d)\n", ofs);

	static const char* const hdr  = "Offset  | Atn | PC   | PF   | N   | ?  | Fr | Attack   | R | ?  | Loop     | F | D8 | Address ";
	static const char* const line = "--------+-----+------+------+-----+----+----+----------+---+----+----------+---+----+---------";

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

		printf("%-3d | ", ptr[0]);
		printf("%-4d | %-+4d | ", (signed char)ptr[1], (signed char)ptr[2]);
		printf("%-3d | %02X | ", ptr[3], ptr[4] & 0xC0);

		const int frac = ptr[4] & 0x3F;
		const int attack = (ptr[5] << 16) | (ptr[6] << 8) | ptr[7];
		const int reverse = ptr[8] >> 7;
		const int loop = (ptr[9] << 16) | (ptr[10] << 8) | ptr[11];
		const int format = ptr[12] >> 6, dpcm = (ptr[12] >> 1) & 0x1F;
		const int addr = (((ptr[12] & 0x01) << 24) | (ptr[13] << 16) | (ptr[14] << 8) | ptr[15]) << 2;

		// const double frac_loop = (double)((loop << 6) - (loop ? frac : 0)) * 0.015625;

		printf("%-2d | %-8d | ", frac, attack);
		printf("%d | %02X | %-8d | ", reverse, ptr[8] & 0x7F, loop);
		printf("%d | %02X | %07X \n", format, dpcm, addr);

		ptr += 16;
		ofs += 16;
	}
}

void write_utf16_str(const char* const str, FILE* const fp)
{
	for (int i = 0;; ++i)
	{
		const unsigned short c = str[i];
		if (!c) break;

		#ifdef _WIN32
		if (c == '\n')
		{
			static const unsigned short cr = '\r';
			fwrite(&cr, 2, 1, fp);
		}
		#endif

		fwrite(&c, 2, 1, fp);
	}
}

int print_bitmaps(FILE* const fp, const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	static const unsigned short block[4][2] =
	{
		{ ' ',    ' '    }, // Space
		{ 0x2580, 0x2580 }, // Upper half block
		{ 0x2584, 0x2584 }, // Lower half block
		{ 0x2588, 0x2588 }  // Full block
	};

	#ifdef _WIN32
	static const size_t n = 2;
	const unsigned short eol[n] = { '\r', '\n' };
	#else
	static const size_t n = 1;
	const unsigned short eol[n] = { '\n' };
	#endif

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		char str[16];
		sprintf(str, "+%d\n\n", ofs);

		write_utf16_str(str, fp);

		for (int j = 0; j < 8; ++j)
		{
			const int line1 = (ptr[0] << 8) | ptr[1];
			const int line2 = (ptr[2] << 8) | ptr[3];

			static const unsigned short space = ' ';
			fwrite(&space, 2, 1, fp);

			for (int k = 0x8000; k; k >>= 1)
			{
				const int bit1 = !!(line1 & k);
				const int bit2 = !!(line2 & k);

				fwrite(block[(bit2 << 1) | bit1], 2, 2, fp);
			}

			fwrite(eol, 2, n, fp);
			ptr += 4;
		}

		fwrite(eol, 2, n, fp);
		ofs += 32;
	}

	return num;
}

int read_wavetbl(const char* const filenames[4], WDL_HeapBuf* const buf)
{
	unsigned char* ptr = (unsigned char*)buf->ResizeOK((32 + 8)*1024*1024);
	if (!ptr) return 0;

	unsigned char* const tmp = ptr + 32*1024*1024;
	int total = 0;

	for (int i = 0; i < 4; ++i)
	{
		FILE* const fp = fopen(filenames[i], "rb");
		if (!fp) return 0;

		const int n = (int)fread(tmp, 1, 8*1024*1024, fp) & ~1;
		fclose(fp);

		for (int j = 0; j < n; j += 2)
		{
			memcpy(ptr, &tmp[j], 2);
			ptr += 4;
		}

		total += n;
		ptr -= !(i & 1) ? (n - 1) * 2 : 2;
	}

	buf->Resize(32*1024*1024);

	return total == buf->GetSize() ? total : 0;
}

// https://bleepsandpops.com/post/37792760450/adding-cue-points-to-wav-files-in-c

void write_cue_points(WaveWriter* const wav, const int num, const int loop, const int end)
{
	assert(num == 1 || num == 2);

	static const int max_num = 2;
	int cue[3 + max_num * 6];

	cue[0] = WDL_bswap32_if_le('cue ');
	cue[1] = WDL_bswap32_if_be((1 + num * 6) * sizeof(int));
	cue[2] = WDL_bswap32_if_be(num);

	int* ptr = &cue[3];
	int ofs = num == 2 ? loop : end;

	for (int i = 0; i < num; ++i)
	{
		ptr[0] = WDL_bswap32_if_be(i);
		ptr[1] = 0;
		ptr[2] = WDL_bswap32_if_le('data');
		ptr[3] = 0;
		ptr[4] = 0;
		ptr[5] = WDL_bswap32_if_be(ofs);

		ofs = end;
		ptr += 6;
	}

	wav->WriteChunk(cue, (3 + num * 6) * sizeof(int));
}

int write_sample(const char* const filename, const WDL_HeapBuf* const wavetbl, const int format, const int addr, const int attack, const int loop, const bool reverse = false, const int dpcm = 0)
{
	const unsigned char* const buf = (const unsigned char*)wavetbl->Get() + addr;

	static const int extra = 3;
	const int len = attack + loop + extra, n = len - 1;

	WaveWriter wav;
	if (!wav.Open(filename, format == 2 ? 8 : 16, 1, 44100, 0)) return 0;

	switch (format)
	{
		// 16-bit signed linear PCM
		case 0:
		{
			const unsigned char* const base = buf - (attack << 1);

			for (int i = 0; i <= n; ++i)
			{
				int j = n - i;
				j = !reverse ? i : j;

				const unsigned char* const ptr = &base[j << 1];
				short sample = ptr[0] | (ptr[1] << 8);

				sample = WDL_bswap16_if_be(sample);
				wav.WriteRaw(&sample, 2);
			}
			break;
		}

		// 12-bit signed linear PCM
		case 1:
		{
			const unsigned char* const base = buf - (attack >> 1) * 3;

			for (int i = 0; i <= n; ++i)
			{
				int j = n - i;
				j = !reverse ? i : j;

				const unsigned char* const ptr = &base[(j >> 1) * 3];
				short sample;

				if (!(i & 1))
					sample = (ptr[0] << 4) | (ptr[1] << 12);
				else
					sample = (ptr[1] & 0xF0) | (ptr[2] << 8);

				sample = WDL_bswap16_if_be(sample);
				wav.WriteRaw(&sample, 2);
			}
			break;
		}

		// 8-bit signed linear PCM
		case 2:
		{
			const unsigned char* const base = buf - attack;

			for (int i = 0; i <= n; ++i)
			{
				int j = n - i;
				j = !reverse ? i : j;

				unsigned char sample = base[j] ^ 0x80;
				wav.WriteRaw(&sample, 1);
			}
			break;
		}

		// 8-bit signed log DPCM
		case 3:
		{
			assert(reverse == false);

			static const unsigned short log_tbl[128] =
			{
				0, 8, 16, 24, 32, 40, 48, 56, 64, 72,
				80, 88, 96, 104, 112, 120, 128, 136, 144, 152,
				160, 168, 176, 184, 192, 200, 208, 216, 224, 232,
				240, 248, 256, 272, 288, 304, 320, 336, 352, 368,
				384, 400, 416, 432, 448, 464, 480, 496, 512, 528,
				544, 560, 576, 592, 608, 624, 640, 656, 672, 688,
				704, 720, 736, 752, 768, 800, 832, 864, 896, 928,
				960, 992, 1024, 1056, 1088, 1120, 1152, 1184, 1216, 1248,
				1280, 1312, 1344, 1376, 1408, 1440, 1472, 1504, 1536, 1568,
				1600, 1632, 1664, 1696, 1728, 1760, 1792, 1856, 1920, 1984,
				2048, 2112, 2176, 2240, 2304, 2368, 2432, 2496, 2560, 2624,
				2688, 2752, 2816, 2880, 2944, 3008, 3072, 3136, 3200, 3264,
				3328, 3392, 3456, 3520, 3584, 3648, 3712, 3776
			};

			static const unsigned char ofs_tbl[4] = { 7, 6, 4, 0 };
			const int ofs = ofs_tbl[dpcm & 3], scale = (dpcm >> 2) & 7;

			const unsigned char* const base = buf - attack;
			int sum = 0;

			for (int i = 0; i <= n; ++i)
			{
				const int step = base[i];

				const int delta = log_tbl[step & 0x7F];
				const int min_delta = -delta;

				sum += step & 0x80 ? min_delta : delta;
				sum -= ofs;

				int y = (sum << scale) / 8;

				y = wdl_max(y, -32768);
				y = wdl_min(y, +32767);

				short sample = WDL_bswap16_if_be((short)y);
				wav.WriteRaw(&sample, 2);
			}
			break;
		}

		default: assert(false);
	}

	wav.EndDataChunk();
	write_cue_points(&wav, loop ? 2 : 1, attack, len - extra);
	wav.Close();

	return len;
}

int extract_sample(const char* const filename, const int ofs, const WDL_HeapBuf* const wavetbl, const unsigned char* const ptr)
{
	const int attack = (ptr[0] << 16) | (ptr[1] << 8) | ptr[2];
	const int reverse = ptr[3] >> 7;
	const int loop = (ptr[4] << 16) | (ptr[5] << 8) | ptr[6];

	if (!(attack || loop)) return 0;

	const int format = ptr[7] >> 6, dpcm = (ptr[7] >> 1) & 0x1F;
	const int addr = (((ptr[7] & 0x01) << 24) | (ptr[8] << 16) | (ptr[9] << 8) | ptr[10]) << 2;

	#ifndef MUTABLE_EXTRACT_DUPLICATES

	static const int max_samples = 2190;
	static unsigned char sample_list[max_samples][11];

	static int num_samples = 0;

	unsigned char hash[11];
	memcpy(hash, ptr, 11);

	hash[3] = reverse << 7;
	hash[7] = (format << 6) | (format == 3 ? dpcm : 0);

	for (int i = 0; i < num_samples; ++i)
	{
		if (!memcmp(sample_list[i], hash, 11)) return 0;
	}

	assert(num_samples < max_samples);
	memcpy(sample_list[num_samples++], hash, 11);

	#endif

	char fn[128];
	sprintf(fn, filename, ofs);

	return write_sample(fn, wavetbl, format, addr, attack, loop, reverse, dpcm);
}

int extract_drum_samples(const char* const filename, const WDL_HeapBuf* const firmware, int ofs, const int num, const WDL_HeapBuf* const wavetbl)
{
	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;
	int n = 0;

	for (int i = 0; i < num; ++i)
	{
		const int sfx_no = (ptr[24] << 8) | ptr[25];

		if (sfx_no == 0xFFFF)
		{
			n += extract_sample(filename, i * 42, wavetbl, &ptr[31]) > 0;
		}

		ptr += 42;
	}

	return n;
}

int extract_samples(const char* const filename, const WDL_HeapBuf* const firmware, const int ofs, const int num, const WDL_HeapBuf* const wavetbl)
{
	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;
	int n = 0;

	for (int i = 0; i < num; ++i)
	{
		n += extract_sample(filename, i * 16, wavetbl, &ptr[5]) > 0;
		ptr += 16;
	}

	return n;
}

int main(const int argc, const char* const* const argv)
{
	int opt = argc == 2 ? argv[1][0] : 0;
	if (opt == '-') opt = argv[1][1] | ('-' << 8);

	static const char* const roms[] =
	{
		#ifdef MU2000_FIRMWARE_V1_01
		// IC25 XW87020 FLASH ROM v1.01
		"mu2000/xw87020.ic25", // SHA1(2213b9c661c6b1a79963321c37aff40be7cc1fff)
		// IC24 XW86920 FLASH ROM v1.01
		"mu2000/xw86920.ic24", // SHA1(594b4c64aecf6a5204b058375e39e44b8fe373be)
		#else // MU2000_FIRMWARE_V2_01
		// IC25 FLASH ROM v2.01
		"mu2000/mu2000-v2.01-h.bin", // SHA1(00d008b6a2536a71681ce2f4fd1a5853406f82f2)
		// IC24 FLASH ROM v2.01
		"mu2000/mu2000-v2.01-l.bin", // SHA1(fd8fe6a5cbba028d847453c004cb2dcf9ba02013)
		#endif

		// IC49 XV364A0 WAVE ROM 1 64M
		"mu2000/xv364a0.ic49", // SHA1(e7098246b33c3cf22ed8cc15ed6383f8a06d17e9)
		// IC50 XV365A0 WAVE ROM 1 64M
		"mu2000/xv365a0.ic50", // SHA1(d45a2e85859e05046f3ede8317a9bb0b88898116)

		// IC53 XW848A0 WAVE ROM 2 64M
		"mu2000/xw848a0.ic53", // SHA1(9e8b55c2cbac3f69cc0b17aeaf02053145bfaeda)
		// IC54 XW849A0 WAVE ROM 2 64M
		"mu2000/xw849a0.ic54"  // SHA1(7670d672e24d6388fa92799175f35869a140c451)
	};

	WDL_HeapBuf firmware;

	if (!read_firmware(&roms[0], &firmware, 2, 2*1024*1024))
	{
		return EXIT_FAILURE;
	}

	if (opt == '-f')
	{
		const int size = write_firmware("build/mu2000_firmware.bin", &firmware);
		return size == firmware.GetSize() ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (opt == '-m')
	{
		static const char* const filename[] =
		{
			"morio_demo",
			"music_factory",
			"invitation"
		};

		static const int ofs[][4] =
		{
			{ +2832656, +2869272, +2883360, +2884648 },
			{ +2884912, +2904888, +2922304, +2940488 },
			{ +2941520, +2966616, +2981216, +2986344 }
		};

		#ifdef MU2000_FIRMWARE_V1_01
		static const int rebase = -281316;
		#else // MU2000_FIRMWARE_V2_01
		static const int rebase = 0;
		#endif

		static const int m = sizeof(ofs) / sizeof(ofs[0]);
		int n = 0;

		for (int i = 0; i < m; ++i)
		{
			for (int j = 0; j < 4; ++j)
			{
				char fn[128];
				sprintf(fn, "midi/mu2000_%s_%c.mid", filename[i], 'a' + j);

				const int size = write_midi(fn, &firmware, ofs[i][j] + rebase);
				n += size > 0;
			}
		}

		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	if (opt == '-t')
	{
		static const int ofs[] =
		{
			#ifdef MU2000_FIRMWARE_V1_01
			+2419984,
			+2404382,
			+2361248,
			+2360608,
			+2360096,
			+2361120,
			+2420752,
			+2246944,
			+1824944,
			+2070688,
			+1823936,
			+1777680, 2891
			#else // MU2000_FIRMWARE_V2_01
			+2696000,
			+2680398,
			+2637264,
			+2636624,
			+2636112,
			+2637136,
			+2696768,
			+2522960,
			+2100960,
			+2346704,
			+2099952,
			+2053536, 2901
			#endif
		};

		const int num = ofs[12];

		printf("MU2000 Data Tables\n\n");

		print_drum_banks(&firmware, ofs[0], 6); puts("\n--\n");
		print_drum_kits(&firmware, ofs[1], 60); puts("\n--\n");
		print_drum_voices(&firmware, ofs[2], 1027); puts("\n--\n");
		print_sfx_voices(&firmware, ofs[3], 97); puts("\n--\n");

		print_bank_lists(&firmware, ofs[4], 4, "MU2000"); puts("\n--\n");
		print_bank_lists(&firmware, ofs[5], 1, "TG300B"); puts("\n--\n");
		print_bank_lists(&firmware, ofs[6], 2, "GM"); puts("\n--\n");
		print_program_banks(&firmware, ofs[7], 221); puts("\n--\n");
		print_normal_voices(&firmware, ofs[8], 1635); puts("\n--\n");
		print_level_scales(&firmware, ofs[9], 1377); puts("\n--\n");

		print_sample_sets(&firmware, ofs[10], 503); puts("\n--\n");
		print_samples(&firmware, ofs[11], num);

		return EXIT_SUCCESS;
	}

	if (opt == '-b')
	{
		static const int ofs_num[][2] =
		{
			#ifdef MU2000_FIRMWARE_V1_01
			{ +1578192, 1427 },
			{ +1625024, 40   },
			{ +1626322, 310  },
			{ +1641020, 47   },
			{ +1642588, 55   },
			{ +1644484, 20   },
			{ +1651568, 5    },
			{ +1651794, 1    },
			{ +1652400, 1    },
			{ +1652466, 1    },
			{ +1652532, 14   },
			{ +1654508, 1    },
			{ +1656772, 1    },
			{ +1657404, 1    },
			{ +1657538, 32   },
			{ +1659036, 58   },
			{ +1660960, 112  },
			{ +1664578, 1    },
			{ +1664712, 1    },
			{ +1665000, 2    },
			{ +1665132, 23   },
			{ +1666018, 48   },
			{ +1667656, 1    },
			{ +1667706, 1    },
			{ +1667774, 16   },
			{ +1668346, 2    },
			{ +1668444, 6    },
			{ +1670036, 32   },
			{ +1671442, 16   },
			{ +1672652, 16   },
			{ +1673470, 16   },
			{ +1674092, 63   },
			{ +1676266, 7    },
			{ +1676592, 16   },
			{ +1677138, 15   },
			{ +1677670, 11   },
			{ +1678116, 2    },
			{ +1678212, 22   },
			{ +1678982, 31   },
			{ +1680044, 10   },
			#else // MU2000_FIRMWARE_V2_01
			{ +1817552, 469  },
			{ +1832696, 1695 },
			{ +1888452, 45   },
			{ +1889910, 310  },
			{ +1904676, 47   },
			{ +1906244, 55   },
			{ +1908140, 37   },
			{ +1915768, 5    },
			{ +1915994, 1    },
			{ +1916600, 1    },
			{ +1916666, 1    },
			{ +1916732, 14   },
			{ +1918708, 1    },
			{ +1920972, 1    },
			{ +1921604, 1    },
			{ +1921738, 32   },
			{ +1923284, 76   },
			{ +1925784, 112  },
			{ +1929402, 1    },
			{ +1929536, 1    },
			{ +1929824, 2    },
			{ +1929956, 23   },
			{ +1930842, 48   },
			{ +1932480, 1    },
			{ +1932530, 1    },
			{ +1932598, 16   },
			{ +1933170, 2    },
			{ +1933268, 6    },
			{ +1934860, 32   },
			{ +1936266, 16   },
			{ +1937476, 16   },
			{ +1938294, 16   },
			{ +1938916, 63   },
			{ +1941090, 7    },
			{ +1941416, 16   },
			{ +1941962, 15   },
			{ +1942494, 11   },
			{ +1942940, 2    },
			{ +1943036, 22   },
			{ +1943806, 31   },
			{ +1944904, 40   },
			{ +1946190, 52   }
			#endif
		};

		FILE* const fp = fopen("table/mu2000_bitmap.txt", "wb");
		if (!fp) return EXIT_FAILURE;

		static const unsigned short bom = 0xFEFF;
		fwrite(&bom, 2, 1, fp);

		write_utf16_str("MU2000 Bitmaps\n\n", fp);

		static const int m = sizeof(ofs_num) / sizeof(ofs_num[0]);
		int n = 0;

		for (int i = 0; i < m; ++i)
		{
			const int* const ofs = ofs_num[i];
			const int* const num = &ofs[1];

			n += print_bitmaps(fp, &firmware, *ofs, *num);
		}

		fclose(fp);
		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	WDL_HeapBuf wavetbl;

	if (!read_wavetbl(&roms[2], &wavetbl))
	{
		return EXIT_FAILURE;
	}

	if (opt == '-w')
	{
		static const int ofs[] =
		{
			#ifdef MU2000_FIRMWARE_V1_01
			+2361248,
			+1777680, 2891
			#else // MU2000_FIRMWARE_V2_01
			+2637264,
			+2053536, 2901
			#endif
		};

		const int num = ofs[2];

		int n = extract_drum_samples("wave/mu2000/drum_%05d.wav", &firmware, ofs[0], 1027, &wavetbl);
		n += extract_samples("wave/mu2000/sample_%05d.wav", &firmware, ofs[1], num, &wavetbl);

		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	printf("Usage: %s -f | -m | -t | -b | -w\n", argv[0]);
	return EXIT_FAILURE;
}
