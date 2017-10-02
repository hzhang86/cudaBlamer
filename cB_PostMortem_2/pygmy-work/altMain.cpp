/*
 *  altMain.cpp
 *  
 *
 *  Created by Hui Zhang on 11/10/16.
 *  Copyright 2016 __MyCompanyName__. All rights reserved.
 *
 */
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>
#include <sys/time.h>
#include <algorithm>
#include <dirent.h> // for interating the directory

#include "BlameProgram.h"
#include "BlameFunction.h"
#include "BlameModule.h"
#include "Instances.h"

using namespace std;

//Hui for test: output file for finer stacktraces
ofstream stack_info;
nodeHash nodeIDNameMap; //Initialized in populateForkSamples

// some forward-declarations
static void glueStackTraces(string node, int InstNum, Instance &inst, 
        preInstanceHash &pre_instance_table);
static void populateCompSamples(vector<Instance> &comp_instances, string traceName, string nodeName);
static void populatePreSamples(InstanceHash &pre_instances, string traceName, string nodeName);
static void populateSamplesFromDirs(compInstanceHash &comp_instance_table, preInstanceHash &pre_instance_table);

//the following util function will be used globablly
//====================vv========================================//
bool isTopMainFrame(std::string name)
{
  if (name == "chpl_user_main" || name == "chpl_gen_main")
    return true;
  else
    return false;
}
//========================^^===============================//

void my_timestamp()
{
  time_t ltime; 
  ltime = time(NULL);
  struct timeval detail_time;
  gettimeofday(&detail_time,NULL);
   
  fprintf(stderr, "%s ", asctime( localtime(&ltime) ) );
  fprintf(stderr, "%d\n", detail_time.tv_usec /1000);
}


static void glueStackTraces(string node, int InstNum, Instance &inst, 
                                    preInstanceHash &pre_instance_table)
{
  stack_info<<"In glueStackTraces for instance "<<InstNum<<" on "<<node<<endl;
  if (inst.frames.back().frameName != "thread_begin") {
    stack_info<<"Error: the last frame isn't main nor thread_begin; it's "
        <<inst.frames.back().frameName<<endl;
    return;
  }
  else if (inst.frames.back().task_id <=0) {
    stack_info<<"Error: thread_begin doesn't have task_id >0, it's "
        <<inst.frames.back().task_id<<endl;
    return;
  }

  //Find the last frame("thread_begin") and locate the pre-inst with its task_id
  InstanceHash &preForNode = pre_instance_table[node];
  StackFrame &lastFrame = inst.frames.back();  
  Instance &pre_inst = preForNode[lastFrame.task_id];
  
  stack_info<<"Glueing inst&pre_inst TaskID: "<<lastFrame.task_id<<endl;
  if (!pre_inst.frames.empty()) {
    inst.frames.pop_back(); //delete previous 'thread_begin' frame of inst
    inst.frames.insert(inst.frames.end(), pre_inst.frames.begin(), 
                                            pre_inst.frames.end());
    if (isTopMainFrame(inst.frames.back().frameName)) {
      inst.isMainThread = true;
      stack_info<<"Finish glueStackTraces for inst "<<InstNum<<" to main"<<endl;
      return;
    }
    
    else if (inst.frames.back().frameName == "thread_begin") {
      stack_info<<"Continue glueStackTraces for inst "<<InstNum<<endl;
      //We need to recursively call this function if inst is still not main
      glueStackTraces(node, InstNum, inst, pre_instance_table);
    }
  }
  else  
    stack_info<<"Corresponding pre_inst is empty TaskID: "<<lastFrame.task_id<<endl;
}


static void populateCompSamples(vector<Instance> &comp_instances, string traceName, string nodeName)
{
  ifstream ifs_comp(traceName);
  string line;

  if (ifs_comp.is_open()) {
    getline(ifs_comp, line);
    int rawNumInst = atoi(line.c_str());
    cout<<"Raw number of instances from "<<traceName<<" is "<<rawNumInst<<endl;
 
    for (int i = 0; i < rawNumInst; i++) {
      Instance inst;
      getline(ifs_comp, line);
      bool fromGEST = false; //whether the inst is from getEarlyStackTrace
      char buffer[20]; //hold inst#, no use

      int numFrames;
      inst.instType = COMPUTE_INST;
      sscanf(line.c_str(), "%d %lu %s", &numFrames, &(inst.taskID), buffer); //taskID will always be 0
    
      for (int j=0; j<numFrames; j++) {
        StackFrame sf;
        getline(ifs_comp, line);
        stringstream ss(line);
    
        ss>>sf.frameNumber;
        ss>>sf.lineNumber;
        ss>>sf.moduleName;
        ss>>std::hex>>sf.address>>std::dec;
        ss>>sf.frameName;

        if (sf.frameName == "thread_begin") 
          ss>>sf.task_id;
        inst.frames.push_back(sf);
      }
    
      comp_instances.push_back(inst);
    }
  }
  else
    cerr<<"Error: open file "<<traceName<<" fail"<<endl;
}


