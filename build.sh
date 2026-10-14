#!/usr/bin/env bash
# Builds the game library (build/libgame.a) from source/*.cpp and links the
# demo program (build/dungeon_export) against it.
set -euo pipefail
cd "$(dirname "$0")"

CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -Wpedantic"

mkdir -p build/obj
rm -f build/obj/*.o build/libgame.a

# 1. Compile each library source into an object file.
for src in source/*.cpp; do
    "$CXX" $CXXFLAGS -Isource -c "$src" -o "build/obj/$(basename "${src%.cpp}").o"
done

# 2. Archive the objects into a static library.
ar rcs build/libgame.a build/obj/*.o

# 3. Compile the program and link it against the library.
"$CXX" $CXXFLAGS -Isource app/main.cpp -Lbuild -lgame -o build/dungeon_export

echo "built: build/libgame.a, build/dungeon_export"
