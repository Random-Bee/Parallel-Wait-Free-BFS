#!/bin/bash

# Experiment 1

# for i in {100000..600000..100000}
# do
#   echo $i
  
#   echo 2 $i | ./a.out > datasets/dummy1.txt

#   echo Sequential
#   for j in {1..5}
#   do
#     ./seq datasets/dummy1.txt
#     sleep 5
#   done

#   echo Wait-Free
#   for j in 16
#   do
#     echo $j
#     for k in {1..5}
#     do
#       echo $j | ./wf datasets/dummy1.txt
#       sleep 5
#     done
#   done

#   echo Barrier
#   for j in 16
#   do
#     echo $j
#     for k in {1..5}
#     do
#       echo $j | ./bar datasets/dummy1.txt
#       sleep 5
#     done
#   done

# done

# Experiment 2

# echo 2 500000 | ./a.out > datasets/dummy1.txt
# echo Sequential
# for i in {1..5}
# do
#   ./seq datasets/dummy1.txt
#   sleep 5
# done
# echo

# for i in 2 4 8 16 32 64
# do
#   echo $i
#   echo Wait-Free
#   for j in {1..5}
#   do
#     echo $i | ./wf datasets/dummy1.txt
#     sleep 5
#   done
#   echo

#   echo Barrier
#   for j in {1..5}
#   do
#     echo $i | ./bar datasets/dummy1.txt
#     sleep 5
#   done
#   echo
# done


# for i in 2 4 8 16 32 64
# do
#   echo $i
#   echo Wait-Free
#   for j in {1..5}
#   do
#     echo $i | ./wf datasets/soc-LiveJournal1.txt
#     sleep 5
#   done
#   echo

#   echo Barrier
#   for j in {1..5}
#   do
#     echo $i | ./bar datasets/soc-LiveJournal1.txt
#     sleep 5
#   done
#   echo
# done

# for i in 2 4 8 16 32 64
# do
#   echo $i
#   echo Wait-Free
#   for j in {1..5}
#   do
#     echo $i | ./wf datasets/soc-sinaweibo.mtx
#     sleep 5
#   done
#   echo

#   echo Barrier
#   for j in {1..5}
#   do
#     echo $i | ./bar datasets/soc-sinaweibo.mtx
#     sleep 5
#   done
#   echo
# done


# Experiment 3

# echo 2 500000 | ./a.out > datasets/dummy1.txt
# echo Sequential
# for i in {1..5}
# do
#   ./seq datasets/dummy1.txt
#   sleep 5
# done

for i in {1..5}
do
  echo $i
  # echo Wait-Free
  # for j in {1..5}
  # do
  #   echo 16 $i | ./wftd datasets/dummy1.txt
  #   sleep 5
  # done
  # echo

  echo Barrier
  for j in {1..5}
  do
    echo 16 $i | ./bartd datasets/dummy1.txt
    sleep 5
  done
  echo
done