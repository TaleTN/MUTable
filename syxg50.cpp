// Copyright (C) 2019-2024 Theo Niessink <theo@taletn.com>
// This work is free. You can redistribute it and/or modify it under the
// terms of the Do What The Fuck You Want To Public License, Version 2,
// as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WDL/heapbuf.h"
#include "WDL/wdlendian.h"
#include "WDL/wavwrite.h"

/* Voice Table File

Offset  | Size  | Description
--------+-------+----------------------------------------------------
+0      | 16    | Voice table version string (MU50 2/4MB V2.0)
+16     | 15    | Wave table filename        (when bit 7 @ +31 is 0)
+16     | 4     | Embedded wave table offset (when bit 7 @ +31 is 1)
+20     | 4     | Embedded wave table size   (when bit 7 @ +31 is 1)
+31     | Bit 7 | 0:file,      1:embedded
+31     | Bit 0 | 0:encrypted, 1:non-encrypted
--------+-------+----------------------------------------------------
+32     | 4     | GS drum bank size     (128)
+36     | 4     | XG drum bank size     (128)
+40     | 4     | XG SFX  bank size     (128)
+44     | 4     | GM drum bank size     (128)
--------+-------+----------------------------------------------------
+48     | 4     | Drum kits size        (n*256, n = 31)
+52     | 4     | Drum voices size      (n*30,  n = 312)
+56     | 4     | SFX  voices size      (n*2,   n = 87)
--------+-------+----------------------------------------------------
+60     | 4     | GS  bank list size    (128)
+64     | 4     | SFX bank list size    (128)
+68     | 4     | XG  bank list size    (128)
+72     | 4     | GM  bank list size    (128)
--------+-------+----------------------------------------------------
+76     | 4     | GS banks size         (n*256, n = 24)
+80     | 4     | XG/GM banks size      (n*256, n = 56)
+84     | 4     | Normal voices A size  (64298)
+88     | 4     | Normal voices B size  (23726)
--------+-------+----------------------------------------------------
+92     | 4     | Sample sets size      (n*2,  n = 246)
+96     | 4     | Samples size          (n*16, n = 1098 or 1089)

*/
int read_firmware(const char* const filename, WDL_HeapBuf* const buf)
{
	FILE* const fp = fopen(filename, "rb");
	if (!fp) return 0;

	fseek(fp, 0, SEEK_END);
	const int size = (int)ftell(fp);

	void* const ptr = size > 0 ? buf->ResizeOK(size) : NULL;

	fseek(fp, 0, SEEK_SET);
	const int n = ptr ? (int)fread(ptr, 1, size, fp) : 0;
	fclose(fp);

	return n == size ? size : 0;
}

void print_drum_banks(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("Drum Banks (+%d)\n", ofs);

	static const char* const name[] =
	{
		"GS",
		"XG",
		"SFX",
		"GM"
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
		"Silent Kit",
		"GS Standard Kit",
		"GS Room Kit",
		"GS Power Kit",
		"GS Electro Kit",
		"GS Analog Kit",
		"GS Jazz Kit",
		"GS Brush Kit",
		"GS Orchestra Kit",
		"GS SFX Set",
		"GS C/M Kit",
		"XG Standard Kit",
		"XG Standard Kit 2",
		"XG Room Kit",
		"XG Rock Kit",
		"XG Electro Kit",
		"XG Analog Kit",
		"XG Jazz Kit",
		"XG Brush Kit",
		"XG Symphony Kit",
		"XG SFX Kit 1",
		"XG SFX Kit 2",
		"GM Standard Kit",
		"GM Room Kit",
		"GM Rock Kit",
		"GM Electronic Kit",
		"GM Analog Kit",
		"GM Jazz Kit",
		"GM Brush Kit",
		"GM Orchestra Kit",
		"GM SFX Kit"
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

			const int drum_voice_ofs = ptr[0] | (ptr[1] << 8);
			printf("| %-+6d ", drum_voice_ofs);

			ptr += 2;
		}

		putchar('\n');
	}
}

