# Makefile for Pokemon Yellow Recompiled
#
# Usage:
#   make              # build for your current platform
#   make run          # build and run
#   make windows      # cross-compile .exe (needs mingw-w64)
#   make clean        # remove build artifacts

# Core files shared across all targets
CORE = interpreter.c ppu.c recompiled_game.c
CFLAGS = -O2 -Wall

# ---- Linux / macOS (default) -----------------------------------------------
gbrecomp: sdl_main.c $(CORE)
	gcc $(CFLAGS) -o $@ $^ $(shell sdl2-config --cflags --libs)
	@echo "Built: $@"

run: gbrecomp
	./gbrecomp pokemon_yellow.gb

# ---- Headless build (no SDL, PNG output only) -------------------------------
headless: main.c $(CORE)
	gcc $(CFLAGS) -o gbrecomp_headless $^
	@echo "Built: gbrecomp_headless  (no window - use GB_SCREENSHOT_SAMPLES=1)"

# ---- Windows cross-compile (.exe) ------------------------------------------
# Requires: apt install mingw-w64
# Requires SDL2 Windows dev package from https://github.com/libsdl-org/SDL/releases
# Extract SDL2-devel-*-mingw.tar.gz and set SDL2_WIN to the x86_64-w64-mingw32 folder
SDL2_WIN ?= /opt/SDL2-mingw

windows: sdl_main.c $(CORE)
	x86_64-w64-mingw32-gcc $(CFLAGS) -o gbrecomp.exe $^ \
		-I$(SDL2_WIN)/include/SDL2 \
		-L$(SDL2_WIN)/lib \
		-lSDL2main -lSDL2 \
		-mwindows \
		-static-libgcc -static-libstdc++
	@echo "Built: gbrecomp.exe"
	@echo "Copy SDL2.dll from $(SDL2_WIN)/bin/ next to gbrecomp.exe"

clean:
	rm -f gbrecomp gbrecomp_headless gbrecomp.exe *.o
	rm -rf screenshots/

.PHONY: run headless windows clean
