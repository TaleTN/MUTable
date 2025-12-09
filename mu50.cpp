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

int read_firmware(const char* const filename, WDL_HeapBuf* const buf, const int size)
{
	unsigned char* ptr = (unsigned char*)buf->ResizeOK(size);
	if (!ptr) return 0;

	FILE* const fp = fopen(filename, "rb");
	if (!fp) return 0;

	const int n = (int)fread(ptr, 1, size, fp) & ~1;
	fclose(fp);

	for (int i = 0; i < n; i += 2)
	{
		unsigned short word;
		memcpy(&word, ptr, 2);

		word = WDL_bswap16(word);
		memcpy(ptr, &word, 2);

		ptr += 2;
	}

	return n == size ? size : 0;
}

int write_firmware(const char* const filename, const WDL_HeapBuf* const buf)
{
	FILE* const fp = fopen(filename, "wb");
	if (!fp) return 0;

	const int n = (int)fwrite(buf->Get(), 1, buf->GetSize(), fp);
	fclose(fp);

	return n;
}

int write_var_len(const int len, FILE* const fp)
{
	assert(len >= 0 && len <= 0x0FFFFFFF);

	if (len >= 0x200000) fputc((len >> 21) | 0x80, fp);
	if (len >= 0x4000) fputc(((len >> 14) & 0x7F) | 0x80, fp);
	if (len >= 0x80) fputc(((len >> 7) & 0x7F) | 0x80, fp);
	fputc(len & 0x7F, fp);

	return len;
}

int write_midi(const char* const filename, const WDL_HeapBuf* const firmware, const int ofs)
{
	FILE* const fp = fopen(filename, "wb");
	if (!fp) return 0;

	static const unsigned char hdr[] =
	{
		'M', 'T', 'h', 'd', 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0xC0, // 192 TPQN
		'M', 'T', 'r', 'k', 0x00, 0x00, 0x00, 0x07, 0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20 // ~120.01 BPM
	};

	fwrite(hdr, sizeof(hdr), 1, fp);

	const unsigned char* const buf = (const unsigned char*)firmware->Get();
	const int size = firmware->GetSize();

	for (int i = ofs, delta = 0; i < size;)
	{
		const int status = buf[i++];

		// The End
		if (status == 0xF2) break;

		// Delay
		if (status == 0xF3 || status == 0xF4)
		{
			for (int j = 0; j <= 7; j += 7)
			{
				assert(i < size);
				delta += buf[i++] << j;
				if (status == 0xF3) break;
			}
			continue;
		}

		write_var_len(delta, fp);
		delta = 0;

		// SysEx
		if (status == 0xF0)
		{
			int j = i;
			do
			{
				assert(j < size);
			}
			while (buf[j++] != 0xF7);

			fputc(status, fp);
			write_var_len(j - i, fp);

			do
			{
				fputc(buf[i], fp);
			}
			while (buf[i++] != 0xF7);

			continue;
		}

		switch (status & 0xF0)
		{
			// 2-byte Note Off
			case 0x80:
			{
				fputc(status | 0x10, fp);

				assert(i < size);
				fputc(buf[i++], fp);

				fputc(0, fp);
				break;
			}

			// 3-byte MIDI message
			case 0x90: case 0xA0: case 0xB0: case 0xE0:
			{
				fputc(status, fp);

				for (int j = 0; j < 2; ++j)
				{
					assert(i < size);
					fputc(buf[i++], fp);
				}
				break;
			}

			// 2-byte MIDI message
			case 0xC0: case 0xD0:
			{
				fputc(status, fp);

				assert(i < size);
				fputc(buf[i++], fp);
				break;
			}

			default: assert(false);
		}
	}

	static const int m = sizeof(hdr) - 7;
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
		"TG300B",
		"XG",
		"SFX"
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
		"XG Standard Kit",
		"XG Room Kit",
		"XG Rock Kit",
		"XG Electro Kit",
		"XG Analog Kit",
		"XG Jazz Kit",
		"XG Brush Kit",
		"XG Symphony Kit",
		"XG SFX Kit 1",
		"XG SFX Kit 2",
		"Silent Kit",
		"XG Standard Kit 2",
		"DOC C/M Kit"
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
+24     | 3    | 000000 - 3FFFFF | Loop point address        | 4 MB wave ROM address      | 000000
+27     | 1    | Bit 6  - 7      | Sample format             | 1:12-bit, 2:8-bit, 3:DPCM  | 00
        |      | Bit 0  - 5      | Loop fraction             | 0 - 63                     |
        |      | Bit 2  - 4      | DPCM scale                | 2^0 - 2^7                  |
        |      | Bit 0  - 1      | DPCM offset               | 0:-7, 1:-6, 2:-4, 3:0      |
+28     | 1    | 80     - 7F     | Pitch fine                | -128 - +127 [cent]         | 00
+29     | 1    | 80     - 7F     | Pitch coarse              | -128 - +127 [semitone]     | 00

*/
void print_drum_voices(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Drum Voices (+%d)\n", ofs);

	static const char* const hdr  = "                                              Voice                                               |                                 Sample                                  \n"
	                                "Offset  | PC  | PF  | Lvl | Alt | Pan | Rev | Cho | Var | K | 0 | 1 | Cut | Q   | A   | D1  | D2  | SFX#  | Rat | Attack | -  | Loop   | Addr   | F | Fr | D8 | PF   | PC   ";
	static const char* const line = "--------+-----+-----+-----+-----+-----+-----+-----+-----+---+---+---+-----+-----+-----+-----+-----+-------+-----+--------+----+--------+--------+---+----+----+------+------";

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

		printf("%-6d | ", attack);
		printf("%02X | %-6d | ", ptr[21], loop);
		printf("%06X | ", addr);

		const int format = ptr[27] >> 6, frac = ptr[27] & 0x3F;
		const int dpcm = frac /* & 0x1F */;

		// const double frac_loop = (double)((loop << 6) - (format != 3 && loop ? frac : 0)) * 0.015625;

		printf("%d | ", format);
		printf(format != 3 ? "%-2d |    | " : "   | %02X | ", format != 3 ? frac : dpcm);
		printf("%-+4d | %-+4d \n", (signed char)ptr[28], (signed char)ptr[29]);

		ptr += 30;
		ofs += 30;
	}
}

