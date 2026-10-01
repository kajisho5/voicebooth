#!/bin/bash
# テスト用の音声ファイルを作る（自作の合成音。著作物は使わない）
#   44.1 kHz / ステレオ / 2.0 秒 = 88200 サンプル。0.5 秒（22050 サンプル目）から 10 ms だけ 1 kHz・振幅 0.5
#   その位置を、デコーダの頭のずれ（エンコーダ遅延の扱い）を測る目印にする
set -euo pipefail
cd "$(dirname "$0")"
SRC="aevalsrc='0.5*sin(2*PI*1000*(t-0.5))*between(t,0.5,0.51)':s=44100:c=stereo:d=2"
ffmpeg -loglevel error -y -f lavfi -i "$SRC" -c:a libmp3lame -b:a 128k burst.mp3
ffmpeg -loglevel error -y -f lavfi -i "$SRC" -c:a aac -b:a 128k burst.m4a