/* Drum Voices

Offset  | Size | Data            | Parameter                 | Description                | Default
--------+------+-----------------+---------------------------+----------------------------+---------
+0      | 1    | 00     - 7F     | Pitch coarse              | 0 - 127 [note]             | 3C
+1      | 1    | 00     - 7F     | Pitch fine                | -64 - +63 [cent]           | 40
+2      | 1    | 00     - 7F     | Instrument level          | 0 - 127                    | 7F
+3      | 1    | 00     - 7F     | Alternate group           | 0:off, 1 - 127             | 00
+4      | 1    | 00     - 7F     | Pan                       | 0:random, 1 - 127          | 40
+5      | 1    | 00     - 7F     | Reverb send               | 0 - 127                    | 7F
+6      | 1    | 00     - 7F     | Chorus send               | 0 - 127                    | 7F
+7      | 1    | 00     - 7F     | Variation send            | 0 - 127                    | 7F
+8      | 1    | 00     - 01     | Key assign                | 0:single, 1:multi          | 00
+9      | 1    | 00     - 01     | Receive Note Off          | 0:off, 1:on                | 01
+10     | 1    | 00     - 01     | Receive Note On           | 0:off, 1:on                | 01
+11     | 1    | 00     - 7F     | Filter cutoff             | 0 - 127                    | 7F
+12     | 1    | 00     - 7F     | Filter resonance          | 0 - 127                    | 10
+13     | 1    | 00     - 7F     | Amp EG attack rate        | 0 - 127                    | 7F
+14     | 1    | 00     - 7F     | Amp EG decay 1 rate       | 0 - 127                    | 40
+15     | 1    | 00     - 7F     | Amp EG decay 2 rate       | 0 - 127                    | 20
--------+------+-----------------+---------------------------+----------------------------+---------
+16     | 2    | 0000   - FFFF   | SFX voice#                | 0 - 86, 65535:drum         | FFFF
+18     | 1    | 00     - 7F     | Playback rate             | -64 - +63 [semitone]       | 40
+19     | 2    | 0000   - FFFF   | Attack length             | 0 - 65535 [samples]        | 0000
+21     | 1    | 00              | Not used                  | 00                         | 00
+22     | 2    | 0000   - FFFF   | Loop length               | 0 - 65535 [samples]        | 0000
+24     | 3    | 000000 - FFFFFF | Loop point address        | 2/4+ MB wave ROM address   | 000000
+27     | 1    | Bit 7           | Sample format             | 0:16-bit, 1:8-bit          | 00
+28     | 1    | 80     - 7F     | Pitch fine                | -128 - +127 [cent]         | 00
+29     | 1    | 80     - 7F     | Pitch coarse              | -128 - +127 [semitone]     | 00

*/
void print_drum_voices(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Drum Voices (+%d)\n", ofs);

	static const char* const hdr  = "                                              Voice                                               |                             Sample                            \n"
	                                "Offset  | PC  | PF  | Lvl | Alt | Pan | Rev | Cho | Var | K | 0 | 1 | Cut | Q   | A   | D1  | D2  | SFX#  | Rat | Attack | -  | Loop   | Addr   | F | PF   | PC   ";
	static const char* const line = "--------+-----+-----+-----+-----+-----+-----+-----+-----+---+---+---+-----+-----+-----+-----+-----+-------+-----+--------+----+--------+--------+---+------+------";

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
		printf("%-3d | %-+3d | ", ptr[0], ptr[1] - 64);
		printf("%-3d | %-3d | %-3d | ", ptr[2], ptr[3], ptr[4]);
		printf("%-3d | %-3d | %-3d | ", ptr[5], ptr[6], ptr[7]);
		printf("%d | %d | %d | ", ptr[8], ptr[9], ptr[10]);
		printf("%-3d | %-3d | ", ptr[11], ptr[12]);
		printf("%-3d | %-3d | %-3d | ", ptr[13], ptr[14], ptr[15]);

		// Sample
		const int sfx_no = (ptr[16] << 8) | ptr[17];
		printf("%-5d | ", sfx_no);

		printf("%-3d | ", ptr[18]);

		const int attack = (ptr[19] << 8) | ptr[20];
		const int loop = (ptr[22] << 8) | ptr[23];
		const int addr = (ptr[24] << 16) | (ptr[25] << 8) | ptr[26];
		const int format = ptr[27] >> 7;

		printf("%-6d | ", attack);
		printf("%02X | %-6d | ", ptr[21], loop);
		printf("%06X | %0d | ", addr, format);

		printf("%-+4d | %-+4d \n", (signed char)ptr[28], (signed char)ptr[29]);

		ptr += 30;
		ofs += 30;
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

		const int normal_voice_ofs = ptr[0] | (ptr[1] << 8);
		printf("| %c%-+6d ", 'A', normal_voice_ofs);

		ptr += 2;
	}

	putchar('\n');
}

