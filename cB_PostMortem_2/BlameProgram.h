/*
 *  Copyright 2014-2017 Hui Zhang
 *  Previous contribution by Nick Rutar 
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

#ifndef BLAME_PROGRAM_H
#define BLAME_PROGRAM_H 

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include <set>

#include "Instances.h"
/*
#ifdef __GNUC__
#include <ext/hash_map>
#else
#include <hash_map>
#endif
*/
#include "BlameModule.h"
#include "BlameStruct.h"

////added by Hui///////
#define HUI_CHPL
//////////////////////

using namespace std;


class BlameFunction;
class BlameModule;


class BlameProgram
{
 public:  
  void parseProgram();
	
	
	// FOR Sample Converger
	void parseProgram_SC();
	////////
	
	void parseSideEffects();
	void printSideEffects();
	
	
	void parseStructs();
 // bool isVerbose();
  void parseConfigFile(const char * path);
	
	void grabUsedModulesFromDir();
	void grabUsedModules(std::string traceName, std::string nodeName);

  void addFunction(BlameFunction * bf);
  
	void printParsed(std::ostream &O);
	void printStructs(std::ostream &O);
	
	void addImplicitBlamePoints();
	void addImplicitBlamePoint(const char * checkName);
    
    void resolvePidsFromPPAs(void);
	
	void calcRecursiveSEAliases();


	
  BlameModule * findOrCreateModule(const char *);
	BlameFunction * getOrCreateBlameFunction(std::string funcName, std::string moduleName,std::string modulePathName);
	
	// goes through all the functions and calculates the side
	//  effects for all of them
	void calcSideEffects();
	StructHash blameStructs;
    ModuleHash blameModules;
	FunctionHash blameFunctions;
	std::set<std::string> sampledModules;
	
	
	////////// ALIAS ONLY ///////////
	void parseProgram_OA();
	void parseConfigFile_OA(const char * path);
	void parseLoops_OA();

	///////////////////////////////////

  
 private:
	std::vector<std::string> blameExportFiles;
	std::vector<std::string> blameStructFiles;
	std::vector<std::string> blameSEFiles;
	std::vector<std::string> blameLoopFiles;
  //std::string boolVerbose;  
};

#endif