static void populatePreSamples(InstanceHash &pre_instances, string traceName, string nodeName)
{
  ifstream ifs_pre(traceName);
  string line;

  if (ifs_pre.is_open()) {
    getline(ifs_pre, line);
    int numInstances = atoi(line.c_str());
    cout<<"Number of pre_instances from "<<traceName<<" is "<<numInstances<<endl;
 
    for (int i = 0; i < numInstances; i++) {
      Instance inst;
      getline(ifs_pre, line);
      
      int numFrames;
      inst.instType = PRESPAWN_INST;
      sscanf(line.c_str(), "%d %lu", &numFrames, &(inst.taskID));
    
      for (int j=0; j<numFrames; j++) {
        StackFrame sf;
        getline(ifs_pre, line);
        stringstream ss(line);
    
        ss>>sf.frameNumber;
        ss>>sf.lineNumber;
        ss>>sf.moduleName;
        ss>>std::hex>>sf.address>>std::dec;
        ss>>sf.frameName;

        if (sf.frameName == "thread_begin") 
          ss>>sf.task_id;
          
        inst.frames.push_back(sf);
      }
    
      if (pre_instances.count(inst.taskID) == 0)
        pre_instances[inst.taskID] = inst;
      else
        cerr<<"Error: more than 1 pre-instance mapped to the same taskID "
          <<inst.taskID<<" in "<<traceName<<endl;
    }
  }
  else
    cerr<<"Error: open file "<<traceName<<" fail"<<endl;
}


