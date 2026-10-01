#!/bin/bash
# テスト用の音声ファイルを作る（自作の合成音。著作物は使わない）
#   44.1 kHz / ステレオ / 2.0 秒 = 88200 サンプル。0.5 秒（22050 サンプル目）から 10 ms だけ 1 kHz・振幅 0.5
#   その位置を、デコーダの頭のずれ（エンコーダ遅延の扱い）を測る目印にする
#   burst.mp3         LAME（Xing / LAME タグで頭と尻の詰め物）
#   burst.m4a         ffmpeg の AAC（edit list で頭の詰め物 1024 を飛ばす）
#   burst_itunes.m4a  同じ AAC を edit list なし＋ iTunes の iTunSMPB で（iTunes で作った m4a と同じ形）
set -euo pipefail
cd "$(dirname "$0")"
SRC="aevalsrc='0.5*sin(2*PI*1000*(t-0.5))*between(t,0.5,0.51)':s=44100:c=stereo:d=2"
ffmpeg -loglevel error -y -f lavfi -i "$SRC" -c:a libmp3lame -b:a 128k burst.mp3
ffmpeg -loglevel error -y -f lavfi -i "$SRC" -c:a aac -b:a 128k burst.m4a
ffmpeg -loglevel error -y -f lavfi -i "$SRC" -c:a aac -b:a 128k -use_editlist 0 burst_noedit.m4a
python3 add_itunsmpb.py burst_noedit.m4a burst_itunes.m4a 1024 88200
rm burst_noedit.m4a
