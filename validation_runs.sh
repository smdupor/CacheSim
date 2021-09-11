#!/bin/bash
./sim_cache 16 1024 2 0 0 0 gcc_trace.txt > gcc.0
./sim_cache 16 1024 2 16 0 0 gcc_trace.txt > gcc.1
./sim_cache 16 1024 2 0 8192 4 gcc_trace.txt > gcc.2
./sim_cache 16 1024 2 16 8192 4 gcc_trace.txt > gcc.3
./sim_cache 16 1024 1 0 0 0 gcc_trace.txt > gcc.4
./sim_cache 16 1024 1 16 0 0 gcc_trace.txt > gcc.5
./sim_cache 16 1024 1 0 8192 4 gcc_trace.txt > gcc.6
./sim_cache 16 1024 1 16 8192 4 gcc_trace.txt > gcc.7

for i in 1 2 3 4 5 6 7
do

  echo "trace$i"
  diff -iw gcc.$i z_Resources/gcc.output$i.txt
done