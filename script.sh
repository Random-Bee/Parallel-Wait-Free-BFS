#!/bin/bash

# Experiment 1

# echo Experiment-1
# echo
# for i in {100000..600000..100000}
# do
#   echo $i
  
#   echo 2 $i | ./a.out > datasets/dummy.txt

#   echo Sequential
#   for j in {1..5}
#   do
#     ./seq datasets/dummy.txt
#     sleep 2
#   done
#   echo

#   echo Wait-Free
#   for j in 16
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
#   for j in 16
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
#   for j in 16
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

echo soc-LiveJournal1
echo

for i in 2 4 8 16 32 64
do
  echo $i
  echo Wait-Free
  for j in {1..5}
  do
    echo $i | ./wf datasets/soc-LiveJournal1.txt
    sleep 2
  done
  echo

  echo Wait-Free Atomic
  for j in {1..5}
  do
    echo $i | ./wfa datasets/soc-LiveJournal1.txt
    sleep 2
  done
  echo

  echo Barrier
  for j in {1..5}
  do
    echo $i | ./bar datasets/soc-LiveJournal1.txt
    sleep 2
  done
  echo
done

# echo soc-sinaweibo
# echo

# for i in 2 4 8 16 32 64
# do
#   echo $i
#   echo Wait-Free
#   for j in {1..5}
#   do
#     echo $i | ./wf datasets/soc-sinaweibo.mtx
#     sleep 2
#   done
#   echo

  # echo Wait-Free Atomic
  # for j in {1..5}
  # do
  #   echo $i | ./wfa datasets/soc-sinaweibo.mtx
  #   sleep 2
  # done
  # echo

  # echo Barrier
  # for j in {1..5}
  # do
  #   echo $i | ./bar datasets/soc-sinaweibo.mtx
  #   sleep 2
  # done
  # echo
# done


# Experiment 3

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