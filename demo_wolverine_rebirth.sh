#!/usr/bin/env bash
set -euo pipefail

echo "=== IVAULT / WOLVERINE REBIRTH DEMO ==="

rm -rf /tmp/ivault-wolverine-demo
mkdir -p /tmp/ivault-wolverine-demo/docs

echo "alpha core file" > /tmp/ivault-wolverine-demo/a.txt
echo "beta protected file" > /tmp/ivault-wolverine-demo/docs/b.txt

echo
echo "=== 1. SEAL CLEAN SYSTEM ==="
./ivault seal /tmp/ivault-wolverine-demo
VAULT="$(./ivault latest | tail -n 1)"
echo "VAULT=$VAULT"

echo
echo "=== 2. VERIFY CLEAN ==="
./ivault verify /tmp/ivault-wolverine-demo "$VAULT"

echo
echo "=== 3. DAMAGE THE SYSTEM ==="
rm /tmp/ivault-wolverine-demo/a.txt
echo "corrupted beta" > /tmp/ivault-wolverine-demo/docs/b.txt
echo "enemy file" > /tmp/ivault-wolverine-demo/intruder.txt

./ivault verify /tmp/ivault-wolverine-demo "$VAULT" || true

echo
echo "=== 4. WOLVERINE REBIRTH WATCH ==="
./ivault watch /tmp/ivault-wolverine-demo "$VAULT" --interval 1 --cycles 2 --auto-restore --auto-prune

echo
echo "=== 5. FINAL VERIFY ==="
./ivault verify /tmp/ivault-wolverine-demo "$VAULT"

echo
echo "=== 6. AUDIT TRAIL ==="
./ivault audit

echo
echo "=== DEMO COMPLETE: SYSTEM REBORN ==="
