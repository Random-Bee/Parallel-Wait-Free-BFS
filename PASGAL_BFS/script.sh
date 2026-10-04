#!/bin/bash
set -euo pipefail

# built as every program here is (see DO_BFS/script.sh for the compiler,
# standard and alignment). ParlayLib comes from PASGAL's own submodule
# (git -C ../PASGAL submodule update --init) and needs 16-byte
# compare-and-swap and threads; NDEBUG only drops assert checks
cxx=/opt/rh/gcc-toolset-15/root/usr/bin/g++
"$cxx" -std=c++23 -O2 -g -fno-omit-frame-pointer -falign-loops=32 -falign-jumps=32 -mcx16 -pthread \
    -DNDEBUG -I ../PASGAL/external/parlaylib/include pasgal-bfs.cpp -o pasgal.out
echo "PASGAL BFS compiled"

for i in 2 4 8 16 32 64
do
  echo $i
  echo PASGAL
  for j in {1..5}
  do
    ./pasgal.out ../datasets/liveJournal1.edges $i
    sleep 2
  done
  echo
done
