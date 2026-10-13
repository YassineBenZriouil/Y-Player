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

# Phase 5: 2 audio tracks, 2 subtitle tracks, 3 chapters (multitrack.mkv) + external subtitles.
printf ';FFMETADATA1\n[CHAPTER]\nTIMEBASE=1/1000\nSTART=0\nEND=2000\ntitle=Opening\n[CHAPTER]\nTIMEBASE=1/1000\nSTART=2000\nEND=4000\ntitle=Middle\n[CHAPTER]\nTIMEBASE=1/1000\nSTART=4000\nEND=6000\ntitle=End\n' > chapters.txt
printf '1\n00:00:00,000 --> 00:00:05,900\nEnglish subtitle\n' > sub_en.srt
printf '1\n00:00:00,000 --> 00:00:05,900\nSous-titre français\n' > sub_fr.srt
$FF -f lavfi -i testsrc2=size=320x240:rate=30 -f lavfi -i sine=frequency=440:sample_rate=48000 \
    -f lavfi -i sine=frequency=880:sample_rate=48000 -i sub_en.srt -i sub_fr.srt -i chapters.txt -t 6 -y -hide_banner -loglevel error \
    -map 0:v -map 1:a -map 2:a -map 3:s -map 4:s -map_metadata 5 -map_chapters 5 \
    -c:v libx264 -preset veryslow -b:v 120k -pix_fmt yuv420p -c:a:0 aac -b:a:0 48k -c:a:1 libopus -b:a:1 48k -c:s srt \
    -metadata:s:a:0 language=eng -metadata:s:a:0 title=Main -metadata:s:a:1 language=fre -metadata:s:a:1 title=Commentary \
    -metadata:s:s:0 language=eng -metadata:s:s:1 language=fre multitrack.mkv
rm -f chapters.txt sub_en.srt sub_fr.srt
# External subtitles: UTF-8 with right-to-left text, and a deliberately malformed file.
printf '1\n00:00:00,000 --> 00:00:05,000\nمرحبا بالعالم — שלום עולם — こんにちは\n' > external_rtl.srt
printf 'this is not\n--> a subtitle 99:99\n\x00\x01garbage\n' > malformed.srt
