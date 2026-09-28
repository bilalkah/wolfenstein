#!/bin/bash
# Renders the music scripts/import_freedoom.py leaves as MIDI (build/music)
# to MP3s in assets/music, with FluidSynth and the FluidR3 General MIDI
# soundfont (MIT licence, kept in assets/licenses/music), in a Docker
# container: nothing to install. The web build plays MP3 only.
#
# Usage: ./scripts/render_music.sh [midi dir]
set -euo pipefail

cd "$(dirname "$0")/.."
MIDI="${1:-build/music}"
OUT=assets/music
LICENSES=assets/licenses/music
mkdir -p "$OUT" "$LICENSES"

docker run --rm -v "$PWD":/repo -w /repo debian:bookworm-slim bash -c "
	set -euo pipefail
	apt-get update -qq >/dev/null
	DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
		fluidsynth fluid-soundfont-gm ffmpeg >/dev/null
	for midi in $MIDI/*.mid; do
		name=\$(basename \"\$midi\" .mid)
		fluidsynth -ni -q -F /tmp/\$name.wav -r 44100 -g 0.6 \
			/usr/share/sounds/sf2/FluidR3_GM.sf2 \"\$midi\" >/dev/null
		# Even loudness from track to track; stereo at 96 kbit/s
		ffmpeg -loglevel error -y -i /tmp/\$name.wav \
			-af loudnorm=I=-18:TP=-2 -ar 44100 \
			-codec:a libmp3lame -b:a 96k $OUT/\$name.mp3
		echo \"\$name: \$(ffprobe -v error -show_entries format=duration \
			-of default=nw=1:nk=1 $OUT/\$name.mp3)s\"
	done
	cp /usr/share/doc/fluid-soundfont-gm/copyright $LICENSES/FluidR3_GM-copyright.txt
	chown -R $(id -u):$(id -g) $OUT $LICENSES
"
