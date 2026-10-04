#!/bin/bash
set -euo pipefail

# every program here is built with GCC 15, the newest compiler installed, so
# ours, GAPBS, PASGAL and GBBS share one compiler; the system's GCC 8 cannot
# build PASGAL or GBBS. They are built as C++23, the newest published standard
# (GCC 15 calls its C++26 support highly experimental). Loops start on 32-byte
# boundaries: a hot loop of up to 32 bytes then never spans two cache lines,
# so an edit that moves it does not change the times by a few percent. Jump
# targets do too, since GCC aligns some loop tops as jump targets rather than
# loops: left on 8 bytes, scan_block's frontier test crossed a line in
# wf-dir.cpp and cost it 9% on uk-2002. 64-byte boundaries slowed
# wf-dir-mo.cpp by a third on road
cxx=/opt/rh/gcc-toolset-15/root/usr/bin/g++
"$cxx" -std=c++23 -O2 -g -fno-omit-frame-pointer -falign-loops=32 -falign-jumps=32 -pthread wf-dir.cpp -o wfd.out
echo "Wait-Free Direction-Optimizing BFS compiled"

# Experiment 1

# echo Experiment-1
# echo
# for i in {100000..600000..100000}
# do
#   echo $i

#   echo 2 $i | ./a.out > datasets/dummy.txt

  # echo Sequential
  # for j in {1..5}
  # do
  #   ./seq datasets/dummy.txt
  #   sleep 2
  # done
  # echo

  # echo Wait-Free
  # for j in 16
  # do
  #   echo $j
  #   for k in {1..5}
  #   do
  #     echo $j | ./wf datasets/dummy.txt
  #     sleep 2
  #   done
  # done
  # echo

  # echo Wait-Free Atomic
  # for j in 64
  # do
  #   echo $j
  #   for k in {1..5}
  #   do
  #     echo $j | ./wfa datasets/dummy.txt
  #     sleep 2
  #   done
  # done
  # echo

  # echo Barrier
  # for j in 64
  # do
  #   echo $j
  #   for k in {1..5}
  #   do
  #     echo $j | ./bar datasets/dummy.txt
  #     sleep 2
  #   done
  # done
  # echo

# done

# Experiment 2

# echo Experiment-2
# echo
# echo 2 500000 50 | ./a.out > datasets/dummy.txt
# echo Sequential
# for i in {1..5}
# do
#   ./seq datasets/dummy.txt
#   sleep 2
# done
# echo

# echo

# for i in 2 4 8 16 32 64
# do
#   echo $i
#   echo Wait-Free
#   for j in {1..5}
#   do
#     echo $i | ./wf datasets/dummy.txt
#     sleep 2
#   done
#   echo

#   echo Wait-Free Atomic
#   for j in {1..5}
#   do
#     echo $i | ./wfa datasets/dummy.txt
#     sleep 2
#   done
#   echo

#   echo Barrier
#   for j in {1..5}
#   do
#     echo $i | ./bar datasets/dummy.txt
#     sleep 2
#   done
#   echo
# done

# echo liveJournal1
# echo

# echo Sequential
# for i in {1..5}
# do
#   ./seq datasets/liveJournal1.txt
#   sleep 2
# done
# echo

for i in 2 4 8 16 32 64
do
  echo $i
  echo Wait-Free Direction-Optimizing
  for j in {1..5}
  do
    ./wfd.out ../datasets/liveJournal1.edges $i
    sleep 2
  done
  echo
done

#   echo Wait-Free Atomic
#   for j in {1..5}
#   do
#     echo $i | ./wfa datasets/liveJournal1.txt
#     sleep 2
#   done
#   echo

#   echo Barrier
#   for j in {1..5}
#   do
#     echo $i | ./bar datasets/liveJournal1.txt
#     sleep 2
#   done
#   echo
# done

# echo sinaweibo
# echo

# echo Sequential
# for i in {1..5}
# do
#   ./seq datasets/sinaweibo.mtx
#   sleep 2
# done
# echo

# for i in 2 4 8 16 32 64
# do
#   echo $i
#   echo Wait-Free
#   for j in {1..5}
#   do
#     echo $i | ./wf datasets/sinaweibo.mtx
#     sleep 2
#   done
#   echo

#   echo Wait-Free Atomic
#   for j in {1..5}
#   do
#     echo $i | ./wfa datasets/sinaweibo.mtx
#     sleep 2
#   done
#   echo

#   echo Barrier
#   for j in {1..5}
#   do
#     echo $i | ./bar datasets/sinaweibo.mtx
#     sleep 2
#   done
#   echo
# done

# Experiment 3

# echo Experiment-3
# echo
# n=50000
# for i in 1 5 10 25 50 100
# do
#   echo $i
  
#   echo 4 $n $i | ./a.out > datasets/dummy.txt

#   echo Sequential
#   for j in {1..5}
#   do
#     ./seq datasets/dummy.txt
#     sleep 2
#   done
#   echo

#   echo Wait-Free
#   for j in 64
#   do
#     echo $j
#     for k in {1..5}
#     do
#       echo $j | ./wf datasets/dummy.txt
#       sleep 2
#     done
#   done
#   echo

#   echo Wait-Free Atomic
#   for j in 64
#   do
#     echo $j
#     for k in {1..5}
#     do
#       echo $j | ./wfa datasets/dummy.txt
#       sleep 2
#     done
#   done
#   echo

#   echo Barrier
#   for j in 64
#   do
#     echo $j
#     for k in {1..5}
#     do
#       echo $j | ./bar datasets/dummy.txt
#       sleep 2
#     done
#   done
#   echo

# done


# Experiment 4

# echo 2 500000 | ./a.out > datasets/dummy1.txt
# echo Sequential
# for i in {1..5}
# do
#   ./seq datasets/dummy1.txt
#   sleep 2
# done

# for i in {1..5}
# do
#   echo $i
#   echo Wait-Free
#   for j in {1..5}
#   do
#     echo 16 $i | ./wftd datasets/dummy1.txt
#     sleep 2
#   done
#   echo

#   echo Barrier
#   for j in {1..5}
#   do
#     echo 16 $i | ./bartd datasets/dummy1.txt
#     sleep 2
#   done
#   echo
# done