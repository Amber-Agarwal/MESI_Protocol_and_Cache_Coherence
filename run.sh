#!/bin/bash
clang++ cache.cpp
./a.out -t app1 > outputs/output1.txt
./a.out -t app2 > outputs/output2.txt
./a.out -t app3 > outputs/output3.txt
./a.out -t app4 -s 0 -E 1 > outputs/output4.txt
./a.out -t app5 > outputs/output5.txt
./a.out -t app6 > outputs/output6.txt
./a.out -t app7 > outputs/output7.txt
./a.out -t app8 > outputs/output8.txt
./a.out -t app9 > outputs/output9.txt