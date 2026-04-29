#!/usr/bin/env bash
set -e

echo "[imeta] building..."
make

echo "[imeta] installing binaries..."
sudo cp imeta /usr/local/bin/imeta
sudo cp imeta-watchd /usr/local/bin/imeta-watchd

echo "[imeta] preparing default watch folder..."
mkdir -p "$HOME/imeta_watch"

echo "[imeta] installing user systemd service..."
mkdir -p "$HOME/.config/systemd/user"
cp systemd/imeta-watchd.service "$HOME/.config/systemd/user/imeta-watchd.service"

systemctl --user daemon-reload
systemctl --user enable imeta-watchd

echo "[imeta] installed."
echo "Start:  systemctl --user start imeta-watchd"
echo "Logs:   journalctl --user -u imeta-watchd -f"
echo "Test:   echo hello > ~/imeta_watch/test.txt"
