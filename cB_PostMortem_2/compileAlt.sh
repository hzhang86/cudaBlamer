#!/bin/bash
rm *.o -f
g++ -c -g  *.cpp -std=gnu++11  # there was -pg in three g++ cmds
g++ -c -g  altMain.cpp -std=gnu++11
g++ -g -o AltParser *.o -std=gnu++11
