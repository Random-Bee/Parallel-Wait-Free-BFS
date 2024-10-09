#!/bin/bash

echo 16 1

for i in {1..5}
do
  echo 16 1 | ./wftd datasets/dummy1.txt
  sleep 5
done

echo 16 2

for i in {1..5}
do
  echo 16 2 | ./wftd datasets/dummy1.txt
  sleep 5
done

echo 16 3

for i in {1..5}
do
  echo 16 4 | ./wftd datasets/dummy1.txt
  sleep 5
done

echo 16 4

for i in {1..5}
do
  echo 16 8 | ./wftd datasets/dummy1.txt
  sleep 5
done

echo 16 5

for i in {1..5}
do
  echo 16 15 | ./wftd datasets/dummy1.txt
  sleep 5
done