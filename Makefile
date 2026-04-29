CC=gcc
CFLAGS=-Wall -Wextra -O2 -std=c11 -I./src/include

IVAULT_SRC=src/cli/ivault.c \
	src/core/seal.c \
	src/core/verify.c \
	src/core/wos_memory.c \
	src/core/restore.c \
	src/core/prune.c \
	src/core/ledger.c \
	src/core/timewalker.c \
	src/core/watch.c \
	src/core/manifest.c \
	src/core/hash.c \
	src/core/fs.c \
	src/core/log.c

MENU_SRC=src/cli/menu.c

IVAULT_BIN=ivault
MENU_BIN=menu

all: $(IVAULT_BIN) $(MENU_BIN) imeta

$(IVAULT_BIN): $(IVAULT_SRC)
	$(CC) $(CFLAGS) -o $(IVAULT_BIN) $(IVAULT_SRC)

$(MENU_BIN): $(MENU_SRC)
	$(CC) $(CFLAGS) -o $(MENU_BIN) $(MENU_SRC)

imeta:
	$(MAKE) -C modules/imeta
	cp modules/imeta/imeta ./imeta
	cp modules/imeta/imeta-watchd ./imeta-watchd

clean:
	rm -f $(IVAULT_BIN) $(MENU_BIN) imeta imeta-watchd
	$(MAKE) -C modules/imeta clean
	@echo "SAFE CLEAN: binaries removed only. vaults/state/logs/.wolverine preserved."

reset-demo:
	@echo "SAFE: reset-demo does not delete user data."
	mkdir -p vaults state logs
	touch vaults/.gitkeep state/.gitkeep logs/.gitkeep

danger-reset-state:
	@echo "REFUSING: Makefile will not delete vaults, state, logs, or .wolverine."
	@echo "Manual deletion only."
	@false
