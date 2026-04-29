CC = gcc
CFLAGS = -Wall -Wextra -O2

# === Binaries ===
CLI_BIN = imeta
WATCHD_BIN = imeta-watchd

# === Source Files ===
CLI_SRC = \
    src/cli/imeta.c \
    src/core/scan.c \
    src/core/index.c \
    src/core/hash.c \
    src/core/bind.c \
    src/core/bind_missing.c \
    src/core/doctor.c \
    src/core/reconcile.c \
    src/core/memory.c

WATCHD_SRC = \
    src/daemon/imeta_watchd.c \
    src/core/hash.c \
    src/core/bind.c

# === Build Targets ===
all: $(CLI_BIN) $(WATCHD_BIN)

$(CLI_BIN): $(CLI_SRC)
	$(CC) $(CFLAGS) -o $@ $(CLI_SRC)

$(WATCHD_BIN): $(WATCHD_SRC)
	$(CC) $(CFLAGS) -o $@ $(WATCHD_SRC)

# === Clean ===
clean:
	rm -f $(CLI_BIN) $(WATCHD_BIN)

# === Convenience ===
rebuild: clean all
