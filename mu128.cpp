// Copyright (C) 2019-2024 Theo Niessink <theo@taletn.com>
// This work is free. You can redistribute it and/or modify it under the
// terms of the Do What The Fuck You Want To Public License, Version 2,
// as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WDL/heapbuf.h"
#include "WDL/wdlendian.h"

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

int read_var_len(FILE* const fp)
{
	int len = 0;

	for (int i = 0;;)
	{
		const int c = fgetc(fp);
		if (c == EOF) return EOF;

		if (c & 0x80) len <<= 7;
		len |= c & 0x7F;

		if (!(c & 0x80)) break;
		if (++i >= 4) return EOF;
	}

	return len;
}

int read_upgrade(const char* const filename, WDL_HeapBuf* const buf, const int size)
{
	unsigned char* const ptr = (unsigned char*)buf->ResizeOK(size);
	if (!ptr) return 0;

	memset(ptr, 0xFF, size);

	FILE* const fp = fopen(filename, "rb");
	if (!fp) return 0;

	fseek(fp, 14 + 8, SEEK_SET);

	for (int addr = 0;;)
	{
		/* const int delta = */ read_var_len(fp);
		const int status = fgetc(fp);

		// Meta event
		if (status == 0xFF)
		{
			const int type = fgetc(fp);
			const int len = read_var_len(fp);

			fseek(fp, len, SEEK_CUR);

			// End of Track
			if (type == 0x2F) break;

			continue;
		}

		// SysEx
		if (status == 0xF0 || status == 0xF7)
		{
			int len = read_var_len(fp);

			if (status == 0xF0 && len >= 8)
			{
				len -= 8;

				unsigned char tmp[8];
				fread(tmp, 1, 8, fp);

				// F0 43 10 59 01 aa aa aa aa F7
				if (tmp[0] == 0x43 && tmp[1] == 0x10 && tmp[2] == 0x59 && tmp[3] == 0x01)
				{
					addr = (((((tmp[4] << 7) | tmp[5]) << 7) | tmp[6]) << 7) | tmp[7];
				}

				// F0 43 00 59 nn nn oo oo oo ... F7
				else if (tmp[0] == 0x43 && tmp[1] == 0x00 && tmp[2] == 0x59)
				{
					int n = (tmp[3] << 7) | tmp[4];
					const int ofs = (((tmp[5] << 7) | tmp[6]) << 7) | tmp[7];

					assert((n % 8) == 0);
					int m = ((n >> 3) * 7) & ~7;

					for (int i = addr + ofs; n > 0;)
					{
						fread(tmp, 1, 8, fp);

						for (int j = 0; j < 7 && m > 0; --m, ++j)
						{
							assert(i < buf->GetSize());
							ptr[i++] = tmp[j] | ((tmp[7] << (j + 1)) & 0x80);
						}

						len -= 8;
						n -= 8;
					}
				}
			}

			fseek(fp, len, SEEK_CUR);
			continue;
		}

		assert(false);
		break;
	}

	fclose(fp);

	return size;
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
		'M', 'T', 'r', 'k', 0x00, 0x00, 0x00, 0x07, 0x00, 0xFF, 0x51, 0x03, 0x07, 0x53, 0x00 // 125 BPM
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
+30     | 1    | 00     - 6C     | ?                         | ?                           | 00
+31     | 3    | 000000 - FFFFFF | Attack length             | 0 - 16777215 [samples]      | 000000
+34     | 1    | 00     - 80     | ?                         | ?                           | 00
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

	static const char* const hdr  = "                                                                  Voice                                                                  |                                     Sample                                      \n"
	                                "Offset  | ?  | PF  | Lvl | Alt | Pan | Rev | Cho | Var | K | 0 | 1 | Cut | Q   | A   | D1  | D2  | LG  | HG  | LF | HF  | ?        | PC  | SFX#  | Rat | PF   | PC   | ?     | Attack   | ?  | Loop     | F | D8 | Address ";
	static const char* const line = "--------+----+-----+-----+-----+-----+-----+-----+-----+---+---+---+-----+-----+-----+-----+-----+-----+-----+----+-----+----------+-----+-------+-----+------+------+-------+----------+----+----------+---+----+---------";

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

		for (int j = 29; j < 31; ++j) printf("%02X ", ptr[j]);
		printf("| ");

		const int attack = (ptr[31] << 16) | (ptr[32] << 8) | ptr[33];
		const int loop = (ptr[35] << 16) | (ptr[36] << 8) | ptr[37];
		const int format = ptr[38] >> 6, dpcm = (ptr[38] >> 1) & 0x1F;
		const int addr = (((ptr[38] & 0x01) << 24) | (ptr[39] << 16) | (ptr[40] << 8) | ptr[41]) << 2;

		printf("%-8d | ", attack);
		printf("%02X | %-8d | ", ptr[34], loop);
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
+0      | 1    | 01     - 03     | Element switch            | 1:1, 3:1+2                  | 01
+1      | 1    | 00     - 7F     | Voice level               | y =~ (x/127)^2              |
+2      | 10   | 20     - 7F     | Voice name                | ASCII, space padded         |
+12     | 1    | 00              | ?                         | ?                           | 00
+13     | 1    | 7F              | ?                         | ?                           | 7F
+14     | 84   | See below       | Element 1                 | See below                   |
+98     | 84   | See below       | Element 2 (optional)      | See below                   |

Element 1/2

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
+36     | 1    | 00     - 7F     | Cutoff scaling break pt 1 | 0 - 127 [note]              | 18
+37     | 1    | 00     - 7F     | Cutoff scaling break pt 2 | 0 - 127 [note]              | 30
+38     | 1    | 00     - 7F     | Cutoff scaling break pt 3 | 0 - 127 [note]              | 48
+39     | 1    | 00     - 7F     | Cutoff scaling break pt 4 | 0 - 127 [note]              | 60
+40     | 1    | 00     - 7F     | Cutoff scaling offset 1   | -64 - +63                   | 40
+41     | 1    | 00     - 7F     | Cutoff scaling offset 2   | -64 - +63                   | 40
+42     | 1    | 00     - 7F     | Cutoff scaling offset 3   | -64 - +63                   | 40
+43     | 1    | 00     - 7F     | Cutoff scaling offset 4   | -64 - +63                   | 40
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
+58     | 1    | 00     - 7F     | Level scaling break pt 1  | 0 - 127 [note]              | 18
+59     | 1    | 00     - 7F     | Level scaling break pt 2  | 0 - 127 [note]              | 30
+60     | 1    | 00     - 7F     | Level scaling break pt 3  | 0 - 127 [note]              | 48
+61     | 1    | 00     - 7F     | Level scaling break pt 4  | 0 - 127 [note]              | 60
+62     | 1    | 00     - 7F     | Level scaling offset 1    | -64 - +63                   | 40
+63     | 1    | 00     - 7F     | Level scaling offset 2    | -64 - +63                   | 40
+64     | 1    | 00     - 7F     | Level scaling offset 3    | -64 - +63                   | 40
+65     | 1    | 00     - 7F     | Level scaling offset 4    | -64 - +63                   | 40
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

	static const char* const hdr  = "                 Voice                 |            Element            |                    LFO                    |                                               Pitch                                               |                                                                 Filter                                                                  |                                                                   Amplitude                                                                    \n"
	                                "Offset  | E | Lvl | Name       | ?     | Wave# | N1  | N2  | V1  | V2  | I | W | F | Sp | Del | Fad | PM | FM | AM | NS  | Det | S | Ns  | D | VL  | VR  | RS  | Nrs | R1 | R2 | R3 | R4 | L0  | L1  | L2  | L3  | L4  | Q  | VS | Cut | BP1 | BP2 | BP3 | BP4 | Of1 | Of2 | Of3 | Of4 | VL  | VR  | RS  | Nrs | R1 | R2 | R3 | R4 | L0  | L1  | L2  | L3  | L4  | Lvl | BP1 | BP2 | BP3 | BP4 | Of1 | Of2 | Of3 | Of4 | VC | P  | RS  | Nrs | R0 | R1  | R2  | R3  | R4  | L2  | L3  | Addr   | QS  | ?           ";
	static const char* const line = "--------+---+-----+------------+-------+-------+-----+-----+-----+-----+---+---+---+----+-----+-----+----+----+----+-----+-----+---+-----+---+-----+-----+-----+-----+----+----+----+----+-----+-----+-----+-----+-----+----+----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+----+----+----+----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+----+----+-----+-----+----+-----+-----+-----+-----+-----+-----+--------+-----+-------------";
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
			for (int l = 0; l < 4; ++l) printf("%-3d | ", ptr[36 + l]);
			for (int l = 0; l < 4; ++l) printf("%-+3d | ", ptr[40 + l] - 64);
			printf("%-+3d | %-+3d | ", ptr[44] - 64, ptr[45] - 64);
			printf("%-+3d | %-3d | ", ptr[46] - 64, ptr[47]);
			for (int l = 0; l < 4; ++l) printf("%-2d | ", ptr[48 + l]);
			for (int l = 0; l < 5; ++l) printf("%-+3d | ", ptr[52 + l] - 64);

			// Amplitude
			printf("%-3d | ", ptr[57]);
			for (int l = 0; l < 4; ++l) printf("%-3d | ", ptr[58 + l]);
			for (int l = 0; l < 4; ++l) printf("%-+3d | ", ptr[62 + l] - 64);
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
+4      | 1    | 00     - 6C     | ?                         | ?                           | 00
+5      | 3    | 000000 - FFFFFF | Attack length             | 0 - 16777215 [samples]      | 000000
+8      | 1    | 80     - 7F     | ?                         | ?                           | 00
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

	static const char* const hdr  = "Offset  | Atn | PC   | PF   | N   | ?  | Attack   | ?  | Loop     | F | D8 | Address ";
	static const char* const line = "--------+-----+------+------+-----+----+----------+----+----------+---+----+---------";

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
		printf("%-3d | ", ptr[3]);

		const int attack = (ptr[5] << 16) | (ptr[6] << 8) | ptr[7];
		const int loop = (ptr[9] << 16) | (ptr[10] << 8) | ptr[11];
		const int format = ptr[12] >> 6, dpcm = (ptr[12] >> 1) & 0x1F;
		const int addr = (((ptr[12] & 0x01) << 24) | (ptr[13] << 16) | (ptr[14] << 8) | ptr[15]) << 2;

		printf("%02X | %-8d | ", ptr[4], attack);
		printf("%02X | %-8d | ", ptr[8], loop);
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

int main(const int argc, const char* const* const argv)
{
	int opt = argc == 2 ? argv[1][0] : 0;
	if (opt == '-') opt = argv[1][1] | ('-' << 8);

	static const char* const roms[] =
	{
		#ifdef MU128_FIRMWARE_V1_06
		// IC27 XV217C0 FLASH ROM v1.06
		"mu128/xv217c0.ic27", // SHA1(381e1d146c4e693a21f6e6e4ea2a8b9f6e3033ef)
		// IC25 XV224C0 FLASH ROM v1.06
		"mu128/xv224c0.ic25"  // SHA1(56d69f3214899fa25ef5e9ea6c2bbf0c3d378123)
		#elif defined(MU128_UPGRADE_V2_00)
		// Upgrade to v2.00
		"mu128/V200Q.ydl" // SHA1(9ba008f9343cd1e4a9b255ae393d84517ed3d893)
		#else // MU128_FIRMWARE_V2_00
		// IC27 FLASH ROM v2.00
		"mu128/mu128-v2.00-h.bin", // SHA1(8ed4a6929c66fcb5248e16288dfaf56a3286aaf8)
		// IC25 FLASH ROM v2.00
		"mu128/mu128-v2.00-l.bin"  // SHA1(e1ff3387968e89f5bc5df3e15cd0d6039104acd0)
		#endif
	};

	WDL_HeapBuf firmware;

	#ifdef MU128_UPGRADE_V2_00
	if (!read_upgrade(roms[0], &firmware, 2*1024*1024))
	#else // MU128_FIRMWARE_*
	if (!read_firmware(&roms[0], &firmware, 2, 1*1024*1024))
	#endif
	{
		return EXIT_FAILURE;
	}

	if (opt == '-f')
	{
		const int size = write_firmware("build/mu128_firmware.bin", &firmware);
		return size == firmware.GetSize() ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	if (opt == '-m')
	{
		static const int ofs[] =
		{
			+705820,
			+776443,
			+816732,
			+827255,

			// TN: Alternate demo showing names of design staff. Easter egg?
			// Unfortunately I don't know how to trigger it though...
			+842364,
			+886390
		};

		#ifdef MU128_FIRMWARE_V1_06
		static const int rebase = -47224;
		#else // MU128_FIRMWARE_V2_00
		static const int rebase = 0;
		#endif

		static const int m = sizeof(ofs) / sizeof(ofs[0]);
		int n = 0;

		for (int i = 0; i < m; ++i)
		{
			char fn[128];
			sprintf(fn, "midi/mu128_demo_%c", (i < 4 ? 'a' : 'c' - 4) + i);
			int len = (int)strlen(fn);
			if (i >= 2) fn[len++] = '0' + (i >> 1);
			strcpy(&fn[len], ".mid");

			const int size = write_midi(fn, &firmware, ofs[i] + rebase);
			n += size > 0;
		}

		if (!n) return EXIT_FAILURE;

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	if (opt == '-t')
	{
		static const int ofs_num[][2] =
		{
			#ifdef MU128_FIRMWARE_V1_06
			{ +1474876, 4    },
			{ +1462396, 48   },
			{ +1422916, 940  },
			{ +1422412, 94   },
			{ +1421900, 4    },
			{ +1422788, 1    },
			{ +1322060, 195  },
			{ +1088496, 1581 },
			{ +1087572, 461  },
			{ +1044132, 2715 }
			#else // MU128_FIRMWARE_V2_00
			{ +1483552, 6    },
			{ +1468470, 58   },
			{ +1428276, 957  },
			{ +1427636, 94   },
			{ +1427124, 4    },
			{ +1428148, 1    },
			{ +1317044, 215  },
			{ +1083480, 1581 },
			{ +1082556, 461  },
			{ +1039116, 2715 }
			#endif
		};

		const int* const ofs = ofs_num[0];
		const int* const num = &ofs[1];

		printf("MU128 Data Tables\n\n");

		print_drum_banks(&firmware, ofs[0], num[0]); puts("\n--\n");
		print_drum_kits(&firmware, ofs[2], num[2]); puts("\n--\n");
		print_drum_voices(&firmware, ofs[4], num[4]); puts("\n--\n");
		print_sfx_voices(&firmware, ofs[6], num[6]); puts("\n--\n");

		print_bank_lists(&firmware, ofs[8], num[8], "MU128"); puts("\n--\n");
		print_bank_lists(&firmware, ofs[10], num[10], "TG300B"); puts("\n--\n");

		#ifndef MU128_FIRMWARE_V1_06
		print_bank_lists(&firmware, +1484320, 2, "GM"); puts("\n--\n");
		#endif

		print_program_banks(&firmware, ofs[12], num[12]); puts("\n--\n");
		print_normal_voices(&firmware, ofs[14], num[14]); puts("\n--\n");

		print_sample_sets(&firmware, ofs[16], num[16]); puts("\n--\n");
		print_samples(&firmware, ofs[18], num[18]);

		return EXIT_SUCCESS;
	}

	if (opt == '-b')
	{
		#ifdef MU128_FIRMWARE_V1_06
		static const int ofs = +898124, num = 705;
		#else // MU128_FIRMWARE_V2_00
		static const int ofs = +906324, num = 541;
		#endif

		FILE* const fp = fopen("table/mu128_bitmap.txt", "wb");
		if (!fp) return EXIT_FAILURE;

		static const unsigned short bom = 0xFEFF;
		fwrite(&bom, 2, 1, fp);

		write_utf16_str("MU128 Bitmaps\n\n", fp);
		const int n = print_bitmaps(fp, &firmware, ofs, num);

		printf("%d\n", n);
		return EXIT_SUCCESS;
	}

	printf("Usage: %s -f | -m | -t | -b\n", argv[0]);
	return EXIT_FAILURE;
}
