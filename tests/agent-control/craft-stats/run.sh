#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
# Explicit public source checkout, SDL/yaml SDK prefix, legally available stock data,
# and an exclusive new output directory. No hardcoded private resource paths.
S=$(realpath "${1:?source src directory}")
SDK=$(realpath "${2:?SDK usr prefix}")
DATA=$(realpath "${3:?stock data root}")
BUILD=${4:?new build directory}
N=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
mkdir "$BUILD"; BUILD=$(realpath "$BUILD"); mkdir "$BUILD/obj"
python "$N/prepare.py" "$S" "$BUILD"
cp "$N/Test.cpp" "$BUILD/Test.cpp"
I=$SDK/include; L=$SDK/lib/x86_64-linux-gnu
FLAGS="-std=c++11 -O2 -g -DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL -D__NO_OPENGL -DOPENXCOM_LOADING_PRESENTATION_HELPERS -ffunction-sections -fdata-sections"
for unit in Engine/Surface Engine/Font Engine/Unicode Engine/Language Engine/LanguagePlurality Engine/LocalizedText Engine/Palette Engine/Action Engine/CrossPlatform Interface/Text Engine/InteractiveSurface Interface/NumberText Observer/AgentUi Observer/AgentProtocol Observer/AgentProjectileWitness Mod/RuleInterface Interface/ImageButton Interface/TextButton Interface/ComboBox Interface/TextList Interface/ArrowButton Interface/ScrollBar Interface/Window Engine/Timer lodepng; do
 name=$(echo "$unit" | tr / _)
 c++ $FLAGS -iquote"$S" -I"$I" -I"$I/SDL" -c "$S/$unit.cpp" -o "$BUILD/obj/$name.o"
done
c++ $FLAGS -iquote"$S" -I"$I" -I"$I/SDL" "$BUILD/Test.cpp" "$BUILD"/obj/*.o -Wl,--gc-sections -L"$L" -Wl,-rpath,"$L" -Wl,-rpath-link,"$L" -lSDL -lSDL_image -lSDL_gfx -lSDL_mixer -lyaml-cpp -o "$BUILD/native-craft"
LD_LIBRARY_PATH="$L${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$BUILD/native-craft" "$DATA"
