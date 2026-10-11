#!/bin/sh
# Runs every clip x embed mode x hwdec setting once; appends JSON lines to results.jsonl.
# Usage: sh run_matrix.sh <media-dir> [seconds]
MEDIA=$1; SECS=${2:-15}
for f in "$MEDIA"/*; do
  for mode in wid gl; do
    for hw in auto-safe no; do
      line=$(timeout 120 ./build/mpv-embed-spike.exe $mode "$f" $SECS $hw 2>&1 | tail -1)
      echo "{\"file\":\"$(basename "$f")\",${line#\{}" | tee -a results.jsonl
    done
  done
done
