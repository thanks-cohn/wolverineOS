# ivault v0.5 — Timewalker

`ivault` is the WolverineOS resurrection vault: a tiny C-first recovery core that seals folders into content-addressed vaults, verifies live folders, restores damage, quarantines unknown extras, compares vaults, and now walks time.

The Wolverine principle is simple: if one valid vault remains, the machine can rebuild itself.

## Commands

```bash
./ivault seal <target-folder>
./ivault verify <target-folder> <vault-folder>
./ivault restore <target-folder> <vault-folder>
./ivault prune <target-folder> <vault-folder> [--delete]
./ivault diff <vault-a> <vault-b>
./ivault latest
./ivault inspect <vault-folder>
./ivault recover-file <vault-folder> <relpath> <output-path>
./ivault timeline
./ivault history
./ivault audit
```

## What v0.5 adds

`latest` prints the newest usable vault in `vaults/`.

`inspect` summarizes a vault: file count, total bytes, PDFs, sidecars, text files, and object store location.

`recover-file` restores one file from a vault to a chosen output path without touching the rest of the live folder.

`timeline` lists all vaults with file counts and byte totals.

Together these make iVault usable as a daily resurrection tool, not just a bulk restore hammer.

## Build

```bash
make
```

## Built-in full test

```bash
make test
```

Final clean state should include:

```text
MISSING: 0
CORRUPTED: 0
EXTRA: 0
ERRORS: 0
STATUS: CLEAN
```

## Real-world test

```bash
rm -rf /tmp/ivault-real-test
mkdir -p /tmp/ivault-real-test
cp ~/Downloads/lively_father_in_law.pdf /tmp/ivault-real-test/

./ivault seal /tmp/ivault-real-test
VAULT="$(find vaults -mindepth 1 -maxdepth 1 -type d | sort | tail -n 1)"

./ivault latest
./ivault inspect "$VAULT"
./ivault verify /tmp/ivault-real-test "$VAULT"
```

Damage and resurrect:

```bash
echo "ruined" > /tmp/ivault-real-test/lively_father_in_law.pdf
echo "intruder" > /tmp/ivault-real-test/extra.txt

./ivault verify /tmp/ivault-real-test "$VAULT" || true
./ivault restore /tmp/ivault-real-test "$VAULT"
./ivault prune /tmp/ivault-real-test "$VAULT"
./ivault verify /tmp/ivault-real-test "$VAULT"
```

Recover one file elsewhere:

```bash
mkdir -p /tmp/ivault-one-file
./ivault recover-file "$VAULT" "lively_father_in_law.pdf" /tmp/ivault-one-file/lively_father_in_law.pdf
```

## Why this matters

Modern systems hide recovery behind clouds, opaque databases, vendor lock-in, and panic buttons that only work until they do not.

iVault keeps the recovery substrate plain: manifests, objects, hashes, ledgers, quarantine, and deterministic rebuilds. It is small enough to audit, fast enough to run anywhere, and simple enough to wrap with a GUI later.

This is the kind of foundation WolverineOS needs: a machine memory that does not beg permission to survive.
