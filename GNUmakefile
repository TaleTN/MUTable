# Copyright (C) 2024, 2025 Theo Niessink <theo@taletn.com>
# This work is free. You can redistribute it and/or modify it under the
# terms of the Do What The Fuck You Want To Public License, Version 2,
# as published by Sam Hocevar. See http://www.wtfpl.net/ for more details.

.PHONY : default
default :
	@echo "Usage: make <target> [dupl=1]"

CXXFLAGS = -O2 -Wall -Wno-multichar

ifdef dupl
	ifneq ($(dupl),0)
CXXFLAGS += -D MUTABLE_EXTRACT_DUPLICATES
	endif
endif

DB50XG = db50xg db50xg/xr253a0.ic7

.PHONY : db50xg
db50xg : build build/db50xg

db50xg-firmware : $(DB50XG)
	build/db50xg -f

db50xg-table : $(DB50XG)
	build/db50xg -t > table/db50xg.txt

db50xg-wave : $(DB50XG) db50xg/xq730b0.ic9 db50xg/xq731b0.ic10
	@mkdir -p wave/db50xg
	@rm -f wave/db50xg/*.wav
	build/db50xg -w

DB60XG = db60xg db60xg/xr560b0.ic7

.PHONY : db60xg
db60xg : build build/db60xg

db60xg-firmware : $(DB60XG)
	build/db60xg -f

db60xg-table : $(DB60XG)
	build/db60xg -t > table/db60xg.txt

db60xg-wave : $(DB60XG) db60xg/xq730b0.ic9 db60xg/xq731b0.ic10
	@mkdir -p wave/db60xg
	@rm -f wave/db60xg/*.wav
	build/db60xg -w

MU5 = mu5 mu5/yamaha_mu5_program_xq201a0.bin

.PHONY : mu5
mu5 : build build/mu5

mu5-midi : $(MU5)
	build/mu5 -m

mu5-bitmap : $(MU5)
	build/mu5 -b

mu5-table : $(MU5) mu5/yamaha_mu5_waverom_xp50280-801.bin
	build/mu5 -t > table/mu5.txt

mu5-wave : $(MU5) mu5/yamaha_mu5_waverom_xp50280-801.bin
	@mkdir -p wave/mu5
	@rm -f wave/mu5/*.wav
	build/mu5 -w

MU10 = mu10 mu10/xs289a0.ic07

.PHONY : mu10
mu10 : build build/mu10

mu10-firmware : $(MU10)
	build/mu10 -f

mu10-table : $(MU10)
	build/mu10 -t > table/mu10.txt

mu10-wave : $(MU10) mu10/xr709a0.ic11
	@mkdir -p wave/mu10
	@rm -f wave/mu10/*.wav
	build/mu10 -w

MU15 = mu15 mu15/xv684c0.bin

.PHONY : mu15
mu15 : build build/mu15

mu15-firmware : $(MU15)
	build/mu15 -f

mu15-midi : $(MU15)
	build/mu15 -m

mu15-table : $(MU15)
	build/mu15 -t > table/mu15.txt

mu15-bitmap : $(MU15)
	build/mu15 -b

mu15-wave : $(MU15)
	@mkdir -p wave/mu15
	@rm -f wave/mu15/*.wav
	build/mu15 -w

MU50 = mu50 mu50/xr174c0.ic7

.PHONY : mu50
mu50 : build build/mu50

mu50-firmware : $(MU50)
	build/mu50 -f

mu50-midi : $(MU50)
	build/mu50 -m

mu50-table : $(MU50)
	build/mu50 -t > table/mu50.txt

mu50-bitmap : $(MU50)
	build/mu50 -b

mu50-wave : $(MU50) mu50/xq057c0.ic18 mu50/xq058c0.ic19
	@mkdir -p wave/mu50
	@rm -f wave/mu50/*.wav
	build/mu50 -w

MU80 = mu80 mu80/xq556a0.ic8

.PHONY : mu80
mu80 : build build/mu80

mu80-firmware : $(MU80)
	build/mu80 -f

mu80-midi : $(MU80)
	build/mu80 -m

mu80-table : $(MU80)
	build/mu80 -t > table/mu80.txt

mu80-bitmap : $(MU80)
	build/mu80 -b

mu80-wave : $(MU80) mu80/xq012b0-822.bin mu80/xq013b0-823.bin mu80/xq089b0-824.bin mu80/xq090b0-825.bin
	@mkdir -p wave/mu80
	@rm -f wave/mu80/*.wav
	build/mu80 -w

MU90 = mu90 mu90/xs519d0.ic9

.PHONY : mu90
mu90 : build build/mu90

mu90-firmware : $(MU90)
	build/mu90 -f

mu90-midi : $(MU90)
	build/mu90 -m

mu90-table : $(MU90)
	build/mu90 -t > table/mu90.txt

mu90-bitmap : $(MU90)
	build/mu90 -b

mu90-wave : $(MU90) mu90/xs518a0.ic22 mu90/xs743a0.ic23
	@mkdir -p wave/mu90
	@rm -f wave/mu90/*.wav
	build/mu90 -w

MU100 = mu100 mu100/xu50720.ic11

.PHONY : mu100
mu100 : build build/mu100

mu100-firmware : $(MU100)
	build/mu100 -f

mu100-midi : $(MU100)
	build/mu100 -m

mu100-table : $(MU100)
	build/mu100 -t > table/mu100.txt

mu100-bitmap : $(MU100)
	build/mu100 -b

mu100-wave : $(MU100) mu100/xs518b0.ic34 mu100/xs743b0.ic35 mu100/xt445a0-828.ic36 mu100/xt461a0-829.ic37 mu100/xt462a0.ic39 mu100/xt463a0.ic38
	@mkdir -p wave/mu100
	@rm -f wave/mu100/*.wav
	build/mu100 -w

MU128 = mu128 mu128/mu128-v2.00-h.bin mu128/mu128-v2.00-l.bin

.PHONY : mu128
mu128 : build build/mu128

mu128-firmware : $(MU128)
	build/mu128 -f

mu128-midi : $(MU128)
	build/mu128 -m

mu128-table : $(MU128)
	build/mu128 -t > table/mu128.txt

mu128-bitmap : $(MU128)
	build/mu128 -b

mu128-wave : $(MU128) mu128/xv364a0.ic53 mu128/xv365a0.ic54 mu128/xv366a0.ic57 mu128/xv376a0.ic58
	@mkdir -p wave/mu128
	@rm -f wave/mu128/*.wav
	build/mu128 -w

MU1000 = mu1000 mu1000/mu1000-v2.01-h.bin mu1000/mu1000-v2.01-l.bin

.PHONY : mu1000
mu1000 : build build/mu1000

mu1000-firmware : $(MU1000)
	build/mu1000 -f

mu1000-table : $(MU1000)
	build/mu1000 -t > table/mu1000.txt

mu1000-bitmap : $(MU1000)
	build/mu1000 -b

mu1000-wave : $(MU1000) mu2000/xv364a0.ic49 mu2000/xv365a0.ic50 mu2000/xw848a0.ic53 mu2000/xw849a0.ic54
	@mkdir -p wave/mu1000
	@rm -f wave/mu1000/*.wav
	build/mu1000 -w

MU2000 = mu2000 mu2000/mu2000-v2.01-h.bin mu2000/mu2000-v2.01-l.bin

.PHONY : mu2000
mu2000 : build build/mu2000

mu2000-firmware : $(MU2000)
	build/mu2000 -f

mu2000-midi : $(MU2000)
	build/mu2000 -m

mu2000-table : $(MU2000)
	build/mu2000 -t > table/mu2000.txt

mu2000-bitmap : $(MU2000)
	build/mu2000 -b

mu2000-wave : $(MU2000) mu2000/xv364a0.ic49 mu2000/xv365a0.ic50 mu2000/xw848a0.ic53 mu2000/xw849a0.ic54
	@mkdir -p wave/mu2000
	@rm -f wave/mu2000/*.wav
	build/mu2000 -w

PSR500 = psr500 psr500/xj426b0.ic3

.PHONY : psr500
psr500 : build build/psr500

psr500-table : $(PSR500)
	build/psr500 -t > table/psr500.txt

psr500-wave : $(PSR500)
	@mkdir -p wave/psr500
	@rm -f wave/psr500/*.wav
	build/psr500 -w

SW1000XG = sw1000xg sw1000xg/1.06.06_xv561d0.ic102

.PHONY : sw1000xg
sw1000xg : build build/sw1000xg

sw1000xg-firmware : $(SW1000XG)
	build/sw1000xg -f

sw1000xg-midi : $(SW1000XG)
	build/sw1000xg -m

sw1000xg-table : $(SW1000XG)
	build/sw1000xg -t > table/sw1000xg.txt

sw1000xg-wave : $(SW1000XG) sw1000xg/xt445a0-828.ic124 sw1000xg/xt461a0-829.ic123 sw1000xg/xv389a0.ic122 sw1000xg/xv390a0.ic121
	@mkdir -p wave/sw1000xg
	@rm -f wave/sw1000xg/*.wav
	build/sw1000xg -w

SYXG50 = syxg50 syxg50/sxgbin41.tbl

.PHONY : syxg50
syxg50 : build build/syxg50

syxg50-table : $(SYXG50)
	build/syxg50 -t > table/syxg50.txt

syxg50-wave : $(SYXG50) syxg50/Sxgwave4.tbl
	@mkdir -p wave/syxg50
	@rm -f wave/syxg50/*.wav
	build/syxg50 -w

TG100 = tg100 tg100/xk731c0.ic4

.PHONY : tg100
tg100 : build build/tg100

tg100-midi : $(TG100)
	build/tg100 -m

tg100-font : $(TG100)
	build/tg100 -b

tg100-table : $(TG100) tg100/xk992a0.ic6
	build/tg100 -t > table/tg100.txt

tg100-wave : $(TG100) tg100/xk992a0.ic6
	@mkdir -p wave/tg100
	@rm -f wave/tg100/*.wav
	build/tg100 -w

YRW801 = yrw801 yrw801/yrw801.rom

.PHONY : yrw801
yrw801 : build build/yrw801

yrw801-table : $(YRW801)
	build/yrw801 -t > table/yrw801.txt

yrw801-wave : $(YRW801)
	@mkdir -p wave/yrw801
	@rm -f wave/yrw801/*.wav
	build/yrw801 -w

build :
	@mkdir -p build

build/% : %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean :
	rm -rf build/
