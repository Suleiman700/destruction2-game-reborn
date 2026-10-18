#!/bin/bash
# Builds the Destruction II web port. Usage: ./build.sh [debug]
set -e
cd "$(dirname "$0")"
source "${EMSDK:-$HOME/emsdk}/emsdk_env.sh" >/dev/null 2>&1
export MODE=${1:-release}
OPT="-O2"
[ "$MODE" = debug ] && OPT="-O0 -g -sASSERTIONS=2 -sSAFE_HEAP=1"
export CXXFLAGS="$OPT -std=gnu++11 -fms-extensions -fms-compatibility -fms-compatibility-version=19.0 -fno-strict-aliasing -Icompat -Isrc -Wno-everything -DWIN32 -D_WINDOWS"
OUT=dist; [ "$MODE" = debug ] && OUT=dist-debug
mkdir -p obj/$MODE $OUT
compile_one() {
  f=$1; o=obj/$MODE/$(basename "${f%.cpp}").o
  if [ -f "$o" ] && [ "$o" -nt "$f" ] && [ -z "$(find compat src -name '*.h' -newer "$o" -print -quit)" ]; then return 0; fi
  em++ $CXXFLAGS -c "$f" -o "$o" 2>"$o.log" || { echo "FAIL $f"; rm -f "$o"; return 1; }
}
export -f compile_one
if ! ls src/*.cpp compat/*.cpp | xargs -P 8 -n 1 bash -c 'compile_one "$0"'; then echo "compile failed"; exit 1; fi
em++ $OPT obj/$MODE/*.o -o $OUT/d2.js \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sSTACK_SIZE=1MB \
  -sEXPORTED_FUNCTIONS=_main,_d2_key,_d2_mix,_d2_focus,_d2_request_sync,_d2_debug_status,_d2_dispatch_destroy,_malloc,_free \
  -sEXPORTED_RUNTIME_METHODS=ccall,callMain,FS,addRunDependency,removeRunDependency,HEAPU8,HEAPF32 \
  -lidbfs.js --preload-file assets@/game
cp shell/index.html $OUT/index.html
echo "built $OUT/ ($MODE)"
