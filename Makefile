# INIT WARS: The Last PID - GNU Makefile
#
#   make            build build/init-wars
#   make run        build and run
#   make test       build and run the headless test-suite (no window or audio needed)
#   make install    install to $(PREFIX) (default /usr/local); honours DESTDIR
#   make clean

CC       ?= cc
PREFIX   ?= /usr/local
DATADIR  ?= $(PREFIX)/share/init-wars
OPT      ?= -O2 -g

WARN     := -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wstrict-prototypes
CFLAGS_ALL := -std=gnu11 $(OPT) $(WARN) -DINITWARS_DATADIR='"$(DATADIR)"' $(CFLAGS)

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS   := $(shell sdl2-config --libs 2>/dev/null)
LIBS       := -lm

BUILD := build
CORE_SRC := src/common.c src/balance.c src/font.c src/sprites.c src/render.c \
            src/score.c src/achievement.c src/profile.c src/story.c src/codex.c src/game.c src/view.c
SDL_SRC  := src/main.c src/audio.c src/assets.c

CORE_OBJ := $(CORE_SRC:src/%.c=$(BUILD)/%.o)
SDL_OBJ  := $(SDL_SRC:src/%.c=$(BUILD)/%.o)
HEADERS  := $(wildcard src/*.h)

.PHONY: all run test strict strict-core strict-sdl analyze sanitize smoke capture install uninstall clean check-sdl

all: check-sdl $(BUILD)/init-wars

check-sdl:
	@command -v sdl2-config >/dev/null 2>&1 || { \
	  echo "error: sdl2-config not found. Install the SDL2 development package"; \
	  echo "  Void:   doas xbps-install -S SDL2-devel"; \
	  echo "  Debian: sudo apt install libsdl2-dev"; \
	  echo "  Fedora: sudo dnf install SDL2-devel"; \
	  echo "  Arch/EndeavourOS: sudo pacman -S --needed base-devel sdl2"; \
	  exit 1; }

$(BUILD)/init-wars: $(CORE_OBJ) $(SDL_OBJ)
	$(CC) -o $@ $^ $(SDL_LIBS) $(LIBS) $(LDFLAGS)

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS_ALL) $(SDL_CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

run: all
	./$(BUILD)/init-wars

# ---- tests: the game logic is SDL-free, so no display or audio is needed ----
$(BUILD)/test_game: tests/test_game.c $(CORE_OBJ) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS_ALL) -Isrc -o $@ tests/test_game.c $(CORE_OBJ) $(LIBS)

test: $(BUILD)/test_game
	./$(BUILD)/test_game

# strict: -Werror plus extra warnings at -O0/-O2/-O3 with gcc and (if present) clang. Optimisation level
# matters: some warnings (format truncation, maybe-uninitialized) only appear at -O3.
#   strict-core  game logic, renderer, tests: needs NO SDL headers
#   strict-sdl   main.c, audio.c, assets.c:  needs SDL2 development files
#   strict       both (strict-sdl is skipped with a notice when sdl2-config is missing)
STRICT_WARN := $(WARN) -Wconversion -Wsign-conversion -Wdouble-promotion -Wcast-qual -Wundef -Wmissing-prototypes -Werror
STRICT_CORE_FILES := $(CORE_SRC) tests/test_game.c

define STRICT_LOOP
for cc in gcc clang; do command -v $$cc >/dev/null 2>&1 || continue; \
	  for o in -O0 -O2 -O3; do \
	    echo "$(1): $$cc $$o"; \
	    for f in $(2); do \
	      $$cc -std=gnu11 $$o $(STRICT_WARN) -Isrc $(3) -DINITWARS_DATADIR='"/x"' -c -o /dev/null $$f || exit 1; \
	    done; \
	  done; \
	done; echo "$(1): OK"
endef

strict-core:
	@$(call STRICT_LOOP,strict-core,$(STRICT_CORE_FILES),)

strict-sdl:
	@if command -v sdl2-config >/dev/null 2>&1; then \
	  $(call STRICT_LOOP,strict-sdl,$(SDL_SRC),$(SDL_CFLAGS)); \
	else echo "strict-sdl: SKIPPED (sdl2-config not found; install the SDL2 development package)"; fi

strict: strict-core strict-sdl

# analyze: clang static analyzer incl. the experimental stream/security checkers, on the SDL-free core.
# Fails on any report.
analyze:
	@command -v clang >/dev/null 2>&1 || { echo "analyze: SKIPPED (clang not found)"; exit 0; }
	@out=$$(for f in $(CORE_SRC); do clang --analyze -Xanalyzer -analyzer-checker=alpha.core,alpha.security,alpha.unix,alpha.deadcode -std=gnu11 -Isrc $$f -o /dev/null 2>&1; done | grep "warning:"); \
	  if [ -n "$$out" ]; then echo "$$out"; echo "analyze: FAILED"; exit 1; fi; echo "analyze: OK"

# sanitize: prefer clang when available (much faster instrumented compilation here), else use CC.
SAN_CC ?= $(shell command -v clang >/dev/null 2>&1 && echo clang || echo $(CC))
SAN_OPT ?= -O1 -g
sanitize:
	mkdir -p $(BUILD)
	$(SAN_CC) -std=gnu11 $(SAN_OPT) -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer \
	  -Isrc -o $(BUILD)/test_game_san tests/test_game.c $(CORE_SRC) $(LIBS)
	./$(BUILD)/test_game_san

# smoke: runs the real binary on a private Xvfb (+ Window Maker if installed); see tests/smoke.sh
# capture: developer tool that drives the real game and saves renderer output (see screenshots/README.md)
$(BUILD)/capture: tools/capture.c tests/bot.h $(CORE_OBJ) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS_ALL) $(SDL_CFLAGS) -Isrc -o $@ tools/capture.c $(CORE_OBJ) $(SDL_LIBS) $(LIBS)

capture: check-sdl $(BUILD)/capture

smoke: all
	sh tests/smoke.sh

install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(DATADIR)
	install -m 755 $(BUILD)/init-wars $(DESTDIR)$(PREFIX)/bin/init-wars
	@if [ -d assets ] && [ -n "$$(ls -A assets 2>/dev/null)" ]; then cp -r assets/. $(DESTDIR)$(DATADIR)/; fi

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/init-wars
	rm -rf $(DESTDIR)$(DATADIR)

clean:
	rm -rf $(BUILD)