static void populateSamplesFromDirs(compInstanceHash &comp_instance_table, 
                                        preInstanceHash &pre_instance_table)
{

  DIR *dir;
  struct dirent *ent;
  string traceName;
  string dirName;
  string nodeName;
  size_t pos;

  dirName = "COMPUTE";
  if ((dir = opendir(dirName.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      if ((ent->d_type == DT_REG) && (strstr(ent->d_name, "Input-") != NULL)) {
        traceName = dirName + "/" + std::string(ent->d_name);
        pos = traceName.find("Input-");
        nodeName = traceName.substr(pos+6);

        vector<Instance> comp_instances;
        populateCompSamples(comp_instances, traceName, nodeName);
        if (comp_instance_table.count(nodeName) == 0)
          comp_instance_table[nodeName] = comp_instances; //push this node's sample
                                                        //info to the table
        else
          cerr<<"Error: This file was populated before: "<<traceName<<endl;
      }
      else if ((ent->d_type == DT_REG) && 
          (strstr(ent->d_name, "Input-compute") == NULL)) //avoid dir entries: .&..
        cerr<<"Error: there is a unmatching file "<<ent->d_name<<" in "<<dirName<<endl;
    }
    closedir(dir);
  } 
  else 
    cerr<<"Error: couldn't open the directory "<<dirName<<endl;

  dirName = "PRESPAWN";
  if ((dir = opendir(dirName.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      if ((ent->d_type == DT_REG) && 
          (strstr(ent->d_name, "Input-preSpawn-") != NULL)) {
        traceName = dirName + "/" + std::string(ent->d_name);
        pos = traceName.find("Input-preSpawn-");
        nodeName = traceName.substr(pos+15);

        InstanceHash pre_instances;
        populatePreSamples(pre_instances, traceName, nodeName);
        if (pre_instance_table.count(nodeName) == 0)
          pre_instance_table[nodeName] = pre_instances; //push this node's presample
                                                        //info to the table
        else
          cerr<<"Error: This file was populated before: "<<traceName<<endl;
      }
      else if ((ent->d_type == DT_REG) && 
          (strstr(ent->d_name, "Input-preSpawn-") == NULL)) //avoid dir entries: .&..
        cerr<<"Error: there is a unmatching file "<<ent->d_name<<" in "<<dirName<<endl;
    }
    closedir(dir);
  } 
  else 
    cerr<<"Error: couldn't open the directory "<<dirName<<endl;


  cout<<"Done populating all Input- files"<<endl;
}


// <altMain> <exe Name(not used)> <config file name> 
int main(int argc, char** argv)
{ 
  if (argc <3){
      std::cerr<<"Wrong Number of Arguments! "<<argc<<std::endl;
      exit(0);
  }
  
  bool verbose = false;
  
  fprintf(stderr,"START - ");
  my_timestamp();
  
  BlameProgram bp;
  bp.parseConfigFile(argv[2]);
  
  //std::cout<<"Parsing structs "<<std::endl;
  bp.parseStructs();
  //std::cout<<"Printing structs"<<std::endl;
  //bp.printStructs(std::cout);
  
  //std::cout<<"Parsing side effects "<<std::endl;
  bp.parseSideEffects();
  
  //std::cout<<"Printing side effects(PRE)"<<std::endl;
  //bp.printSideEffects();
  
  //std::cout<<"Calc Rec side effects"<<std::endl;
  bp.calcRecursiveSEAliases();
  
  //std::cout<<"Printing side effects(POST)"<<std::endl;
  //bp.printSideEffects();
  
  //std::cout<<"Grab used modules "<<std::endl;
  bp.grabUsedModulesFromDir();
  
  //std::cout<<"Parsing program"<<std::endl;
  bp.parseProgram();
  
  //std::cout<<"Adding implicit blame points."<<std::endl;
  bp.addImplicitBlamePoints();
  
  //std::cout<<"Printing parsed output to 'output.txt'"<<std::endl;
  //ofstream outtie("output.txt");
  //bp.printParsed(outtie);
  

  //bp.calcSideEffects();
  //std::cout<<"Populating samples."<<std::endl;
  fprintf(stderr,"SAMPLES - ");
  my_timestamp();

  stack_info.open("stackDebug", ofstream::out);
  compInstanceHash  comp_instance_table;
  preInstanceHash  pre_instance_table; //Added by Hui 12/25/15

  // Import sample info from all Input- files
  populateSamplesFromDirs(comp_instance_table, pre_instance_table);
  
  int validInstances = 0;

  //Trim pre_instances frames first  
  preInstanceHash::iterator ph_i;
  InstanceHash::iterator ih_i;
  //ph_i: <string nodeName, InstanceHash pre_hash>
  for (ph_i=pre_instance_table.begin(); ph_i!=pre_instance_table.end(); ph_i++) {
    InstanceHash &pre_hash = (*ph_i).second;
    //ih_i: <int taskID, Instance inst>
    int iCounter = 0;
    for (ih_i=pre_hash.begin(); ih_i!=pre_hash.end(); ih_i++) {
      (*ih_i).second.trimFrames(bp.blameModules, iCounter, (*ph_i).first);
      iCounter++;
    }
  }

  //Trim comp_instances frames
  compInstanceHash::iterator ch_i; 
  vector<Instance>::iterator vec_I_i;
  for (ch_i=comp_instance_table.begin(); ch_i!=comp_instance_table.end(); ch_i++) {
    vector<Instance> &comp_vec = (*ch_i).second;
    int iCounter = 0;
    for (vec_I_i=comp_vec.begin(); vec_I_i!=comp_vec.end(); vec_I_i++) {
      (*vec_I_i).trimFrames(bp.blameModules, iCounter, (*ch_i).first);
      iCounter++;
    }
  }

  //Now we need to glue those stack traces, we use a lazy way: 
  //we keep searching for the pre stacktrace if last frame of the current 
  //inst is thread_begin with a task_id until we hit the top MAIN frame
  for (ch_i=comp_instance_table.begin(); ch_i!=comp_instance_table.end(); ch_i++) {
    vector<Instance> &comp_vec = (*ch_i).second;
    string node = (*ch_i).first;
    string outName = "PARSED_" + node;
    ofstream gOut(outName.c_str());
    int iCounter = 0;

    for (vec_I_i=comp_vec.begin(); vec_I_i!=comp_vec.end(); vec_I_i++) {
      //We should always see at least a thread_begin frame in the inst, otherwise it's broken
      if ((*vec_I_i).frames.empty()) {
        stack_info<<"Instance #"<<iCounter<<" is empty, ignore this instanace"<<endl;
        continue;
      }

      gOut<<"---INSTANCE "<<iCounter<<"  ---"<<std::endl;

      // Glue stacktraces whenever necessary !
      if ((*vec_I_i).isMainThread == false) 
        glueStackTraces(node, iCounter, (*vec_I_i), pre_instance_table);

      // Remove frames like chpl_gen_main, coforall/wrapcoforall
      (*vec_I_i).removeRedundantFrames(bp.blameModules, node);

      // concise_print the final "perfect" stacktrace for each compute sample
      stack_info<<"NOW final stacktrace for inst#"<<iCounter<<" on "<<node<<endl;
      (*vec_I_i).printInstance_concise();
      stack_info<<"=========================================================\n\n";

      // Handle the perfect instance now !
      (*vec_I_i).handleInstance(bp.blameModules, gOut, iCounter, verbose);
    
      gOut<<"$$$INSTANCE "<<iCounter<<"  $$$"<<std::endl; 
      iCounter++;
    }
  }
      
//    ////for testing/////////////////////
//    if((*vec_I_i).frames.size() !=0)
//      validInstances++;
//    //////////////////////////////////
//  
//  fprintf(stderr, "#total instances = %d, #validInstances = %d\n",iCounter,validInstances);
  fprintf(stderr,"DONE - ");
  my_timestamp();
 
}
