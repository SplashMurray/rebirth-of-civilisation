#!/bin/sh
# Rebuilds assets/audio from its open-licensed sources.
#
# Sources (see assets/audio/CREDITS.txt for the full credits):
#  * 0 A.D. by Wildfire Games, CC BY-SA 3.0: most music and all sound effects and ambience
#  * FreePD.com (public domain / CC0) via the SoundSafari/CC0-1.0-Music collection: music for
#    the industrial and modern eras, modern war and the Fall
#  * primate calls: Wikimedia Commons and Freesound recordings (CC BY 4.0) collected with their
#    credits by orkuneyb/animal_sounds_flutter, and CC0 recordings (craigsmith, AntumDeluge)
#    collected by teamstarcup/starcup
# Everything is trimmed, loudness-matched and re-encoded here (Ogg Vorbis, 44.1 kHz); each result
# keeps its source's licence.
#
# Needs git and ffmpeg (with libvorbis). Usage: tools/fetch_audio.sh [work-dir]
set -e
ZEROAD_COMMIT=61a3b9507d974084e6badb88a0826bd89a6d5b8b
CC0MUSIC_COMMIT=HEAD   # SoundSafari/CC0-1.0-Music
ANIMALS_COMMIT=HEAD    # orkuneyb/animal_sounds_flutter
STARCUP_COMMIT=HEAD    # teamstarcup/starcup
WORK=${1:-/tmp/rebirth-audio-src}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/assets/audio"
mkdir -p "$WORK"
if [ ! -d "$WORK/0ad" ]; then
    git clone -q --filter=blob:none --sparse https://github.com/0ad/0ad "$WORK/0ad"
    (cd "$WORK/0ad" && git checkout -q $ZEROAD_COMMIT && git sparse-checkout set binaries/data/mods/public/audio)
fi
SRC="$WORK/0ad/binaries/data/mods/public/audio"
mkdir -p "$OUT/music" "$OUT/sfx" "$OUT/ambient" "$OUT/voice"
sparse() {  # sparse() dir repo path...  - fetch just some folders of a GitHub repo
    d="$WORK/$1"; r="$2"; shift 2
    if [ ! -d "$d" ]; then git clone -q --depth 1 --filter=blob:none --no-checkout "https://github.com/$r" "$d"; fi
    (cd "$d" && git sparse-checkout set --no-cone "$@" && git checkout -q HEAD)
}
sparse cc0music SoundSafari/CC0-1.0-Music "/freepd.com/Inventing Flight.mp3" "/freepd.com/Construction.mp3" "/freepd.com/Circuit.mp3" \
    "/freepd.com/Chronos.mp3" "/freepd.com/Finally See The Light.mp3" "/freepd.com/Novus Initium.mp3" "/freepd.com/Parhelion.mp3" \
    "/freepd.com/Spec Ops.mp3" "/freepd.com/Guerilla Tactics.mp3" "/freepd.com/Behind Enemy Lines.mp3" \
    "/freepd.com/Desert Conflict.mp3" "/freepd.com/Beginning of Conflict.mp3" "/freepd.com/Dawn of the Apocalypse.mp3"
sparse animals orkuneyb/animal_sounds_flutter /animal-sound-research/processed/ /animal-sound-research/needs-review/ /animal-sound-research/credits.json
sparse starcup teamstarcup/starcup /Resources/Audio/_starcup/Animals/Monkey/ /Resources/Audio/_starcup/Animals/Gorilla/
LOUD="loudnorm=I=-20:TP=-1.5:LRA=11"

