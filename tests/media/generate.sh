#!/bin/sh
# Regenerates the tiny test clips used by tst_mpvengine. Needs an FFmpeg build with
# libx264, libx265, libvpx, libsvtav1 and libopus. Usage: sh generate.sh [path/to/ffmpeg]
set -e
FF=${1:-ffmpeg}
V="-f lavfi -i testsrc2=size=320x240:rate=30 -f lavfi -i sine=frequency=440:sample_rate=48000 -t 3 -y -hide_banner -loglevel error"
$FF $V -c:v libx264 -preset veryslow -b:v 150k -pix_fmt yuv420p -c:a aac -b:a 48k h264_aac.mp4
$FF $V -c:v libx265 -preset medium -b:v 150k -pix_fmt yuv420p10le -x265-params log-level=error -c:a aac -b:a 48k hevc10_aac.mkv
$FF $V -c:v libvpx-vp9 -b:v 150k -deadline good -cpu-used 4 -c:a libopus -b:a 48k vp9_opus.webm
$FF $V -c:v libsvtav1 -preset 8 -b:v 150k -pix_fmt yuv420p10le -c:a libopus -b:a 48k av1_opus.mkv
printf 'this is not a media file\n' > not_media.mp4
