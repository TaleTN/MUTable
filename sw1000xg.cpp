// Copyright (C) 2024, 2025 Theo Niessink <theo@taletn.com>
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
		'M', 'T', 'r', 'k', 0x00, 0x00, 0x00, 0x00
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
		"TG300B",
		"MU Basic",
		"MU100 Native",
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
		"XG Dry Kit",
		"XG Jungle Kit",
		"XG Analog Kit 2",
		"XG Hip Hop Kit",
		"XG Dance Kit",
		"XG Bright Kit",
		"XG Dark Room Kit",
		"XG Rock Kit 2",
		"XG Jazz Kit 2",
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
		"XG Skim Kit",
		"XG Slim Kit",
		"XG Tramp Kit",
		"XG Amber Ki",
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
		"MU100 Native Kit"
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
+0      | 1    | 00     - 7F     | Pitch coarse              | 0 - 127 [note]              | 3C
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
+16     | 1    | 00              | ?                         | ?                           | 00
+17     | 1    | 00     - 7F     | EQ bass gain [?]          | -12 - +12 [dB]              | 40
+18     | 1    | 04     - 28     | EQ bass frequency         | 32 - 2.0k [Hz]              | 0C
+19     | 1    | 00     - 7F     | EQ treble gain [?]        | -12 - +12 [dB]              | 40
+20     | 1    | 1C     - 3A     | EQ treble frequency       | 500 - 16.0k [Hz]            | 36
+21     | 1    | 00     - 5E     | ?                         | ?                           | 00
+22     | 1    | 40     - 48     | ?                         | ?                           | 40
+23     | 1    | 40     - 45     | ?                         | ?                           | 40
--------+------+-----------------+---------------------------+-----------------------------+---------
+24     | 2    | 0000   - FFFF   | SFX voice#                | 0 - 93, 65535:drum          | FFFF
+26     | 1    | 00     - 7F     | Playback rate             | -64 - +63 [semitone]        | 40
+27     | 1    | 80     - 7F     | Pitch fine                | -128 - +127 [cent]          | 00
+28     | 1    | 80     - 7F     | Pitch coarse              | -128 - +127 [semitone]      | 00
+29     | 1    | C5     - 34     | ?                         | ?                           | 00
+30     | 1    | Bit 6  - 7      | ?                         | ?                           | 00
        |      | Bit 0  - 5      | Loop fraction             | 0 - 63                      |
+31     | 3    | 000000 - FFFFFF | Attack length             | 0 - 16777215 [samples]      | 000000
+34     | 1    | Bit 7           | Reverse playback          | 0:normal, 1:reverse         | 00
        |      | Bit 0  - 6      | ?                         | ?                           |
+35     | 3    | 000000 - FFFFFF | Loop length               | 0 - 16777215 [samples]      | 000000
+38     | 1    | Bit 6  - 7      | Sample format             | 0:16, 1:12, 2:8-bit, 3:DPCM | 00
        |      | Bit 3  - 5      | DPCM scale                | 2^0 - 2^7                   |
        |      | Bit 1  - 2      | DPCM offset               | 0:-7, 1:-6, 2:-4, 3:0       |
        |      | Bit 0           | Loop point address        | Bit 24 (20 MB wave ROM)     |
+39     | 3    | 000000 - FFFFFF | Loop point address        | Bit 0 - 23                  | 000000

*/
void print_drum_voices(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Drum Voices (+%d)\n", ofs);

	static const char* const hdr  = "                                                                 Voice                                                                  |                                          Sample                                          \n"
	                                "Offset  | PC  | PF  | Lvl | Alt | Pan | Rev | Cho | Var | K | 0 | 1 | Cut | Q   | A   | D1  | D2  | ?  | LG  | LF | HG  | HF | ?        | SFX#  | Rat | PF   | PC   | ?     | Fr | Attack   | R | ?  | Loop     | F | D8 | Address ";
	static const char* const line = "--------+-----+-----+-----+-----+-----+-----+-----+-----+---+---+---+-----+-----+-----+-----+-----+----+-----+----+-----+----+----------+-------+-----+------+------+-------+----+----------+---+----+----------+---+----+---------";

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

		printf("%02X | ", ptr[16]);

		printf("%-3d | %-2d | ", ptr[17], ptr[18]);
		printf("%-3d | %-2d | ", ptr[19], ptr[20]);

		for (int j = 21; j < 24; ++j) printf("%02X ", ptr[j]);
		printf("| ");

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

		const int normal_voice_ofs = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
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
		"SFX",
		"MU Basic",
		"Model Exclusive",
		"MU100 Native",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		"TG300B"
	};

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	for (int i = 0; i < num; ++i)
	{
		printf("\nBank List %d", i);
		if (i < sizeof(name) / sizeof(name[0]) && name[i]) printf(" (%s)", name[i]);
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
		"Other Instrument 1",
		"Other Instrument 2",
		"Other Instrument 3",
		"Other Instrument 4",
		"Other Instrument 5",
		"Other Instrument 6",
		"Capital Voices MU100 Native",
		"Capital Voices MU Basic",
		"Silence",
		"SFX",
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
		"TG300B Bank 127"
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

			const int normal_voice_ofs = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
			printf("| %-+7d ", normal_voice_ofs);

			ptr += 4;
		}

		putchar('\n');
	}
}

