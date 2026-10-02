# minimp3（同梱）

- 出典: https://github.com/lieff/minimp3
- 取得したコミット: ea99364f61c14656440e8d77e9c233ccf3124633（master、取得日 2026-10-01）
- ライセンス: CC0 1.0（パブリックドメイン相当。LICENSE を参照）
- 使い方: app/audio/Mp3Format.cpp で mp3 を読む（全 OS 共通）

選んだ理由（DESIGN 19）:
- LAME / Xing タグの遅延・詰め物を読んで曲の頭と尻を切る（gapless）。ffmpeg と同じ規約で、OS によって頭の位置が変わらない
- サンプル単位のシーク（MP3D_SEEK_TO_SAMPLE）
- ヘッダだけ・依存なし・ライセンス表記の義務なし

中身は手を入れない。更新するときはこの README のコミットも書き換える。
