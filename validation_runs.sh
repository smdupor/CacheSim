#!/bin/bash
./sim_cache 16 1024 2 0 0 0 gcc_trace.txt > gcc.0
./sim_cache 16 1024 2 16 0 0 gcc_trace.txt > gcc.1
./sim_cache 16 1024 2 0 8192 4 gcc_trace.txt > gcc.2
./sim_cache 16 1024 2 16 8192 4 gcc_trace.txt > gcc.3
./sim_cache 16 1024 1 0 0 0 gcc_trace.txt > gcc.4
./sim_cache 16 1024 1 16 0 0 gcc_trace.txt > gcc.5
./sim_cache 16 1024 1 0 8192 4 gcc_trace.txt > gcc.6
./sim_cache 16 1024 1 16 8192 4 gcc_trace.txt > gcc.7

#Extra Validations
./sim_cache 32 1536 3 0 8192 8 gcc_trace.txt  > extra.1
./sim_cache 16 1024 64 0 0 0 gcc_trace.txt  > extra.2
./sim_cache 16 1024 2 0 0 0 perl_trace.txt  > extra.3
./sim_cache 16 1024 2 16 8192 4 vortex_trace.txt  > extra.4
./sim_cache 32 512 1 7 0 0 go_trace.txt   > extra.5

for i in 1 2 3 4 5 6 7
do

  echo "trace$i"
  diff -iw gcc.$i z_Resources/gcc.output$i.txt
done

for j in 1 2 3 4 5
do

  echo "extra$j"
  diff -iw extra.$j z_Resources/extras/extra.$j.val.txt
done