/* Normal Voices

Offset  | Size | Data            | Parameter                 | Description                 | Default
--------+------+-----------------+---------------------------+-----------------------------+---------
+0      | 1    | 00     - 01     | Element switch            | 0:1, 1:1+2                  | 00
+1      | 1    | 00     - 7F     | Voice level               | y =~ (x/127)^2              |
+2      | 8    | 20     - 7F     | Voice name                | ASCII, space padded         |
+10     | 70   | See below       | Element 1                 | See below                   |
+80     | 70   | See below       | Element 2 (optional)      | See below                   |

Element 1/2

Offset  | Size | Data            | Parameter                 | Description                 | Default
--------+------+-----------------+---------------------------+-----------------------------+---------
+0      | 1    | 00     - 7F     | Wave# MSB                 | 0 - 442                     |
+1      | 1    | 00     - 7F     | Wave# LSB                 |                             |
+2      | 1    | 00     - 7F     | Note limit low            | 0 - 127 [note]              | 00
+3      | 1    | 00     - 7F     | Note limit high           | 0 - 127 [note]              | 7F
+4      | 1    | 01     - 7F     | Velocity limit low        | 1 - 127                     | 01
+5      | 1    | 01     - 7F     | Velocity limit high       | 1 - 127                     | 7F
--------+------+-----------------+---------------------------+-----------------------------+---------
+6      | 1    | Bit 6  - 7      | LFO wave                  | 0:saw, 1:tri, 2:s&h         | 5F
        |      | Bit 0  - 5      | LFO speed                 | 0 - 63                      |
+7      | 1    | Bit 7           | LFO phase init            | 0:off, 1:on                 | 00
        |      | Bit 0  - 6      | Vibrato delay time        | 0 - 127                     |
+8      | 1    | Bit 7           | Filter EG velocity curve  | 0:linear, 1:exponential     | 80
        |      | Bit 0  - 6      | Vibrato fade time         | 0 - 127                     |
+9      | 1    | Bit 6  - 7      | Pitch EG depth            | 0:0.5, 1:1, 2:2, 3:4 [oct]  | 40
        |      | Bit 0  - 5      | LFO pitch mod depth       | 0 - 63                      |
+10     | 1    | Bit 4  - 7      | PEG velocity level sens.  | -7 - +7                     | 70
        |      | Bit 0  - 3      | LFO filter mod depth      | 0 - 15                      |
+11     | 1    | Bit 5  - 7      | Pitch scaling depth       | 0 - 5: 0/5/10/20/50/100%    | 00
        |      | Bit 0  - 4      | LFO amp mod depth         | 0 - 31                      |
--------+------+-----------------+---------------------------+-----------------------------+---------
+12     | 1    | 20     - 60     | Note shift                | -32 - +32 [semitone]        | 40
+13     | 1    | 0E     - 72     | Detune                    | -50 - +50 [cent]            | 40
+14     | 1    | 00     - 7F     | Pitch scaling center note | 0 - 127 [note]              | 3C
+15     | 1    | Bit 4  - 7      | PEG velocity rate sens.   | -7 - +7                     | 77
        |      | Bit 0  - 3      | Pitch EG rate scaling     | -7 - +7                     |
+16     | 1    | 00     - 7F     | Pitch EG RS center note   | 0 - 127 [note]              | 3C
+17     | 1    | 00     - 3F     | Pitch EG attack rate      | 0 - 63                      | 3F
+18     | 1    | 00     - 3F     | Pitch EG decay 1 rate     | 0 - 63                      | 3F
+19     | 1    | 00     - 3F     | Pitch EG decay 2 rate     | 0 - 63                      | 3F
+20     | 1    | 00     - 3F     | Pitch EG release rate     | 0 - 63                      | 3F
+21     | 1    | 00     - 7F     | Pitch EG initial level    | -64 - +63                   | 40
+22     | 1    | 00     - 7F     | Pitch EG attack level     | -64 - +63                   | 40
+23     | 1    | 00     - 7F     | Pitch EG decay 1 level    | -64 - +63                   | 40
+24     | 1    | 00     - 7F     | Pitch EG decay 2 level    | -64 - +63                   | 40
+25     | 1    | 00     - 7F     | Pitch EG release level    | -64 - +63                   | 40
--------+------+-----------------+---------------------------+-----------------------------+---------
+26     | 1    | 00     - 3F     | Filter resonance          | 0 - 63                      | 00
+27     | 1    | 00     - 07     | Velocity sensitivity      | 0 - 7                       | 00
+28     | 1    | 00     - 7F     | Filter cutoff             | 0 - 127                     | 7F
+29     | 1    | 00     - 7F     | Cutoff scaling break pt 1 | 0 - 127 [note]              | 18
+30     | 1    | 00     - 7F     | Cutoff scaling break pt 2 | 0 - 127 [note]              | 30
+31     | 1    | 00     - 7F     | Cutoff scaling break pt 3 | 0 - 127 [note]              | 48
+32     | 1    | 00     - 7F     | Cutoff scaling break pt 4 | 0 - 127 [note]              | 60
+33     | 1    | 00     - 7F     | Cutoff scaling offset 1   | -64 - +63                   | 40
+34     | 1    | 00     - 7F     | Cutoff scaling offset 2   | -64 - +63                   | 40
+35     | 1    | 00     - 7F     | Cutoff scaling offset 3   | -64 - +63                   | 40
+36     | 1    | 00     - 7F     | Cutoff scaling offset 4   | -64 - +63                   | 40
+37     | 1    | Bit 4  - 7      | FEG velocity level sens.  | -7 - +7                     | 77
        |      | Bit 0  - 3      | FEG velocity rate sens.   | -7 - +7                     |
+38     | 1    | Bit 4  - 7      | Velocity curve            | 0 - 6                       | 07
        |      | Bit 0  - 3      | Filter EG rate scaling    | -7 - +7                     |
+39     | 1    | 00     - 7F     | Filter EG RS center note  | 0 - 127 [note]              | 3C
+40     | 1    | 00     - 3F     | Filter EG attack rate     | 0 - 63                      | 3F
+41     | 1    | 00     - 3F     | Filter EG decay 1 rate    | 0 - 63                      | 3F
+42     | 1    | 00     - 3F     | Filter EG decay 2 rate    | 0 - 63                      | 3F
+43     | 1    | 00     - 3F     | Filter EG release rate    | 0 - 63                      | 3F
+44     | 1    | 00     - 7F     | Filter EG initial level   | -64 - +63                   | 40
+45     | 1    | 00     - 7F     | Filter EG attack level    | -64 - +63                   | 40
+46     | 1    | 00     - 7F     | Filter EG decay 1 level   | -64 - +63                   | 40
+47     | 1    | 00     - 7F     | Filter EG decay 2 level   | -64 - +63                   | 40
+48     | 1    | 00     - 7F     | Filter EG release level   | -64 - +63                   | 40
--------+------+-----------------+---------------------------+-----------------------------+---------
+49     | 1    | 00     - 7F     | Element level             | 0 - 127, y =~ (x/127)^2     |
+50     | 1    | 00     - 7F     | Level scaling break pt 1  | 0 - 127 [note]              | 18
+51     | 1    | 00     - 7F     | Level scaling break pt 2  | 0 - 127 [note]              | 30
+52     | 1    | 00     - 7F     | Level scaling break pt 3  | 0 - 127 [note]              | 48
+53     | 1    | 00     - 7F     | Level scaling break pt 4  | 0 - 127 [note]              | 60
+54     | 1    | 00     - 7F     | Level scaling offset 1    | -64 - +63                   | 40
+55     | 1    | 00     - 7F     | Level scaling offset 2    | -64 - +63                   | 40
+56     | 1    | 00     - 7F     | Level scaling offset 3    | -64 - +63                   | 40
+57     | 1    | 00     - 7F     | Level scaling offset 4    | -64 - +63                   | 40
+58     | 1    | Bit 4  - 7      | Amp EG rate scaling       | -7 - +7                     | 77
        |      | Bit 0  - 3      | Pan                       | 0 - 14, 15:scaling          |
+59     | 1    | 00     - 7F     | Amp EG RS center note     | 0 - 127 [note]              | 3C
+60     | 1    | Bit 4  - 7      | Resonance sensitivity     | -7 - +7                     | 70
        |      | Bit 0  - 3      | Amp EG key on delay       | 0 - 15                      |
+61     | 1    | 00     - 3F     | Amp EG attack rate        | 0 - 63                      | 3F
+62     | 1    | 00     - 3F     | Amp EG decay 1 rate       | 0 - 63                      | 3F
+63     | 1    | 00     - 3F     | Amp EG decay 2 rate       | 0 - 63                      | 3F
+64     | 1    | 00     - 3F     | Amp EG release rate       | 0 - 63                      | 3F
+65     | 1    | 00     - 7F     | Amp EG decay 1 level      | -64 - +63                   | 40
+66     | 1    | 00     - 7F     | Amp EG decay 2 level      | -64 - +63                   | 40
+67     | 1    | 00     - 7F     | Address offset MSB        | Attack offset [samples]     | 00
+68     | 1    | 00     - 7F     | Address offset LSB        |                             | 00
+69     | 1    | 00     - 7F     | ?                         | ?                           | 00

*/
void print_normal_voices(const WDL_HeapBuf* const firmware, int ofs, const int num)
{
	printf("Normal Voices (+%d)\n", ofs);

	static const char* const hdr  = "            Voice            |            Element            |                          LFO                           |                                       Pitch                                       |                                                                  Filter                                                                   |                                                            Amplitude                                                            \n"
	                                "Offset  | E | Lvl | Name     | Wave# | N1  | N2  | V1  | V2  | W | Sp | I | Del | F | Fad | D | PM | VL | FM | S | AM | NS  | Det | Ns  | VR | RS | Nrs | R1 | R2 | R3 | R4 | L0  | L1  | L2  | L3  | L4  | Q  | VS | Cut | BP1 | BP2 | BP3 | BP4 | Of1 | Of2 | Of3 | Of4 | VL | VR | VC | RS | Nrs | R1 | R2 | R3 | R4 | L0  | L1  | L2  | L3  | L4  | Lvl | BP1 | BP2 | BP3 | BP4 | Of1 | Of2 | Of3 | Of4 | RS | P  | Nrs | QS | R0 | R1  | R2  | R3  | R4  | L2  | L3  | Addr   | ?  ";
	static const char* const line = "--------+---+-----+----------+-------+-----+-----+-----+-----+---+----+---+-----+---+-----+---+----+----+----+---+----+-----+-----+-----+----+----+-----+----+----+----+----+-----+-----+-----+-----+-----+----+----+-----+-----+-----+-----+-----+-----+-----+-----+-----+----+----+----+----+-----+----+----+----+----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+----+----+-----+----+----+-----+-----+-----+-----+-----+-----+--------+----";
	static const char* const skip = "        |   |     |          | ";

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
		const int el = ptr[0] + 1;
		printf("%d | %-3d | ", el, ptr[1]);

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

			// LFO
			printf("%d | %-2d | ", ptr[6] >> 6, ptr[6] & 0x3F);
			printf("%d | %-3d | ", ptr[7] >> 7, ptr[7] & 0x7F);
			printf("%d | %-3d | ", ptr[8] >> 7, ptr[8] & 0x7F);
			printf("%d | %-2d | ", ptr[9] >> 6, ptr[9] & 0x3F);
			printf("%-+2d | %-2d | ", (ptr[10] >> 4) - 7, ptr[10] & 0x0F);
			printf("%d | %-2d | ", ptr[11] >> 5, ptr[11] & 0x1F);

			// Pitch
			printf("%-+3d | %-+3d | ", ptr[12] - 64, ptr[13] - 64);
			printf("%-3d | %-+2d | ", ptr[14], (ptr[15] >> 4) - 7);
			printf("%-+2d | %-3d | ", (ptr[15] & 0x0F) - 7, ptr[16]);
			for (int l = 0; l < 4; ++l) printf("%-2d | ", ptr[17 + l]);
			for (int l = 0; l < 5; ++l) printf("%-+3d | ", ptr[21 + l] - 64);

			// Filter
			printf("%-2d | %-2d | %-3d | ", ptr[26], ptr[27], ptr[28]);
			for (int l = 0; l < 4; ++l) printf("%-3d | ", ptr[29 + l]);
			for (int l = 0; l < 4; ++l) printf("%-+3d | ", ptr[33 + l] - 64);
			printf("%-+2d | %-+2d | %-2d | ", (ptr[37] >> 4) - 7, (ptr[37] & 0x0F) - 7, ptr[38] >> 4);
			printf("%-+2d | %-3d | ", (ptr[38] & 0x0F) - 7, ptr[39]);
			for (int l = 0; l < 4; ++l) printf("%-2d | ", ptr[40 + l]);
			for (int l = 0; l < 5; ++l) printf("%-+3d | ", ptr[44 + l] - 64);

			// Amplitude
			printf("%-3d | ", ptr[49]);
			for (int l = 0; l < 4; ++l) printf("%-3d | ", ptr[50 + l]);
			for (int l = 0; l < 4; ++l) printf("%-+3d | ", ptr[54 + l] - 64);
			printf("%-+2d | %-2d | %-3d | ", (ptr[58] >> 4) - 7, ptr[58] & 0x0F, ptr[59]);
			printf("%-+2d | %-2d | ", (ptr[60] >> 4) - 7, ptr[60] & 0x0F);
			for (int l = 0; l < 4; ++l) printf("%-3d | ", ptr[61 + l]);
			for (int l = 0; l < 2; ++l) printf("%-+3d | ", ptr[65 + l] - 64);
			printf("%-+6d | %02X \n", (ptr[67] << 7) | ptr[68], ptr[69]);

			ptr += 70;
		}

		ofs += 10 + el * 70;
		if ((j += el) >= 25) j = 0;
	}
}

