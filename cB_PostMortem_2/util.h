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

/* This file is not used currently
 * just keep it for future use
*/
#ifndef _UTIL_H
#define _UTIL_H

#include <iostream>
#include <string.h>
#include "Instances.h"


bool isForkStarWrapper(std::string name)
{
  if (name == "fork_wrapper" || name == "fork_nb_wrapper" ||
      name == "fork_large_wrapper" || name == "fork_nb_large_wrapper")
    return true;
  else
    return false;
}

bool isTopMainFrame(std::string name)
{
  if (name == "chpl_user_main" || name == "chpl_gen_main")
    return true;
  else
    return false;
}

bool forkInfoMatch(StackFrame &sf, Instance &inst)
{
  if (sf.info.callerNode == inst.info.callerNode &&
      sf.info.calleeNode == inst.info.calleeNode &&
      sf.info.fid == inst.info.fid &&
      sf.info.fork_num == inst.info.fork_num)
    return true;
  else
    return false;
}

#endif //_UTIL_H
