/*
 *  Copyright 2014-2017 Hui Zhang
 *  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 *  This is the much simplified version of Instance.h
 *  compared to the one in BFC_Postmortem_2, since all we
 *  care about here is the stack frames info in each Instance.
 *  
 *   Created by Hui 08/07/16
 */

#ifndef INSTANCES_H
#define INSTANCES_H 

#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>

struct fork_t 
{
  int callerNode;
  int calleeNode;
  int fid;
  int fork_num;

  fork_t() : callerNode(-1),calleeNode(-1),fid(-1),fork_num(-1){}
  fork_t(int loc, int rem, int fID, int f_num) : 
      callerNode(loc),calleeNode(rem),fid(fID),fork_num(f_num){}

};

struct StackFrame
{
  int lineNumber;
  int frameNumber;
  std::string moduleName;
  unsigned long address;
  std::string frameName;
  fork_t info; //specifically for fork_*_wrapper frame
  unsigned long task_id = 0; //specifically for thread_begin frame
};

struct Instance
{
  std::vector<StackFrame> frames;
  int instNum; //denote which inst in this sample file
  unsigned long taskID = 0; //for preSpawn stacktraces
  fork_t info; //specifically for fork* samples
};

struct eqstr
{
  bool operator() (std::string s1, std::string s2) const {
	return s1 == s2;
  }

};

#endif
