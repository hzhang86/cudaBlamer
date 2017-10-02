#!/bin/bash
#SBATCH -t 0:15:0
#SBATCH --nodes=1
#SBATCH --exclusive
#SBATCH --partition=debug
#SBATCH --output=./job.output

echo "Start running Compilation script of AltParser!"

rm *.o -f
g++ -c -g  *.cpp -std=gnu++11  # there was -pg in three g++ cmds
g++ -c -g  altMain.cpp -std=gnu++11
g++ -g -o AltParser *.o -std=gnu++11