void print_sfx_voices(const WDL_HeapBuf* const firmware, const int normal_voices, const int ofs, const int num)
{
	printf("SFX Voices (+%d)\n\n", ofs);

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	printf("SFX#  "); for (int i = 0; i < 10; ++i) printf("| %-7d ", i);
	printf("\n------"); for (int i = 0; i < 10; ++i) printf("+---------");

	for (int i = 0; i < num; ++i)
	{
		if (!(i % 10)) printf("\n%-5d ", i);

		const int normal_voice_ofs = ((ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3]) - normal_voices;
		printf("| %-+7d ", normal_voice_ofs);

		ptr += 4;
	}

	putchar('\n');
}

void print_bank_lists(const WDL_HeapBuf* const firmware, const int ofs, const int num)
{
	printf("Bank Lists (+%d)\n", ofs);

	static const char* const name[] =
	{
		"TG300B",
		"XG",
		"SFX"
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
		"TG300B Bank 16",
		"TG300B Bank 17",
		"TG300B Bank 18",
		"TG300B Bank 19",
		"TG300B Bank 24",
		"TG300B Bank 25",
		"TG300B Bank 26",
		"TG300B Bank 32",
		"TG300B Bank 33",
		"TG300B Bank 40",
		"TG300B Bank 126",
		"TG300B Bank 127",
		"Silence",
		"DOC Voice List"
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

			const int normal_voice_ofs = ((ptr[0] << 8) | ptr[1]) << 1;
			printf("| %-+7d ", normal_voice_ofs);

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
+2      | 8    | 20     - 7F     | Voice name                | ASCII, space padded        |
+10     | 80   | See below       | Element 1                 | See below                  |
+80     | 80   | See below       | Element 2 (optional)      | See below                  |

Element 1/2

Offset  | Size | Data            | Parameter                 | Description                | Default
--------+------+-----------------+---------------------------+----------------------------+---------
+0      | 1    | 00     - 7F     | Wave# MSB                 | 0 - 245                    |
+1      | 1    | 00     - 7F     | Wave# LSB                 |                            |
+2      | 1    | 00     - 7F     | Note limit low            | 0 - 127 [note]             | 00
+3      | 1    | 00     - 7F     | Note limit high           | 0 - 127 [note]             | 7F
+4      | 1    | 01     - 7F     | Velocity limit low        | 1 - 127                    | 01
+5      | 1    | 01     - 7F     | Velocity limit high       | 1 - 127                    | 7F
+6      | 1    | 00     - 01     | Filter EG velocity curve  | 0:linear, 1:exponential    | 01
--------+------+-----------------+---------------------------+----------------------------+---------
+7      | 1    | 00     - 02     | LFO wave                  | 0:saw, 1:tri, 2:s&h        | 01
+8      | 1    | 00     - 01     | LFO phase init            | 0:off, 1:on                | 00
+9      | 1    | 00     - 3F     | LFO speed                 | 0 - 63                     | 1F
+10     | 1    | 00     - 7F     | Vibrato delay time        | 0 - 127                    | 00
+11     | 1    | 00     - 7F     | Vibrato fade time         | 0 - 127                    | 00
+12     | 1    | 00     - 3F     | LFO pitch mod depth       | 0 - 63                     | 00
+13     | 1    | 00     - 0F     | LFO filter mod depth      | 0 - 15                     | 00
+14     | 1    | 00     - 1F     | LFO amp mod depth         | 0 - 31                     | 00
--------+------+-----------------+---------------------------+----------------------------+---------
+15     | 1    | 20     - 60     | Note shift                | -32 - +32 [semitone]       | 40
+16     | 1    | 0E     - 72     | Detune                    | -50 - +50 [cent]           | 40
+17     | 1    | 00     - 05     | Pitch scaling depth       | 0 - 5: 0/5/10/20/50/100%   | 00
+18     | 1    | 00     - 7F     | Pitch scaling center note | 0 - 127 [note]             | 3C
+19     | 1    | 00     - 01     | Pitch EG depth            | 0:0.5, 1:1, 2:2, 3:4 [oct] | 01
+20     | 1    | 39     - 47     | PEG velocity level sens.  | -7 - +7                    | 40
+21     | 1    | 39     - 47     | PEG velocity rate sens.   | -7 - +7                    | 40
+22     | 1    | 39     - 47     | Pitch EG rate scaling     | -7 - +7                    | 40
+23     | 1    | 00     - 7F     | Pitch EG RS center note   | 0 - 127 [note]             | 3C
+24     | 1    | 00     - 3F     | Pitch EG attack rate      | 0 - 63                     | 3F
+25     | 1    | 00     - 3F     | Pitch EG decay 1 rate     | 0 - 63                     | 3F
+26     | 1    | 00     - 3F     | Pitch EG decay 2 rate     | 0 - 63                     | 3F
+27     | 1    | 00     - 3F     | Pitch EG release rate     | 0 - 63                     | 3F
+28     | 1    | 00     - 7F     | Pitch EG initial level    | -64 - +63                  | 40
+29     | 1    | 00     - 7F     | Pitch EG attack level     | -64 - +63                  | 40
+30     | 1    | 00     - 7F     | Pitch EG decay 1 level    | -64 - +63                  | 40
+31     | 1    | 00     - 7F     | Pitch EG decay 2 level    | -64 - +63                  | 40
+32     | 1    | 00     - 7F     | Pitch EG release level    | -64 - +63                  | 40
--------+------+-----------------+---------------------------+----------------------------+---------
+33     | 1    | 00     - 3F     | Filter resonance          | 0 - 63                     | 00
+34     | 1    | 00     - 07     | Velocity sensitivity      | 0 - 7                      | 00
+35     | 1    | 00     - 7F     | Filter cutoff             | 0 - 127                    | 7F
+36     | 1    | 00     - 7F     | Cutoff scaling break pt 1 | 0 - 127 [note]             | 18
+37     | 1    | 00     - 7F     | Cutoff scaling break pt 2 | 0 - 127 [note]             | 30
+38     | 1    | 00     - 7F     | Cutoff scaling break pt 3 | 0 - 127 [note]             | 48
+39     | 1    | 00     - 7F     | Cutoff scaling break pt 4 | 0 - 127 [note]             | 60
+40     | 1    | 00     - 7F     | Cutoff scaling offset 1   | -64 - +63                  | 40
+41     | 1    | 00     - 7F     | Cutoff scaling offset 2   | -64 - +63                  | 40
+42     | 1    | 00     - 7F     | Cutoff scaling offset 3   | -64 - +63                  | 40
+43     | 1    | 00     - 7F     | Cutoff scaling offset 4   | -64 - +63                  | 40
+44     | 1    | 39     - 47     | FEG velocity level sens.  | -7 - +7                    | 40
+45     | 1    | 39     - 47     | FEG velocity rate sens.   | -7 - +7                    | 40
+46     | 1    | 39     - 47     | Filter EG rate scaling    | -7 - +7                    | 40
+47     | 1    | 00     - 7F     | Filter EG RS center note  | 0 - 127 [note]             | 3C
+48     | 1    | 00     - 3F     | Filter EG attack rate     | 0 - 63                     | 3F
+49     | 1    | 00     - 3F     | Filter EG decay 1 rate    | 0 - 63                     | 3F
+50     | 1    | 00     - 3F     | Filter EG decay 2 rate    | 0 - 63                     | 3F
+51     | 1    | 00     - 3F     | Filter EG release rate    | 0 - 63                     | 3F
+52     | 1    | 00     - 7F     | Filter EG initial level   | -64 - +63                  | 40
+53     | 1    | 00     - 7F     | Filter EG attack level    | -64 - +63                  | 40
+54     | 1    | 00     - 7F     | Filter EG decay 1 level   | -64 - +63                  | 40
+55     | 1    | 00     - 7F     | Filter EG decay 2 level   | -64 - +63                  | 40
+56     | 1    | 00     - 7F     | Filter EG release level   | -64 - +63                  | 40
--------+------+-----------------+---------------------------+----------------------------+---------
+57     | 1    | 00     - 7F     | Element level             | 0 - 127, y =~ (x/127)^2    |
+58     | 1    | 00     - 7F     | Level scaling break pt 1  | 0 - 127 [note]             | 18
+59     | 1    | 00     - 7F     | Level scaling break pt 2  | 0 - 127 [note]             | 30
+60     | 1    | 00     - 7F     | Level scaling break pt 3  | 0 - 127 [note]             | 48
+61     | 1    | 00     - 7F     | Level scaling break pt 4  | 0 - 127 [note]             | 60
+62     | 1    | 00     - 7F     | Level scaling offset 1    | -64 - +63                  | 40
+63     | 1    | 00     - 7F     | Level scaling offset 2    | -64 - +63                  | 40
+64     | 1    | 00     - 7F     | Level scaling offset 3    | -64 - +63                  | 40
+65     | 1    | 00     - 7F     | Level scaling offset 4    | -64 - +63                  | 40
+66     | 1    | 00     - 0B     | Velocity curve            | 0 - 6 [?]                  | 00
+67     | 1    | 00     - 0F     | Pan                       | 0 - 14, 15:scaling         | 07
+68     | 1    | 39     - 47     | Amp EG rate scaling       | -7 - +7                    | 40
+69     | 1    | 00     - 7F     | Amp EG RS center note     | 0 - 127 [note]             | 3C
+70     | 1    | 00     - 0F     | Amp EG key on delay       | 0 - 15                     | 00
+71     | 1    | 00     - 3F     | Amp EG attack rate        | 0 - 63                     | 3F
+72     | 1    | 00     - 3F     | Amp EG decay 1 rate       | 0 - 63                     | 3F
+73     | 1    | 00     - 3F     | Amp EG decay 2 rate       | 0 - 63                     | 3F
+74     | 1    | 00     - 3F     | Amp EG release rate       | 0 - 63                     | 3F
+75     | 1    | 00     - 7F     | Amp EG decay 1 level      | -64 - +63                  | 40
+76     | 1    | 00     - 7F     | Amp EG decay 2 level      | -64 - +63                  | 40
+77     | 1    | 00     - 7F     | Address offset MSB        | Attack offset [samples]    | 00
+78     | 1    | 00     - 7F     | Address offset LSB        |                            | 00
+79     | 1    | 39     - 47     | Resonance sensitivity     | -7 - +7                    | 40

*/
void print_normal_voices(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Normal Voices (+%d)\n", ofs);

	static const char* const hdr  = "            Voice            |            Element                |                  LFO                  |                                             Pitch                                              |                                                                Filter                                                                |                                                            Amplitude                                                            \n"
	                                "Offset  | Lvl | E | Name     | Wave# | N1  | N2  | V1  | V2  | F | W | I | Sp | Del | Fad | PM | FM | AM | NS  | Det | S | Ns  | D | VL | VR | RS | Nrs | R1 | R2 | R3 | R4 | L0  | L1  | L2  | L3  | L4  | Q  | VS | Cut | BP1 | BP2 | BP3 | BP4 | Of1 | Of2 | Of3 | Of4 | VL | VR | RS | Nrs | R1 | R2 | R3 | R4 | L0  | L1  | L2  | L3  | L4  | Lvl | BP1 | BP2 | BP3 | BP4 | Of1 | Of2 | Of3 | Of4 | VC | P  | RS | Nrs | R0 | R1  | R2  | R3  | R4  | L2  | L3  | Addr   | QS ";
	static const char* const line = "--------+-----+---+----------+-------+-----+-----+-----+-----+---+---+---+----+-----+-----+----+----+----+-----+-----+---+-----+---+----+----+----+-----+----+----+----+----+-----+-----+-----+-----+-----+----+----+-----+-----+-----+-----+-----+-----+-----+-----+-----+----+----+----+-----+----+----+----+----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+----+----+----+-----+----+-----+-----+-----+-----+-----+-----+--------+----";
	static const char* const skip = "        |     |   |          | ";

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
		const int el = ilog2(ptr[1]) + 1;
		printf("%-3d | %d | ", ptr[0], el);

		for (int k = 2; k < 10; ++k) putchar(ptr[k]);
		printf(" | ");
		ptr += 10;

		for (int k = 0; k < el; ++k)
		{
			if (k) printf(skip);

			// Element
			printf("%-5d | ", (ptr[0] << 7) | ptr[1]);
			printf("%-3d | %-3d | ", ptr[2], ptr[3]);
			printf("%-3d | %-3d | ", ptr[4], ptr[5]);
			printf("%d | ", ptr[6]);

			// LFO
			printf("%d | %d | ", ptr[7], ptr[8]);
			printf("%-2d | %-3d | %-3d | ", ptr[9], ptr[10], ptr[11]);
			printf("%-2d | %-2d | %-2d | ", ptr[12], ptr[13], ptr[14]);

			// Pitch
			printf("%-+3d | %-+3d | ", ptr[15] - 64, ptr[16] - 64);
			printf("%d | %-3d | ", ptr[17], ptr[18]);
			printf("%d | %-+2d | %-+2d | ", ptr[19], ptr[20] - 64, ptr[21] - 64);
			printf("%-+2d | %-3d | ", ptr[22] - 64, ptr[23]);
			for (int l = 0; l < 4; ++l) printf("%-2d | ", ptr[24 + l]);
			for (int l = 0; l < 5; ++l) printf("%-+3d | ", ptr[28 + l] - 64);

			// Filter
			printf("%-2d | %-2d | %-3d | ", ptr[33], ptr[34], ptr[35]);
			for (int l = 0; l < 4; ++l) printf("%-3d | ", ptr[36 + l]);
			for (int l = 0; l < 4; ++l) printf("%-+3d | ", ptr[40 + l] - 64);
			printf("%-+2d | %-+2d | ", ptr[44] - 64, ptr[45] - 64);
			printf("%-+2d | %-3d | ", ptr[46] - 64, ptr[47]);
			for (int l = 0; l < 4; ++l) printf("%-2d | ", ptr[48 + l]);
			for (int l = 0; l < 5; ++l) printf("%-+3d | ", ptr[52 + l] - 64);

			// Amplitude
			printf("%-3d | ", ptr[57]);
			for (int l = 0; l < 4; ++l) printf("%-3d | ", ptr[58 + l]);
			for (int l = 0; l < 4; ++l) printf("%-+3d | ", ptr[62 + l] - 64);
			printf("%-2d | %-2d | ", ptr[66], ptr[67]);
			printf("%-+2d | %-3d | ", ptr[68] - 64, ptr[69]);
			for (int l = 0; l < 5; ++l) printf("%-*d | ", !!l + 2, ptr[70 + l]);
			for (int l = 0; l < 2; ++l) printf("%-+3d | ", ptr[75 + l] - 64);
			printf("%-+6d | %-+2d \n", (ptr[77] << 7) | ptr[78], ptr[79] - 64);

			ptr += 80;
		}

		ofs += 10 + el * 80;
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

		const int sample_ofs = (ptr[0] << 8) | ptr[1];
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
+2      | 1    | 80     - 7F     | Pitch fine                | -128 - +127 [cent]         | 40
+3      | 1    | 00              | Not used                  | 00                         | 00
+4      | 2    | 0000   - FFFF   | Attack length             | 0 - 65535 [samples]        | 0000
+6      | 1    | 00              | Not used                  | 00                         | 00
+7      | 2    | 0000   - FFFF   | Loop length               | 0 - 65535 [samples]        | 0000
+9      | 3    | 000000 - 3FFFFF | Loop point address        | 4 MB wave ROM address      | 000000
+12     | 1    | Bit 6  - 7      | Sample format             | 1:12-bit, 2:8-bit, 3:DPCM  | 00
        |      | Bit 0  - 5      | Loop fraction             | 0 - 63                     |
        |      | Bit 2  - 4      | DPCM scale                | 2^0 - 2^7                  |
        |      | Bit 0  - 1      | DPCM offset               | 0:-7, 1:-6, 2:-4, 3:0      |
+13     | 1    | 9A     - 76     | ?                         | ?                          | 00
+14     | 1    | 00     - 7F     | Note limit low            | 0 - 127 [note]             | 00
+15     | 1    | 00     - 7F     | Note limit high           | 0 - 126, 255 [note]        | FF

*/
void print_samples(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Samples (+%d)\n", ofs);

	static const char* const hdr  = "Offset  | Atn | PC   | PF   | -  | Attack | -  | Loop   | Addr   | F | Fr | D8 | ?  | N1  | N2  ";
	static const char* const line = "--------+-----+------+------+----+--------+----+--------+--------+---+----+----+----+-----+-----";

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

		const int attack = (ptr[4] << 8) | ptr[5];
		const int loop = (ptr[7] << 8) | ptr[8];
		const int addr = (ptr[9] << 16) | (ptr[10] << 8) | ptr[11];

		printf("%02X | %-6d | ", ptr[3], attack);
		printf("%02X | %-6d | ", ptr[6], loop);
		printf("%06X | ", addr);

		const int format = ptr[12] >> 6, frac = ptr[12] & 0x3F;
		const int dpcm = frac /* & 0x1F */;

		// const double frac_loop = (double)((loop << 6) - (format != 3 && loop ? frac : 0)) * 0.015625;

		printf("%d | ", format);
		printf(format != 3 ? "%-2d |    | " : "   | %02X | ", format != 3 ? frac : dpcm);
		printf("%02X | %-3d | %-3d \n", ptr[13], ptr[14], ptr[15]);

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

int read_wavetbl(const char* const filenames[2], WDL_HeapBuf* const buf, const int num, const int size)
{
	unsigned char* ptr = (unsigned char*)buf->ResizeOK(num * size);
	if (!ptr) return 0;

	int total = 0;

	for (int i = 0; i < num; ++i)
	{
		FILE* const fp = fopen(filenames[i], "rb");
		if (!fp) return 0;

		const int n = (int)fread(ptr, 1, size, fp);
		fclose(fp);

		total += n;
		ptr += n;
	}

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

int write_sample(const char* const filename, const WDL_HeapBuf* const wavetbl, const int format, const int addr, const int attack, const int loop, const int dpcm = 0)
{
	const unsigned char* const buf = (const unsigned char*)wavetbl->Get() + addr;

	static const int extra = 3;
	const int len = attack + loop + extra;

	WaveWriter wav;
	if (!wav.Open(filename, format == 2 ? 8 : 16, 1, 44100, 0)) return 0;

	switch (format)
	{
		// 12-bit signed linear PCM
		case 1:
		{
			const unsigned char* ptr = buf - (attack >> 1) * 3;

			for (int i = 0; i < len; ++i)
			{
				short sample;

				if (!(i & 1))
				{
					sample = (ptr[0] << 4) | (ptr[1] << 12);
				}
				else
				{
					sample = (ptr[1] & 0xF0) | (ptr[2] << 8);
					ptr += 3;
				}

				sample = WDL_bswap16_if_be(sample);
				wav.WriteRaw(&sample, 2);
			}
			break;
		}

		// 8-bit signed linear PCM
		case 2:
		{
			const unsigned char* const ptr = buf - attack;

			for (int i = 0; i < len; ++i)
			{
				unsigned char sample = ptr[i] ^ 0x80;
				wav.WriteRaw(&sample, 1);
			}
			break;
		}

		// 8-bit signed log DPCM
		case 3:
		{
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

			const unsigned char* const ptr = buf - attack;
			int sum = 0;

			for (int i = 0; i < len; ++i)
			{
				const int step = ptr[i];

				const int delta = log_tbl[step & 0x7F];
				const int min_delta = -delta;

				sum += step & 0x80 ? min_delta : delta;
				sum -= ofs;

				int y = (sum << scale) >> 3;

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
	const int attack = (ptr[0] << 8) | ptr[1];
	const int loop = (ptr[3] << 8) | ptr[4];

	if (!(attack || loop)) return 0;

	const int addr = ((ptr[5] & 0x3F) << 16) | (ptr[6] << 8) | ptr[7];
	const int format = ptr[8] >> 6, dpcm = ptr[8] & 0x1F;

	#ifndef MUTABLE_EXTRACT_DUPLICATES

	static const int max_samples = 626;
	static unsigned char sample_list[max_samples][8];

	static int num_samples = 0;
	unsigned char hash[8];

	memcpy(&hash[0], &ptr[0], 2);
	memcpy(&hash[2], &ptr[3], 5);
	hash[7] = (format << 6) | (format == 3 ? dpcm : 0);

	for (int i = 0; i < num_samples; ++i)
	{
		if (!memcmp(sample_list[i], hash, 8)) return 0;
	}

	assert(num_samples < max_samples);
	memcpy(sample_list[num_samples++], hash, 8);

	#endif

	char fn[128];
	sprintf(fn, filename, ofs);

	return write_sample(fn, wavetbl, format, addr, attack, loop, dpcm);
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

int extract_samples(const char* const filename, const WDL_HeapBuf* const firmware, const int ofs, const int num, const WDL_HeapBuf* const wavetbl)
{
	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;
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
		#ifdef MU50_FIRMWARE_V1_04
		// IC7 PROGRAM ROM 4M v1.04
		"mu50/yamaha_mu50.bin", // SHA1(58c41f10d292cac35ef0e8f93029fbc4685df586)
		#elif defined(MU50_FIRMWARE_V1_06)
		// IC7 PROGRAM ROM 4M v1.06
		"mu50/Yamaha MU50 (1.06) [27C4096].bin", // SHA1(1a1f055ba51764611756f4ac8995c1e74d5af20c)
		#else // MU50_FIRMWARE_V1_05
		// IC7 XR174C0 PROGRAM ROM 4M v1.05
		"mu50/xr174c0.ic7", // SHA1(9ca892920598f9fdf08544dac4c0e54e7d46ee3c)
		#endif

		// IC18 XQ057C0 WAVE ROM 1 16M
		"mu50/xq057c0.ic18", // SHA1(32f653c7644d060f5a6d63a435ae3a7412386d92)
		// IC19 XQ058C0 WAVE ROM 2 16M
		"mu50/xq058c0.ic19"  // SHA1(adf68689b4842ec5bc9b0ea1bb99cf66d2dec4de)
	};

	WDL_HeapBuf firmware;

	if (!read_firmware(roms[0], &firmware, 512*1024))
	{
		return EXIT_FAILURE;
	}

	if (opt == '-f')
	{
		const int size = write_firmware("build/mu50_firmware.bin", &firmware);
		return size == firmware.GetSize() ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (opt == '-m')
	{
		const int size = write_midi("midi/mu50_demo.mid", &firmware, +489600);
		const int n = size > 0;

		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	if (opt == '-t')
	{
		static const int ofs[] =
		{
			#ifdef MU50_FIRMWARE_V1_04
			+141786,
			+242574
			#elif defined(MU50_FIRMWARE_V1_06)
			+141976,
			+242742
			#else // MU50_FIRMWARE_V1_05
			+141914,
			+242634
			#endif
		};

		printf("MU50 Data Tables\n\n");

		print_drum_banks(&firmware, ofs[0], 3); puts("\n--\n");
		print_drum_kits(&firmware, +454886, 23); puts("\n--\n");
		print_drum_voices(&firmware, +444296, 353); puts("\n--\n");
		print_sfx_voices(&firmware, +323584, ofs[1], 87); puts("\n--\n");

		print_bank_lists(&firmware, +443910, 3); puts("\n--\n");
		print_program_banks(&firmware, +425734, 71); puts("\n--\n");
		print_normal_voices(&firmware, +323584, 743); puts("\n--\n");

		print_sample_sets(&firmware, +479030, 247); puts("\n--\n");
		print_samples(&firmware, +460774, 1141);

		return EXIT_SUCCESS;
	}

	if (opt == '-b')
	{
		#ifdef MU50_FIRMWARE_V1_04
		static const int rebase = -60;
		#elif defined(MU50_FIRMWARE_V1_06)
		static const int rebase = +92;
		#else // MU50_FIRMWARE_V1_05
		static const int rebase = 0;
		#endif

		FILE* const fp = fopen("table/mu50_bitmap.txt", "wb");
		if (!fp) return EXIT_FAILURE;

		static const unsigned short bom = 0xFEFF;
		fwrite(&bom, 2, 1, fp);

		write_utf16_str("MU50 Bitmaps\n\n", fp);

		int n = print_bitmaps(fp, &firmware, +157978 + rebase, 279);
		n += print_bitmaps(fp, &firmware, +201538 + rebase, 140);

		fclose(fp);
		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	WDL_HeapBuf wavetbl;

	if (!read_wavetbl(&roms[1], &wavetbl, 2, 2*1024*1024))
	{
		return EXIT_FAILURE;
	}

	if (opt == '-w')
	{
		int n = extract_drum_samples("wave/mu50/drum_%05d.wav", &firmware, +444296, 353, &wavetbl);
		n += extract_samples("wave/mu50/sample_%05d.wav", &firmware, +460774, 1141, &wavetbl);

		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	printf("Usage: %s -f | -m | -t | -b | -w\n", argv[0]);
	return EXIT_FAILURE;
}