void print_bank_lists(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("Bank Lists (+%d)\n", ofs);

	static const char* const name[] =
	{
		"GS",
		"SFX",
		"XG",
		"GM"
	};

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		printf("\nBank List %d", i);
		if (i < sizeof(name) / sizeof(name[0])) printf(" (%s)", name[i]);
		puts("\n");

		printf("Bank# "); for (int j = 0; j < 10; ++j) printf("| %-3d ", j);
		printf("\n------"); for (int j = 0; j < 10; ++j) printf("+-----");

		for (int j = 0; j < 128; ++j)
		{
			if (!(j % 10)) printf("\n%-5d ", j);

			const int prog_bank_no = *ptr++;
			printf("| %-3d ", prog_bank_no);
		}

		putchar('\n');
	}
}

void print_program_banks(const WDL_HeapBuf* const firmware, const int ofs, const int num, const char* const title = NULL)
{
	if (title) printf("%s ", title);
	printf("Program Banks (+%d)\n", ofs);

	static const char* const name[] =
	{
		"GS Bank 0",
		"GS Bank 1",
		"GS Bank 2",
		"GS Bank 3",
		"GS Bank 4",
		"GS Bank 5",
		"GS Bank 6",
		"GS Bank 7",
		"GS Bank 8",
		"GS Bank 9",
		"GS Bank 10",
		"GS Bank 11",
		"GS Bank 16",
		"GS Bank 17",
		"GS Bank 18",
		"GS Bank 19",
		"GS Bank 24",
		"GS Bank 25",
		"GS Bank 26",
		"GS Bank 32",
		"GS Bank 33",
		"GS Bank 40",
		"GS Bank 126",
		"GS Bank 127",
		"Silence",
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
		"Attack",
		"Release",
		"Resonant Sweep",
		"Muted",
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
		"Other Waves 1",
		"Other Waves 2",
		"Other Waves 3",
		"Other Waves 4",
		"Other Waves 5",
		"Other Waves 6",
		"Other Waves 7",
		"Other Waves 8",
		"Other Waves 9",
		"Other Instrument 1",
		"Other Instrument 2",
		"Other Instrument 3",
		"Other Instrument 4",
		"Other Instrument 5",
		"Other Instrument 6",
		"SFX",
		"GM Bank 0",
		"GM Bank 1",
		"GM Bank 2",
		"GM Bank 3",
		"GM Bank 4",
		"GM Bank 5",
		"GM Bank 6",
		"GM Bank 7",
		"GM Bank 8",
		"GM Bank 9"
	};

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;
	const int c = title ? title[0] : 0;

	for (int i = 0, j = c == 'G' ? 0 : 24; i < num; ++i)
	{
		printf("\nBank %d", i);
		if (j < sizeof(name) / sizeof(name[0])) printf(" (%s)", name[j++]);
		puts("\n");

		printf("Prog# "); for (int k = 1; k <= 10; ++k) printf("| %-7d ", k);
		printf("\n------"); for (int k = 1; k <= 10; ++k) printf("+---------");

		for (int k = 0; k < 128; ++k)
		{
			if (!(k % 10)) printf("\n%-5d ", k);

			const int normal_voice_ofs = ptr[0] | (ptr[1] << 8);
			printf("| %c%-+6d ", (normal_voice_ofs >> 15) + 'A', (normal_voice_ofs & 0x7FFF) << 1);

			ptr += 2;
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

Offset  | Size | Data            | Parameter                 | Description                | Default
--------+------+-----------------+---------------------------+----------------------------+---------
+0      | 1    | 00     - 7F     | Voice level               | y =~ (x/127)^2             |
+1      | 1    | 01     - 03     | Element switch            | 1:1, 3:1+2                 | 01
+2      | 78   | See below       | Element 1                 | See below                  |
+80     | 78   | See below       | Element 2 (optional)      | See below                  |

Element 1/2

Offset  | Size | Data            | Parameter                 | Description                | Default
--------+------+-----------------+---------------------------+----------------------------+---------
+0      | 1    | 00     - FF     | Wave#                     | 0 - 245                    |
+1      | 1    | 00     - 7F     | Note limit low            | 0 - 127 [note]             | 00
+2      | 1    | 00     - 7F     | Note limit high           | 0 - 127 [note]             | 7F
+3      | 1    | 01     - 7F     | Velocity limit low        | 1 - 127                    | 01
+4      | 1    | 01     - 7F     | Velocity limit high       | 1 - 127                    | 7F
--------+------+-----------------+---------------------------+----------------------------+---------
+5      | 1    | Bit 7           | LFO phase init            | 0:off, 1:on                | 01
        |      | Bit 0  - 1      | LFO wave*                 | 0:saw, 1:tri, 2:s&h        |
+6      | 1    | 00     - 01     | Filter EG velocity curve  | 0:linear, 1:exponential    | 01
+7      | 1    | 00     - 3F     | LFO speed                 | 0 - 63                     | 1F
+8      | 1    | 00     - 7F     | Vibrato delay time        | 0 - 127                    | 00
+9      | 1    | 00     - 7F     | Vibrato fade time         | 0 - 127                    | 00
+10     | 1    | 00     - 3F     | LFO pitch mod depth       | 0 - 63                     | 00
+11     | 1    | 00     - 0F     | LFO filter mod depth      | 0 - 15                     | 00
+12     | 1    | 00     - 1F     | LFO amp mod depth         | 0 - 31                     | 00
--------+------+-----------------+---------------------------+----------------------------+---------
+13     | 1    | 20     - 60     | Note shift                | -32 - +32 [semitone]       | 40
+14     | 1    | 0E     - 72     | Detune                    | -50 - +50 [cent]           | 40
+15     | 1    | 00     - 05     | Pitch scaling depth       | 0 - 5: 0/5/10/20/50/100%   | 00
+16     | 1    | 00     - 7F     | Pitch scaling center note | 0 - 127 [note]             | 3C
+17     | 1    | 00     - 01     | Pitch EG depth            | 0:0.5, 1:1, 2:2, 3:4 [oct] | 01
+18     | 1    | 39     - 47     | PEG velocity level sens.  | -7 - +7                    | 40
+19     | 1    | 39     - 47     | PEG velocity rate sens.   | -7 - +7                    | 40
+20     | 1    | 39     - 47     | Pitch EG rate scaling     | -7 - +7                    | 40
+21     | 1    | 00     - 7F     | Pitch EG RS center note   | 0 - 127 [note]             | 3C
+22     | 1    | 00     - 3F     | Pitch EG attack rate      | 0 - 63                     | 3F
+23     | 1    | 00     - 3F     | Pitch EG decay 1 rate     | 0 - 63                     | 3F
+24     | 1    | 00     - 3F     | Pitch EG decay 2 rate     | 0 - 63                     | 3F
+25     | 1    | 00     - 3F     | Pitch EG release rate     | 0 - 63                     | 3F
+26     | 1    | 00     - 7F     | Pitch EG initial level    | -64 - +63                  | 40
+27     | 1    | 00     - 7F     | Pitch EG attack level     | -64 - +63                  | 40
+28     | 1    | 00     - 7F     | Pitch EG decay 1 level    | -64 - +63                  | 40
+29     | 1    | 00     - 7F     | Pitch EG decay 2 level    | -64 - +63                  | 40
+30     | 1    | 00     - 7F     | Pitch EG release level    | -64 - +63                  | 40
--------+------+-----------------+---------------------------+----------------------------+---------
+31     | 1    | 00     - 3F     | Filter resonance          | 0 - 63                     | 00
+32     | 1    | 00     - 07     | Velocity sensitivity      | 0 - 7                      | 00
+33     | 1    | 00     - 7F     | Filter cutoff             | 0 - 127                    | 7F
+34     | 1    | 00     - 7F     | Cutoff scaling break pt 1 | 0 - 127 [note]             | 18
+35     | 1    | 00     - 7F     | Cutoff scaling break pt 2 | 0 - 127 [note]             | 30
+36     | 1    | 00     - 7F     | Cutoff scaling break pt 3 | 0 - 127 [note]             | 48
+37     | 1    | 00     - 7F     | Cutoff scaling break pt 4 | 0 - 127 [note]             | 60
+38     | 1    | 00     - 7F     | Cutoff scaling offset 1   | -64 - +63                  | 40
+39     | 1    | 00     - 7F     | Cutoff scaling offset 2   | -64 - +63                  | 40
+40     | 1    | 00     - 7F     | Cutoff scaling offset 3   | -64 - +63                  | 40
+41     | 1    | 00     - 7F     | Cutoff scaling offset 4   | -64 - +63                  | 40
+42     | 1    | 39     - 47     | FEG velocity level sens.  | -7 - +7                    | 40
+43     | 1    | 39     - 47     | FEG velocity rate sens.   | -7 - +7                    | 40
+44     | 1    | 39     - 47     | Filter EG rate scaling    | -7 - +7                    | 40
+45     | 1    | 00     - 7F     | Filter EG RS center note  | 0 - 127 [note]             | 3C
+46     | 1    | 00     - 3F     | Filter EG attack rate     | 0 - 63                     | 3F
+47     | 1    | 00     - 3F     | Filter EG decay 1 rate    | 0 - 63                     | 3F
+48     | 1    | 00     - 3F     | Filter EG decay 2 rate    | 0 - 63                     | 3F
+49     | 1    | 00     - 3F     | Filter EG release rate    | 0 - 63                     | 3F
+50     | 1    | 00     - 7F     | Filter EG initial level   | -64 - +63                  | 40
+51     | 1    | 00     - 7F     | Filter EG attack level    | -64 - +63                  | 40
+52     | 1    | 00     - 7F     | Filter EG decay 1 level   | -64 - +63                  | 40
+53     | 1    | 00     - 7F     | Filter EG decay 2 level   | -64 - +63                  | 40
+54     | 1    | 00     - 7F     | Filter EG release level   | -64 - +63                  | 40
--------+------+-----------------+---------------------------+----------------------------+---------
+55     | 1    | 00     - 7F     | Element level             | 0 - 127, y =~ (x/127)^2    |
+56     | 1    | 00     - 7F     | Level scaling break pt 1  | 0 - 127 [note]             | 18
+57     | 1    | 00     - 7F     | Level scaling break pt 2  | 0 - 127 [note]             | 30
+58     | 1    | 00     - 7F     | Level scaling break pt 3  | 0 - 127 [note]             | 48
+59     | 1    | 00     - 7F     | Level scaling break pt 4  | 0 - 127 [note]             | 60
+60     | 1    | 00     - 7F     | Level scaling offset 1    | -64 - +63                  | 40
+61     | 1    | 00     - 7F     | Level scaling offset 2    | -64 - +63                  | 40
+62     | 1    | 00     - 7F     | Level scaling offset 3    | -64 - +63                  | 40
+63     | 1    | 00     - 7F     | Level scaling offset 4    | -64 - +63                  | 40
+64     | 1    | 00     - 0C     | Velocity curve            | 0 - 6                      | 00
+65     | 1    | 00     - 0F     | Pan                       | 0 - 14, 15:scaling         | 07
+66     | 1    | 39     - 47     | Amp EG rate scaling       | -7 - +7                    | 40
+67     | 1    | 00     - 7F     | Amp EG RS center note     | 0 - 127 [note]             | 3C
+68     | 1    | 00     - 0F     | Amp EG key on delay       | 0 - 15                     | 00
+69     | 1    | 00     - 3F     | Amp EG attack rate        | 0 - 63                     | 3F
+70     | 1    | 00     - 3F     | Amp EG decay 1 rate       | 0 - 63                     | 3F
+71     | 1    | 00     - 3F     | Amp EG decay 2 rate       | 0 - 63                     | 3F
+72     | 1    | 00     - 3F     | Amp EG release rate       | 0 - 63                     | 3F
+73     | 1    | 00     - 7F     | Amp EG decay 1 level      | -64 - +63                  | 40
+74     | 1    | 00     - 7F     | Amp EG decay 2 level      | -64 - +63                  | 40
+75     | 1    | 00     - 7F     | Address offset MSB        | Attack offset [samples]    | 00
+76     | 1    | 00     - 7F     | Address offset LSB        |                            | 00
+77     | 1    | 39     - 47     | Resonance sensitivity     | -7 - +7                    | 40

*/
void print_normal_voices(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Normal Voices (+%d)\n", ofs);

	static const char* const hdr  = "      Voice       |            Element            |                    LFO                    |                                             Pitch                                              |                                                                Filter                                                                |                                                            Amplitude                                                            \n"
	                                "Offset  | Lvl | E | Wave# | N1  | N2  | V1  | V2  | I | W | F | Sp | Del | Fad | PM | FM | AM | NS  | Det | S | Ns  | D | VL | VR | RS | Nrs | R1 | R2 | R3 | R4 | L0  | L1  | L2  | L3  | L4  | Q  | VS | Cut | BP1 | BP2 | BP3 | BP4 | Of1 | Of2 | Of3 | Of4 | VL | VR | RS | Nrs | R1 | R2 | R3 | R4 | L0  | L1  | L2  | L3  | L4  | Lvl | BP1 | BP2 | BP3 | BP4 | Of1 | Of2 | Of3 | Of4 | VC | P  | RS | Nrs | R0 | R1  | R2  | R3  | R4  | L2  | L3  | Addr   | QS ";
	static const char* const line = "--------+-----+---+-------+-----+-----+-----+-----+---+---+---+----+-----+-----+----+----+----+-----+-----+---+-----+---+----+----+----+-----+----+----+----+----+-----+-----+-----+-----+-----+----+----+-----+-----+-----+-----+-----+-----+-----+-----+-----+----+----+----+-----+----+----+----+----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+----+----+----+-----+----+-----+-----+-----+-----+-----+-----+--------+----";
	static const char* const skip = "        |     |   | ";

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

		printf("%c%-+6d | ", (ofs >= 64298) + 'A', ofs % 64298);

		// Voice
		const int el = ilog2(ptr[1]) + 1;
		printf("%-3d | %d | ", ptr[0], el);
		ptr += 2;

		for (int k = 0; k < el; ++k)
		{
			if (k) printf(skip);

			// Element
			printf("%-5d | ", ptr[0]);
			printf("%-3d | %-3d | ", ptr[1], ptr[2]);
			printf("%-3d | %-3d | ", ptr[3], ptr[4]);

			// LFO
			printf("%d | %d | %d | ", ptr[5] >> 7, ptr[5] & 0x7F, ptr[6]);
			printf("%-2d | %-3d | %-3d | ", ptr[7], ptr[8], ptr[9]);
			printf("%-2d | %-2d | %-2d | ", ptr[19], ptr[11], ptr[12]);

			// Pitch
			printf("%-+3d | %-+3d | ", ptr[13] - 64, ptr[14] - 64);
			printf("%d | %-3d | ", ptr[15], ptr[16]);
			printf("%d | %-+2d | %-+2d | ", ptr[17], ptr[18] - 64, ptr[19] - 64);
			printf("%-+2d | %-3d | ", ptr[20] - 64, ptr[21]);
			for (int l = 0; l < 4; ++l) printf("%-2d | ", ptr[22 + l]);
			for (int l = 0; l < 5; ++l) printf("%-+3d | ", ptr[26 + l] - 64);

			// Filter
			printf("%-2d | %-2d | %-3d | ", ptr[31], ptr[32], ptr[33]);
			for (int l = 0; l < 4; ++l) printf("%-3d | ", ptr[34 + l]);
			for (int l = 0; l < 4; ++l) printf("%-+3d | ", ptr[38 + l] - 64);
			printf("%-+2d | %-+2d | ", ptr[42] - 64, ptr[43] - 64);
			printf("%-+2d | %-3d | ", ptr[44] - 64, ptr[45]);
			for (int l = 0; l < 4; ++l) printf("%-2d | ", ptr[46 + l]);
			for (int l = 0; l < 5; ++l) printf("%-+3d | ", ptr[50 + l] - 64);

			// Amplitude
			printf("%-3d | ", ptr[55]);
			for (int l = 0; l < 4; ++l) printf("%-3d | ", ptr[56 + l]);
			for (int l = 0; l < 4; ++l) printf("%-+3d | ", ptr[60 + l] - 64);
			printf("%-2d | %-2d | ", ptr[64], ptr[65]);
			printf("%-+2d | %-3d | ", ptr[66] - 64, ptr[67]);
			for (int l = 0; l < 5; ++l) printf("%-*d | ", !!l + 2, ptr[68 + l]);
			for (int l = 0; l < 2; ++l) printf("%-+3d | ", ptr[73 + l] - 64);
			printf("%-+6d | %-+2d \n", (ptr[75] << 7) | ptr[76], ptr[77] - 64);

			ptr += 78;
		}

		ofs += 2 + el * 78;
		if ((j += el) >= 25) j = 0;
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

		const int sample_ofs = ptr[0] | (ptr[1] << 8);
		printf("| %-+6d ", sample_ofs);

		ptr += 2;
	}

	putchar('\n');
}

/* Samples

Offset  | Size | Data            | Parameter                 | Description                | Default
--------+------+-----------------+---------------------------+----------------------------+---------
+0      | 1    | 00     - 7F     | Attenuation               | y =~ 2^(-x/8)              | 00
+1      | 1    | 80     - 7F     | Pitch coarse              | -128 - 127 [note]          | 3C
+2      | 1    | 80     - 7F     | Pitch fine                | -128 - +127 [cent]         | 00
+3      | 1    | 00              | Not used                  | 00                         | 00
+4      | 2    | 0000   - FFFF   | Attack length             | 0 - 65535 [samples]        | 0000
+6      | 1    | 00              | Not used                  | 00                         | 00
+7      | 2    | 0000   - FFFF   | Loop length               | 0 - 65535 [samples]        | 0000
+9      | 3    | 000000 - FFFFFF | Loop point address        | 2/4+ MB wave ROM address   |
+12     | 1    | Bit 7           | Sample format             | 0:16-bit, 1:8-bit          | 00
+13     | 1    | 00              | Not used                  | 00                         | 00
+14     | 1    | 00     - 7F     | Note limit low            | 0 - 127 [note]             | 00
+15     | 1    | 00     - 7F     | Note limit high           | 0 - 126, 255 [note]        | FF

*/
void print_samples(const WDL_HeapBuf* const firmware, int ofs, int num)
{
	printf("Samples (+%d)\n", ofs);

	static const char* const hdr  = "Offset  | Atn | PC   | PF   | -  | Attack | -  | Loop   | Addr   | F | -  | N1  | N2  ";
	static const char* const line = "--------+-----+------+------+----+--------+----+--------+--------+---+----+-----+-----";

	const unsigned char* ptr = (const unsigned char*)firmware->Get();

	if (num >= INT_MAX)
	{
		memcpy(&num, &ptr[+96], 4);
		num = WDL_bswap32_if_be(num);
		num >>= 4;
	}

	ptr += ofs;
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

		const int attack = (ptr[4] << 8) | ptr[5];
		const int loop = (ptr[7] << 8) | ptr[8];
		const int addr = (ptr[9] << 16) | (ptr[10] << 8) | ptr[11];
		const int format = ptr[12] >> 7;

		printf("%02X | %-6d | ", ptr[3], attack);
		printf("%02X | %-6d | ", ptr[6], loop);
		printf("%06X | %d | ", addr, format);

		printf("%02X | %-3d | %-3d \n", ptr[13], ptr[14], ptr[15]);

		ptr += 16;
		ofs += 16;
	}
}

int read_wavetbl(const char* const filename, WDL_HeapBuf* const buf, const int decrypt = 0x5D)
{
	const int size = read_firmware(filename, buf);
	if (!size) return 0;

	if (decrypt)
	{
		unsigned char* const ptr = (unsigned char*)buf->Get();
		unsigned char a = decrypt, b = 0;

		for (int i = 0; i < size; ++i)
		{
			const unsigned char c = ptr[i] ^ a ^ b;
			ptr[i] = (c >> 4) | (c << 4);

			a = ~a;
			b++;
		}
	}

	return size;
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

int write_sample(const char* const filename, const WDL_HeapBuf* const wavetbl, const int format, const int addr, const int attack, const int loop)
{
	const unsigned char* const buf = (const unsigned char*)wavetbl->Get() + addr;
	const int len = attack + loop;

	WaveWriter wav;
	if (!wav.Open(filename, format == 0 ? 16 : 8, 1, 44100, 0)) return 0;

	switch (format)
	{
		// 16-bit unsigned linear PCM
		case 0:
		{
			const unsigned char* ptr = buf - (attack << 1);

			for (int i = 0; i < len; ++i)
			{
				short sample = (ptr[0] | (ptr[1] << 8)) ^ 0x8000;

				sample = WDL_bswap16_if_be(sample);
				wav.WriteRaw(&sample, 2);

				ptr += 2;
			}
			break;
		}

		// 8-bit unsigned linear PCM
		case 1:
		{
			wav.WriteRaw((unsigned char*)buf - attack, len);
			break;
		}

		default: assert(false);
	}

	wav.EndDataChunk();
	if (loop) write_cue_points(&wav, attack);
	wav.Close();

	return len;
}

int extract_sample(const char* const filename, const int ofs, const WDL_HeapBuf* const wavetbl, const unsigned char* const ptr)
{
	const int attack = (ptr[0] << 8) | ptr[1];
	const int loop = (ptr[3] << 8) | ptr[4];

	if (!(attack || loop)) return 0;

	const int addr = (ptr[5] << 16) | (ptr[6] << 8) | ptr[7];
	const int format = ptr[8] >> 7;

	#ifndef MUTABLE_EXTRACT_DUPLICATES

	static const int max_samples = 586;
	static unsigned char sample_list[max_samples][8];

	static int num_samples = 0;
	unsigned char hash[8];

	memcpy(&hash[0], &ptr[0], 2);
	memcpy(&hash[2], &ptr[3], 6);

	for (int i = 0; i < num_samples; ++i)
	{
		if (!memcmp(sample_list[i], hash, 8)) return 0;
	}

	assert(num_samples < max_samples);
	memcpy(sample_list[num_samples++], hash, 8);

	#endif

	char fn[128];
	sprintf(fn, filename, ofs);

	return write_sample(fn, wavetbl, format, addr, attack, loop);
}

int extract_drum_samples(const char* const filename, const WDL_HeapBuf* const firmware, const int ofs, const int num, const WDL_HeapBuf* const wavetbl)
{
	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;
	int n = 0;

	for (int i = 0; i < num; ++i)
	{
		const int sfx_no = (ptr[16] << 8) | ptr[17];

		if (sfx_no == 0xFFFF)
		{
			n += extract_sample(filename, i * 30, wavetbl, &ptr[19]) > 0;
		}

		ptr += 30;
	}

	return n;
}

int extract_samples(const char* const filename, const WDL_HeapBuf* const firmware, const int ofs, int num, const WDL_HeapBuf* const wavetbl)
{
	const unsigned char* ptr = (const unsigned char*)firmware->Get();

	if (num >= INT_MAX)
	{
		memcpy(&num, &ptr[+96], 4);
		num = WDL_bswap32_if_be(num);
		num >>= 4;
	}

	ptr += ofs;
	int n = 0;

	for (int i = 0; i < num; ++i)
	{
		n += extract_sample(filename, i * 16, wavetbl, &ptr[4]) > 0;
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
		#ifdef SYXG50_WAVETBL_2MB
		// MU50 2MB v2.0
		"syxg50/SXGBIN21.TBL", // SHA1(4b59d9a11a4caa2a9425b3c025541bbe3e6dd4b9)
		"syxg50/SXGWAVE2.TBL"  // SHA1(08d2f3801b0305a3dad3910f635a2ca816b8be90)
		#else // SYXG50_WAVETBL_4MB
		// MU50 4MB v2.0
		"syxg50/sxgbin41.tbl", // SHA1(fe6e1b39c808704fd1a59a6238807bc819d20a11)
		"syxg50/Sxgwave4.tbl"  // SHA1(22b0a62d13e013ef7f459b1255ffd079d7b3aac7)
		#endif
	};

	WDL_HeapBuf firmware;

	if (!read_firmware(roms[0], &firmware))
	{
		return EXIT_FAILURE;
	}

	if (opt == '-t')
	{
		#ifdef SYXG50_WAVETBL_2MB
		static const int size = 2;
		#else // SYXG50_WAVETBL_4MB
		static const int size = 4;
		#endif

		printf("S-YXG50 (%dMB) Data Tables\n\n", size);

		print_drum_banks(&firmware, +100, 4); puts("\n--\n");
		print_drum_kits(&firmware, +612, 31); puts("\n--\n");
		print_drum_voices(&firmware, +8548, 312); puts("\n--\n");
		print_sfx_voices(&firmware, +17908, 87); puts("\n--\n");

		print_bank_lists(&firmware, +18082, 4); puts("\n--\n");
		print_program_banks(&firmware, +18594, 24, "GS"); puts("\n--\n");
		print_program_banks(&firmware, +24738, 56, "XG/GM"); puts("\n--\n");
		print_normal_voices(&firmware, +39074, 683); puts("\n--\n");

		print_sample_sets(&firmware, +127098, 246); puts("\n--\n");
		print_samples(&firmware, +127590, INT_MAX);

		return EXIT_SUCCESS;
	}

	WDL_HeapBuf wavetbl;

	if (!read_wavetbl(roms[1], &wavetbl))
	{
		return EXIT_FAILURE;
	}

	if (opt == '-w')
	{
		int n = extract_drum_samples("wave/syxg50/drum_%05d.wav", &firmware, +8548, 312, &wavetbl);
		n += extract_samples("wave/syxg50/sample_%05d.wav", &firmware, +127590, INT_MAX, &wavetbl);

		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	printf("Usage: %s -t | -w\n", argv[0]);
	return EXIT_FAILURE;
}