# music: name <- source track
music() { ffmpeg -v error -y -i "$SRC/music/$2" -af "$LOUD" -ar 44100 -ac 2 -c:a libvorbis -q:a 0 "$OUT/music/$1.ogg"; }
pdmusic() { ffmpeg -v error -y -i "$WORK/cc0music/freepd.com/$2" -af "$LOUD" -ar 44100 -ac 2 -c:a libvorbis -q:a 0 "$OUT/music/$1.ogg"; }
music title        Hill_of_Sorrows.ogg
music wander_1     Taiko_1.ogg
music wander_2     Sunrise.ogg
music wander_3     Mountain_Idyll.ogg
music wander_4     Whistler.ogg
music wander_5     Harsh_Lands_Rugged_People.ogg
music ancient_1    Cradle_of_Civilization.ogg
music ancient_2    Forging_a_City-State.ogg
music ancient_3    Land_Between_Rivers.ogg
music ancient_4    The_Fledgling_Kingdom.ogg
music ancient_5    Harvest_Festival.ogg
music later_1      The_Road_Ahead.ogg
music later_2      Rise_of_Macedon.ogg
music later_3      Expedition.ogg
music war_1        Calm_Before_the_Storm.ogg
music war_2        Tale_of_Warriors.ogg
music war_3        Taiko_2.ogg
music fall         Dried_Tears.ogg
music victory      "You_are_Victorious!.ogg"
pdmusic industrial_1  "Inventing Flight.mp3"
pdmusic industrial_2  "Construction.mp3"
pdmusic industrial_3  "Circuit.mp3"
pdmusic industrial_4  "Chronos.mp3"
pdmusic modern_1      "Finally See The Light.mp3"
pdmusic modern_2      "Novus Initium.mp3"
pdmusic modern_3      "Parhelion.mp3"
pdmusic modern_war_1  "Spec Ops.mp3"
pdmusic modern_war_2  "Guerilla Tactics.mp3"
pdmusic modern_war_3  "Behind Enemy Lines.mp3"
pdmusic modern_war_4  "Desert Conflict.mp3"
pdmusic modern_war_5  "Beginning of Conflict.mp3"
pdmusic fall_2        "Dawn of the Apocalypse.mp3"

# sound effects (mono): name <- source [seconds to keep]
sfx() {
    if [ -n "$3" ]; then T="-t $3 -af afade=t=out:st=$(awk "BEGIN{printf \"%.2f\", $3 - 0.25}"):d=0.25"; else T=""; fi
    ffmpeg -v error -y -i "$SRC/$2" $T -ar 44100 -ac 1 -c:a libvorbis -q:a 3 "$OUT/sfx/$1.ogg"
}
sfx click          interface/select/building/sel_universal_1.ogg
sfx notify         interface/ui/chat_alert.ogg
sfx invalid        interface/alarm/alarm_invalid_building_placement_01.ogg
sfx tech           interface/alarm/alarmresearchtech_1.ogg
sfx evolve         interface/alarm/alarmresearchphase_1.ogg
sfx achievement    interface/alarm/alarmobjectives_1.ogg
sfx war            interface/alarm/alarmattackplayer_1.ogg
sfx city_attacked  interface/alarm/alarmattackcity_1.ogg
sfx victory        interface/alarm/alarmvictory_1.ogg
sfx defeat         interface/alarm/alarmdefeat_1.ogg
sfx city_founded   interface/select/building/sel_civ_center.ogg
sfx built          interface/select/building/sel_house.ogg
sfx trade          interface/select/building/sel_market.ogg
sfx trade_sea      interface/select/building/sel_dock.ogg 1.5
sfx culture        interface/select/building/sel_temple.ogg 2
sfx step           actor/singlesteps/fs_grass2.ogg
sfx step_sand      actor/singlesteps/fs_sand1.ogg
sfx step_snow      actor/singlesteps/fs_snow_12.ogg
sfx horse          actor/fauna/animal/horse_select1.ogg
sfx ship           actor/ship/ship_select_01.ogg
sfx ship_move      actor/ship/ship_move.ogg 1.6
sfx siege_select   attack/siege/siege_select_10.ogg
sfx sword          attack/weapon/sword_attack_12.ogg
sfx sword_hit      attack/weapon/swordhit_17.ogg
sfx spear          attack/weapon/spear_attack_08.ogg
sfx bow            attack/weapon/bow_attack_07.ogg 1.2
sfx arrow_hit      attack/impact/arrow_dirt_01.ogg
sfx sling          attack/weapon/sling_211.ogg
sfx shield_hit     attack/impact/shield_wood_01.ogg
sfx musket         attack/weapon/Musket_1.ogg
sfx catapult       attack/siege/onager_shooting_11.ogg
sfx ballista       attack/siege/ballist_attack_01.ogg
sfx siege_hit      attack/impact/siegeprojectilehit_01.ogg 1.8
sfx cannon         attack/destruction/explode_debris_20.ogg 1.6
sfx death          attack/impact/fleshimp_10.ogg
sfx horse_death    actor/mounted/death/death_horse_10.ogg
sfx ship_sinks     actor/ship/warship_death_01.ogg
sfx beast          actor/fauna/animal/lion_attack1.ogg 2
sfx nuke           attack/destruction/explode_debris_20.ogg
sfx collapse       attack/destruction/building_collapse_large_01.ogg
sfx thunder        ambient/weather/thunder_11.ogg 5
sfx wood           resource/construction/con_wood_01.ogg
sfx hoe            resource/farming/hoe_12.ogg
sfx dig            resource/mining/mine_stone_02.ogg