void print_sample_sets(const WDL_HeapBuf* const firmware, const int ofs, int num, const char* const title = NULL)
{
	printf("Sample Sets ");
	if (title) printf("%s ", title);
	printf("(+%d)\n\n", ofs);

	printf("Wave# "); for (int i = 0; i < 10; ++i) printf("| %-6d ", i);
	printf("\n------"); for (int i = 0; i < 10; ++i) printf("+--------");

	const unsigned char* ptr = (const unsigned char*)firmware->Get() + ofs;

	static int sample_set_idx = 0;
	int i = sample_set_idx;
	sample_set_idx += num;

	const int n = i % 10;
	int j = 0;

	for (i -= n; j < n; ++j)
	{
		if (!(j % 10)) printf("\n%-5d ", i);
		printf("|        ");
	}

	for (num += n; j < num; ++j)
	{
		if (!((i + j) % 10)) printf("\n%-5d ", i + j);

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
        |      | Bit 0           | Loop point address        | Bit 24 (20 MB wave ROM)     |
+13     | 3    | 000000 - FFFFFF | Loop point address        | Bit 0 - 23                  | 000000

*/
void print_samples(const WDL_HeapBuf* const firmware, int ofs, const int num, const char* const title = NULL)
{
	printf("Samples ");
	if (title) printf("%s ", title);
	printf("(+%d)\n", ofs);

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

int read_wavetbl(const char* const filenames[4], WDL_HeapBuf* const buf)
{
	unsigned char* ptr = (unsigned char*)buf->ResizeOK((20 + 8)*1024*1024);
	if (!ptr) return 0;

	unsigned char* const tmp = ptr + 20*1024*1024;
	int total = 0;

	for (int i = 0; i < 4; ++i)
	{
		FILE* const fp = fopen(filenames[i], "rb");
		if (!fp) return 0;

		const int n = (int)fread(tmp, 1, i < 2 ? 8*1024*1024 : 2*1024*1024, fp) & ~1;
		fclose(fp);

		for (int j = 0; j < n; j += 2)
		{
			memcpy(ptr, &tmp[j], 2);
			ptr += 4;
		}

		total += n;
		ptr -= !(i & 1) ? (n - 1) * 2 : 2;
	}

	buf->Resize(20*1024*1024);

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
				int j = n - i;
				j = !reverse ? i : j;

				const int step = base[j];

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
	const int attack = (ptr[0] << 16) | (ptr[1] << 8) | ptr[2];
	const int reverse = ptr[3] >> 7;
	const int loop = (ptr[4] << 16) | (ptr[5] << 8) | ptr[6];

	if (!(attack || loop)) return 0;

	const int format = ptr[7] >> 6, dpcm = (ptr[7] >> 1) & 0x1F;
	const int addr = (((ptr[7] & 0x01) << 24) | (ptr[8] << 16) | (ptr[9] << 8) | ptr[10]) << 2;

	#ifndef MUTABLE_EXTRACT_DUPLICATES

	static const int max_samples = 1925;
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
		// IC102 XV561D0 FLASH ROM 8M v1.066
		"sw1000xg/1.06.06_xv561d0.ic102", // SHA1(dff42c0f0ad0f0ec9587cd2a53ba2b3836b1d587)

		// IC122 XV389A0 WAVE ROM 64M
		"sw1000xg/xv389a0.ic122", // SHA1(72ec39e03e4e5ddf6137faa798c0ec3c23905855)
		// IC121 XV390A0 WAVE ROM 64M
		"sw1000xg/xv390a0.ic121", // SHA1(1903ed7f1292e6117a448fec19e2b712f3815964)

		// IC124 XT445A0 ROM 1 16M
		"sw1000xg/xt445a0-828.ic124", // SHA1(23b5e046fd2e2ac01af3e6dc6357c5c6547b286b)
		// IC123 XT461A0 ROM 2 16M
		"sw1000xg/xt461a0-829.ic123"  // SHA1(46a7a7225cd7e1818ba551325d2af5ac1bf5b2bf)
	};

	WDL_HeapBuf firmware;

	if (!read_firmware(roms[0], &firmware, 1*1024*1024))
	{
		return EXIT_FAILURE;
	}

	if (opt == '-f')
	{
		const int size = write_firmware("build/sw1000xg_firmware.bin", &firmware);
		return size == firmware.GetSize() ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (opt == '-m')
	{
		static const char* const filename[] =
		{
			"mu100_demo",
			"mu100r_demo"
		};

		static const int ofs[][2] =
		{
			{ +919608, +1019028 },
			{ +969312, +1033781 }
		};

		static const int m = sizeof(ofs) / sizeof(ofs[0]);
		int n = 0;

		for (int i = 0; i < m; ++i)
		{
			for (int j = 0; j < 2; ++j)
			{
				char fn[128];
				sprintf(fn, "midi/sw1000xg_%s_%c.mid", filename[i], 'a' + j);

				const int size = write_midi(fn, &firmware, ofs[i][j]);
				n += size > 0;
			}
		}

		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	if (opt == '-t')
	{
		printf("SW1000XG Data Tables\n\n");

		print_drum_banks(&firmware, +606632, 4); puts("\n--\n");
		print_drum_kits(&firmware, +594600, 47); puts("\n--\n");
		print_drum_voices(&firmware, +555792, 924); puts("\n--\n");
		print_sfx_voices(&firmware, +700584, 94); puts("\n--\n");

		print_bank_lists(&firmware, +699304, 10); puts("\n--\n");
		print_program_banks(&firmware, +607144, 179); puts("\n--\n");
		print_normal_voices(&firmware, +700960, 1455); puts("\n--\n");

		print_sample_sets(&firmware, +903150, 294); puts("\n--\n");
		print_samples(&firmware, +877630, 1595); puts("\n--\n");

		print_sample_sets(&firmware, +907920, 34); puts("\n--\n");
		print_samples(&firmware, +903744, 261); puts("\n--\n");

		print_sample_sets(&firmware, +919364, 116); puts("\n--\n");
		print_samples(&firmware, +907988, 711);

		return EXIT_SUCCESS;
	}

	WDL_HeapBuf wavetbl;

	if (!read_wavetbl(&roms[1], &wavetbl))
	{
		return EXIT_FAILURE;
	}

	if (opt == '-w')
	{
		int n = extract_drum_samples("wave/sw1000xg/drum_%05d.wav", &firmware, +555792, 924, &wavetbl);

		n += extract_samples("wave/sw1000xg/sample1_%05d.wav", &firmware, +877630, 1595, &wavetbl);
		n += extract_samples("wave/sw1000xg/sample2_%05d.wav", &firmware, +903744, 261, &wavetbl);
		n += extract_samples("wave/sw1000xg/sample3_%05d.wav", &firmware, +907988, 711, &wavetbl);

		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	printf("Usage: %s -f | -m | -t | -w\n", argv[0]);
	return EXIT_FAILURE;
}
