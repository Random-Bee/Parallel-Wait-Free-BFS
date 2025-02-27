#!/bin/bash

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
  echo Wait-Free
  for j in {1..5}
  do
    echo $i | ./wf datasets/liveJournal1.txt
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