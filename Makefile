# Copyright (C) 2024 Theo Niessink <theo@taletn.com>
# This work is free. You can redistribute it and/or modify it under the
# terms of the Do What The Fuck You Want To Public License, Version 2,
# as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.

CPPFLAGS = /O2 /D _CRT_SECURE_NO_WARNINGS /W3 /nologo

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
