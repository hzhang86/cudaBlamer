#!/bin/bash
# g++ -c -g -I/hivehomes/rutar/boost_1_36_0/ -I${DYNINST_ROOT}/dyninst -I${DYNINST_ROOT}/dyninst/dyninstAPI/h -I${DYNINST_ROOT}/dyninst/external -I/${DYNINST_ROOT}/dyninst/instructionAPI/h -I${DYNINST_ROOT}/dyninst/symtabAPI/h -I${DYNINST_ROOT}/dyninst/dynutil/h addParser.C

#g++ -L${DYNINST_ROOT}/i386-unknown-linux2.4/lib -ldyninstAPI -lsymtabAPI -linstructionAPI -lcommon -ldl  -o addParser addParser.o

g++ -c -g -I$DYNINST_INSTALL/include addParser.cpp -std=gnu++11
echo "Finish compiling"
g++ -L$DYNINST_INSTALL/lib -o addParser addParser.o -ldyninstAPI -std=gnu++11 #-lsymtabAPI -linstructionAPI -lparseAPI -lcommon -ldl -ldwarf
echo "Finish linking"
