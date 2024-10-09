#!/bin/bash

echo 2

for i in {1..5}
do
  echo 2 | ./wf datasets/soc-LiveJournal1.txt
  sleep 5
done

echo 4

for i in {1..5}
do
  echo 4 | ./wf datasets/soc-LiveJournal1.txt
  sleep 5
done

echo 8

for i in {1..5}
do
  echo 8 | ./wf datasets/soc-LiveJournal1.txt
  sleep 5
done

echo 16

for i in {1..5}
do
  echo 16 | ./wf datasets/soc-LiveJournal1.txt
  sleep 5
done

echo 32

for i in {1..5}
do
  echo 32 | ./wf datasets/soc-LiveJournal1.txt
  sleep 5
done