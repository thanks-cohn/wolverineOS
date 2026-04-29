CC=gcc
CFLAGS=-Wall -Wextra -O2 -std=c11 -I./src/include

SRC=src/cli/ivault.c src/core/seal.c src/core/verify.c src/core/restore.c src/core/prune.c src/core/ledger.c src/core/timewalker.c src/core/watch.c src/core/manifest.c src/core/hash.c src/core/fs.c src/core/log.c

BIN=ivault

all: $(BIN) imeta

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

imeta:
	$(MAKE) -C modules/imeta
	cp modules/imeta/imeta ./imeta
	cp modules/imeta/imeta-watchd ./imeta-watchd

clean:
	rm -f $(BIN) imeta imeta-watchd
	$(MAKE) -C modules/imeta clean
	rm -rf tests/timewalker/demo tests/timewalker/recovered vaults state logs
	mkdir -p vaults state logs
	touch vaults/.gitkeep state/.gitkeep logs/.gitkeep

