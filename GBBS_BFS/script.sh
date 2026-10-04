#!/bin/bash
set -euo pipefail

# built as every program here is (see DO_BFS/script.sh for the compiler,
# standard and alignment). ParlayLib is the version GBBS's MODULE.bazel pins,
# cloned where GBBS's makefiles look for it:
#   git clone https://github.com/ParAlg/parlaylib.git ../gbbs/external/parlaylib
#   git -C ../gbbs/external/parlaylib checkout 352f5367a6b3bd2f0c6f05882cdd02e978e349ea
# It needs 16-byte compare-and-swap and threads. NDEBUG and the standard
# allocator are what GBBS's own build (.bazelrc) uses; without NDEBUG, edgeMap
# prints timings every round
cxx=/opt/rh/gcc-toolset-15/root/usr/bin/g++
"$cxx" -std=c++23 -O2 -g -fno-omit-frame-pointer -falign-loops=32 -falign-jumps=32 -mcx16 -pthread \
    -DNDEBUG -DPARLAY_USE_STD_ALLOC -I ../gbbs/external/parlaylib/include \
    gbbs-bfs.cpp -o gbbs.out
echo "GBBS BFS compiled"

for i in 2 4 8 16 32 64
do
  echo $i
  echo GBBS
  for j in {1..5}
  do
    ./gbbs.out ../datasets/liveJournal1.edges $i
    sleep 2
  done
  echo
done