# ambient loops: name <- source [seconds]
amb() {
    ffmpeg -v error -y -i "$SRC/$2" -t $3 -af "afade=t=in:d=0.4,afade=t=out:st=$(awk "BEGIN{printf \"%.2f\", $3 - 0.4}"):d=0.4" -ar 44100 -ac 1 -c:a libvorbis -q:a 1 "$OUT/ambient/$1.ogg"
}
amb waves    ambient/water/wave_21.ogg 28
amb jungle   ambient/dayscape/day_tropical_10.ogg 30
amb birds    ambient/dayscape/day_temperate_gen_01.ogg 45
amb wind     ambient/weather/wind_11.ogg 40
# primate calls: voice/<species>_<n>.ogg  <- source, start, length, [extra filter]
voice() {
    F="atrim=start=$3:duration=$4,asetpts=PTS-STARTPTS,afade=t=in:d=0.03,afade=t=out:st=$(awk "BEGIN{printf \"%.2f\", $4 - 0.12}"):d=0.12"
    [ -n "$5" ] && F="$F,$5"
    ffmpeg -v error -y -i "$2" -af "$F,loudnorm=I=-16:TP=-2" -ar 44100 -ac 1 -c:a libvorbis -q:a 3 "$OUT/voice/$1.ogg"
}
A="$WORK/animals/animal-sound-research"
M="$WORK/starcup/Resources/Audio/_starcup/Animals/Monkey"
G="$WORK/starcup/Resources/Audio/_starcup/Animals/Gorilla"
UP="asetrate=44100*1.26,aresample=44100"     # four semitones up (bonobos call higher than chimpanzees)
DOWN="asetrate=44100*0.84,aresample=44100"   # three semitones down, slower and deeper
voice chimpanzee_1 "$A/processed/chimpanzee.mp3" 0.0 2.0
voice chimpanzee_2 "$A/processed/chimpanzee.mp3" 2.4 2.0
voice chimpanzee_3 "$M/monkey_exclaim1.ogg" 0 1.1
voice bonobo_1 "$M/monkey_say1.ogg" 0 0.6 "$UP"
voice bonobo_2 "$M/monkey_squeak2.ogg" 0 1.1 "$UP"
voice bonobo_3 "$M/monkey_exclaim3.ogg" 0 0.8 "$UP"
voice orangutan_1 "$G/gorilla_say2.ogg" 0 1.1 "$DOWN"
voice orangutan_2 "$G/gorilla_growl3.ogg" 0 1.2 "$DOWN"
voice gorilla_1 "$G/gorilla_growl1.ogg" 0 1.5
voice gorilla_2 "$G/gorilla_say3.ogg" 0 1.2
voice gorilla_3 "$G/gorilla_scream.ogg" 0 0.8
voice baboon_1 "$A/processed/baboon.mp3" 0.0 0.7
voice baboon_2 "$A/processed/baboon.mp3" 2.4 0.8
voice capuchin_1 "$A/processed/capuchin.mp3" 0.4 0.6
voice capuchin_2 "$A/processed/capuchin.mp3" 2.1 0.6
voice capuchin_3 "$A/processed/capuchin.mp3" 4.9 0.55
voice gibbon_1 "$A/processed/gibbon.mp3" 0.1 1.9
voice gibbon_2 "$A/processed/gibbon.mp3" 2.5 1.9
voice macaque_1 "$A/needs-review/japanese_macaque.mp3" 0.0 0.65
voice macaque_2 "$A/needs-review/japanese_macaque.mp3" 0.85 0.6
echo "Audio written to $OUT"
