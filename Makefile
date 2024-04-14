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

MU80 = mu80 mu80/xq556a0.ic8

mu80 : build build/mu80.exe

mu80-firmware : $(MU80)
	build\mu80.exe -f

mu80-midi : $(MU80)
	build\mu80.exe -m

mu80-table : $(MU80)
	build\mu80.exe -t > table\mu80.txt

mu80-bitmap : $(MU80)
	build\mu80.exe -b

mu80-wave : $(MU80) mu80/xq012b0-822.bin mu80/xq013b0-823.bin mu80/xq089b0-824.bin mu80/xq090b0-825.bin
!IF !EXIST("wave/mu80/")
	@mkdir wave\mu80
!ELSE
	@del wave\mu80\*.wav
!ENDIF
	build\mu80.exe -w

MU90 = mu90 mu90/xs519d0.ic9

mu90 : build build/mu90.exe

mu90-firmware : $(MU90)
	build\mu90.exe -f

mu90-midi : $(MU90)
	build\mu90.exe -m

mu90-table : $(MU90)
	build\mu90.exe -t > table\mu90.txt

mu90-bitmap : $(MU90)
	build\mu90.exe -b

mu90-wave : $(MU90) mu90/xs518a0.ic22 mu90/xs743a0.ic23
!IF !EXIST("wave/mu90/")
	@mkdir wave\mu90
!ELSE
	@del wave\mu90\*.wav
!ENDIF
	build\mu90.exe -w

MU100 = mu100 mu100/xu50720.ic11

mu100 : build build/mu100.exe

mu100-firmware : $(MU100)
	build\mu100.exe -f

mu100-midi : $(MU100)
	build\mu100.exe -m

mu100-table : $(MU100)
	build\mu100.exe -t > table\mu100.txt

mu100-bitmap : $(MU100)
	build\mu100.exe -b

mu100-wave : $(MU100) mu100/xs518b0.ic34 mu100/xs743b0.ic35 mu100/xt445a0-828.ic36 mu100/xt461a0-829.ic37 mu100/xt462a0.ic39 mu100/xt463a0.ic38
!IF !EXIST("wave/mu100/")
	@mkdir wave\mu100
!ELSE
	@del wave\mu100\*.wav
!ENDIF
	build\mu100.exe -w

MU128 = mu128 mu128/mu128-v2.00-h.bin mu128/mu128-v2.00-l.bin

mu128 : build build/mu128.exe

mu128-firmware : $(MU128)
	build\mu128.exe -f

mu128-midi : $(MU128)
	build\mu128.exe -m

mu128-table : $(MU128)
	build\mu128.exe -t > table\mu128.txt

mu128-bitmap : $(MU128)
	build\mu128.exe -b

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
