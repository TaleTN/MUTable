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

MU10 = mu10 mu10/xs289a0.ic07

mu10 : build build/mu10.exe

mu10-firmware : $(MU10)
	build\mu10.exe -f

mu10-table : $(MU10)
	build\mu10.exe -t > table\mu10.txt

mu10-wave : $(MU10) mu10/xr709a0.ic11
!IF !EXIST("wave/mu10/")
	@mkdir wave\mu10
!ELSE
	@del wave\mu10\*.wav
!ENDIF
	build\mu10.exe -w

MU15 = mu15 mu15/xv684c0.bin

mu15 : build build/mu15.exe

mu15-firmware : $(MU15)
	build\mu15.exe -f

mu15-midi : $(MU15)
	build\mu15.exe -m

mu15-table : $(MU15)
	build\mu15.exe -t > table\mu15.txt

mu15-bitmap : $(MU15)
	build\mu15.exe -b

mu15-wave : $(MU15)
!IF !EXIST("wave/mu15/")
	@mkdir wave\mu15
!ELSE
	@del wave\mu15\*.wav
!ENDIF
	build\mu15.exe -w

MU50 = mu50 mu50/xr174c0.ic7

mu50 : build build/mu50.exe

mu50-firmware : $(MU50)
	build\mu50.exe -f

mu50-midi : $(MU50)
	build\mu50.exe -m

mu50-table : $(MU50)
	build\mu50.exe -t > table\mu50.txt

mu50-bitmap : $(MU50)
	build\mu50.exe -b

mu50-wave : $(MU50) mu50/xq057c0.ic18 mu50/xq058c0.ic19
!IF !EXIST("wave/mu50/")
	@mkdir wave\mu50
!ELSE
	@del wave\mu50\*.wav
!ENDIF
	build\mu50.exe -w

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
