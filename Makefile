# Copyright (C) 2024 Theo Niessink <theo@taletn.com>
# This work is free. You can redistribute it and/or modify it under the
# terms of the Do What The Fuck You Want To Public License, Version 2,
# as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.

default :
	@echo Usage: nmake ^<target^> [dupl=1]

CPPFLAGS = /O2 /D WDL_NO_SUPPORT_UTF8 /D _CRT_SECURE_NO_WARNINGS /W3 /nologo

!IF DEFINED(DUPL) && "$(DUPL)" != "0"
CPPFLAGS = $(CPPFLAGS) /D MUTABLE_EXTRACT_DUPLICATES
!ENDIF

MU5 = mu5 mu5/yamaha_mu5_program_xq201a0.bin

mu5 : build build/mu5.exe

mu5-midi : $(MU5)
	build\mu5.exe -m

mu5-bitmap : $(MU5)
	build\mu5.exe -b

mu5-table : $(MU5) mu5/yamaha_mu5_waverom_xp50280-801.bin
	build\mu5.exe -t > table\mu5.txt

mu5-wave : $(MU5) mu5/yamaha_mu5_waverom_xp50280-801.bin
!IF !EXIST("wave/mu5/")
	@mkdir wave\mu5
!ELSE
	@del wave\mu5\*.wav
!ENDIF
	build\mu5.exe -w

build :
!IF !EXIST("build/")
	@mkdir build
!ENDIF

.cpp{build}.exe :
	$(CPP) $(CPPFLAGS) /Fo"build/" /Fe"build/" $<

clean :
!IF EXIST("build/")
	rmdir /s /q build
!ENDIF
