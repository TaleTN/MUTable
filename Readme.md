# MUTable

Reverse engineering documentation and tools for the Yamaha MU
series and related legacy MIDI sound modules. Using this you can extract
data tables, MIDI files, icon bitmaps, and WAV files from the original
firmware and wave ROMs (which are **not** included in this project). The
source code also contains documentation on the data structures and sample
formats used.

## Requires WDL "Tale" Edition

This project requires some header files from the WDL "Tale" Edition library,
which you can download from:

https://github.com/TaleTN/WDL

Extract it so you have (at least) the following files from this project:

```
GNUmakefile
Makefile
*.cpp
```

And then these files from WDL:

```
WDL/heapbuf.h
WDL/pcmfmtcvt.h
WDL/wavread.h
WDL/wavwrite.h
WDL/wdlendian.h
WDL/wdlstring.h
WDL/wdltypes.h
WDL/win32_utf8.h
```

## How to build & run

To extract data tables from the MU50 firmware simply run:

```
make mu50-table
```

Or when using the Microsoft C/C++ compiler and tools:

```
nmake mu50-table
```

See [`GNUmakefile`](GNUmakefile) or [`Makefile`](Makefile) for all supported
sound modules and options.

## License

Copyright &copy; 2019-2025 Theo Niessink &lt;theo@taletn.com&gt;  
This work is free. You can redistribute it and/or modify it under the
terms of the Do What The Fuck You Want To Public License, Version 2,
as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.
