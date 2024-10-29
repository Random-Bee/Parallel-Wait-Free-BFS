#!/bin/bash

# Experiment 1

echo Experiment-1
echo

for i in 2 4 8 16 32 64
do
  echo $i
  echo Parallel
  for j in {1..3}
  do
    echo $i | ./par datasets/dummy.txt
    sleep 2
  done
  echo
done