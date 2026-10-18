#!/bin/bash
# Drives a fresh page from load to the first round of a 1-on-1 match.
node "$(dirname "$0")/cdp.mjs" ${PORT:-9555} goto "${BASE:-http://127.0.0.1:8642}/index.html?debug=1" wait 1500 click "#play" wait 1000 \
  key Space wait 2500 key Space wait 2500 key Space wait 3200 key Enter wait 2500 key ArrowDown wait 300 key Enter wait 3000 \
  key Enter wait 3000 key Space wait 2500 key ArrowDown wait 150 key ArrowDown wait 150 key ArrowDown wait 150 key ArrowRight wait 300 key Enter wait 2500 \
  key Space wait 2500 key ArrowDown wait 150 key ArrowDown wait 150 key ArrowDown wait 150 key ArrowRight wait 300 key Enter wait 2500 \
  key Space wait 3000 "$@"
