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
        preInstanceHash &pre_instance_table, globalForkInstMap &gForkInsts);
static void populateCompSamples(vector<Instance> &comp_instances, string traceName, string nodeName);
static void populatePreSamples(InstanceHash &pre_instances, string traceName, string nodeName);
static void populateForkSamples(vector<Instance> &fork_instances, string traceName, string nodeName);
static void populateSamplesFromDirs(compInstanceHash &comp_instance_table, 
        preInstanceHash &pre_instance_table, forkInstanceHash &fork_instance_table);
//the following util function will be used globablly
//====================vv========================================//
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
        preInstanceHash &pre_instance_table, globalForkInstMap &gForkInsts)
{

  stack_info<<"In glueStackTraces for instance "<<InstNum<<" on "<<node<<endl;
  // We use a while-loop to glue stack traces instead of recursion: jumping
  // between Fork and Pre until we make it to "main" or nowhere
  string workingNode = node; //workingNode will change as fork_inst glued
  while (!inst.isMainThread && (inst.needGlueFork || inst.needGluePre)) {
    if (inst.needGlueFork && inst.needGluePre) {
      stack_info<<"Error: needGlueFork and needGluePre can't be both TRUE! inst#"
        <<InstNum<<" on "<<node<<endl;
      break;
    }
    
    // needGluePre=true, needGlueFork==false
    else if (inst.needGluePre) {
      unsigned long TID = inst.frames.back().task_id;
      stack_info<<"Glueing inst&pre_inst taskID: "<<TID<<" on "<<workingNode<<endl;
      // Double check whether the end frame is thread_begin
      if (inst.frames.back().frameName != "thread_begin") {
        stack_info<<"Error: the end frame is NOT thread_begin while needGluePre==TRUE"<<endl;
        break;
      }
      
      InstanceHash &preForNode = pre_instance_table[workingNode];
      if (preForNode.count(TID) == 0) {
        stack_info<<"Error: worker thread has taskID "<<TID<<
          " not found in preSpawn on "<<workingNode<<endl;
        break;
      }
      
      Instance &pre_inst = preForNode[TID];
      if (!pre_inst.frames.empty()) {
        inst.frames.pop_back(); //delete the original "thread_begin" frame
        inst.frames.insert(inst.frames.end(), pre_inst.frames.begin(), 
                                            pre_inst.frames.end());
        //Very Important! we need to reset the new "inst" attributes
        if (isTopMainFrame(inst.frames.back().frameName)) {
          inst.isMainThread = true;
          inst.needGluePre = false;
          inst.needGlueFork = false;
        }
        else if (isForkStarWrapper(inst.frames.back().frameName)) {
          inst.isMainThread = false;
          inst.needGluePre = false;
          inst.needGlueFork = true;
        }
        else if (inst.frames.back().frameName == "thread_begin") {
          inst.isMainThread = false;
          inst.needGluePre = true;
          inst.needGlueFork = false;
        }
        else { //We should break out the loop but we need to reset before that
          inst.isMainThread = false;
          inst.needGluePre = false;
          inst.needGlueFork = false;
        }
      }
      else {
        stack_info<<"Whoops: the pre_inst is empty for TID: "<<TID<<endl;
        break;
      }
    }

    // needGluePre=false, needGlueFork==true
    else if (inst.needGlueFork) {
      fork_t fork_key = inst.frames.back().info;
      stack_info<<"Glueing inst&fork_inst with fork_key: "<<fork_key.callerNode<<" "<<
        fork_key.calleeNode<<" "<<fork_key.fid<<" "<<fork_key.fork_num<<" on "<<workingNode<<endl;
      // Double check whether the end frame is fork*wrapper
      if (!isForkStarWrapper(inst.frames.back().frameName)) {
        stack_info<<"Error: the end frame is NOT fork_*_wrapper while needGlueFork==TRUE"<<endl;
        break;
      }

      if (gForkInsts.count(fork_key) == 0) {
        stack_info<<"Error: No such fork_key found: "<<fork_key.callerNode<<" "
          <<fork_key.calleeNode<<" "<<fork_key.fid<<" "<<fork_key.fork_num<<endl;
        break;
      }
       
      Instance &fork_inst = gForkInsts[fork_key];
      if (!fork_inst.frames.empty()) {
        inst.frames.pop_back(); //delete the original "fork*wrapper" frame
        inst.frames.insert(inst.frames.end(), fork_inst.frames.begin(), 
                                            fork_inst.frames.end());
        //Very Important! we need to reset the new "inst" attributes
        if (isTopMainFrame(inst.frames.back().frameName)) {
          inst.isMainThread = true;
          inst.needGluePre = false;
          inst.needGlueFork = false;
        }
        else if (isForkStarWrapper(inst.frames.back().frameName)) {
          inst.isMainThread = false;
          inst.needGluePre = false;
          inst.needGlueFork = true;
        }
        else if (inst.frames.back().frameName == "thread_begin") {
          inst.isMainThread = false;
          inst.needGluePre = true;
          inst.needGlueFork = false;
        }
        else { //We should break out the loop but we need to reset before that
          inst.isMainThread = false;
          inst.needGluePre = false;
          inst.needGlueFork = false;
        }
      }
      else {
        stack_info<<"Whoops: the fork_inst is empty for fork_key: "<<fork_key.callerNode
          <<" "<<fork_key.calleeNode<<" "<<fork_key.fid<<" "<<fork_key.fork_num<<endl;
        break;
      }

      //Very Very Important! we need to switch node to work on!
      workingNode = nodeIDNameMap[fork_key.callerNode];
    }
  } //End of while-loop gluing, should end up in main or nowhere(toplevelframes)
 
  //Result check: not changing any attributes
  if (isTopMainFrame(inst.frames.back().frameName) && inst.isMainThread)
    stack_info<<"Got to main after glueStackTraces!"<<endl;
  else if (isForkStarWrapper(inst.frames.back().frameName) && inst.needGlueFork)
    stack_info<<"Got to fork*wrapper after glueStackTraces!"<<endl;
  else if (inst.frames.back().frameName == "thread_begin" && inst.needGluePre)
    stack_info<<"Got to thread_begin after glueStackTraces!"<<endl;
  else
    stack_info<<"Check:"<<inst.isMainThread<<" "<<inst.needGlueFork<<" "<<
      inst.needGluePre<<", last frame="<<inst.frames.back().frameName<<endl;

  stack_info<<"Finish glueStackTraces for instance "<<InstNum<<" on "<<node<<endl;
}


static void populateCompSamples(vector<Instance> &comp_instances, string traceName, string nodeName)
{
  ifstream ifs_comp(traceName);
  string line;

  if (ifs_comp.is_open()) {
    getline(ifs_comp, line);
    int numInstances = atoi(line.c_str());
    cout<<"Number of instances from "<<traceName<<" is "<<numInstances<<endl;
 
    for (int i = 0; i < numInstances; i++) {
      Instance inst;
      getline(ifs_comp, line);
      
      int numFrames;
      inst.instType = COMPUTE_INST;
      sscanf(line.c_str(), "%d %d", &numFrames, &(inst.taskID));
    
      for (int j=0; j<numFrames; j++) {
        StackFrame sf;
        getline(ifs_comp, line);
        stringstream ss(line);
    
        ss>>sf.frameNumber;
        ss>>sf.lineNumber;
        ss>>sf.moduleName;
        ss>>std::hex>>sf.address>>std::dec;
        ss>>sf.frameName;

        if (isForkStarWrapper(sf.frameName)) {
          if (std::count(line.begin(), line.end(), ' ') > 4) {//fork_t info exists
            ss>>sf.info.callerNode;
            ss>>sf.info.calleeNode;
            ss>>sf.info.fid;
            ss>>sf.info.fork_num;
          }
        }
        else if (sf.frameName == "thread_begin") {
          ss>>sf.task_id;
        }

        inst.frames.push_back(sf);
      }
    
      comp_instances.push_back(inst);
    }
    //Close the read file
    ifs_comp.close();
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
      sscanf(line.c_str(), "%d %d", &numFrames, &(inst.taskID));
    
      for (int j=0; j<numFrames; j++) {
        StackFrame sf;
        getline(ifs_pre, line);
        stringstream ss(line);
    
        ss>>sf.frameNumber;
        ss>>sf.lineNumber;
        ss>>sf.moduleName;
        ss>>std::hex>>sf.address>>std::dec;
        ss>>sf.frameName;

        if (isForkStarWrapper(sf.frameName)) {
          if (std::count(line.begin(), line.end(), ' ') > 4) {//fork_t info exists
            ss>>sf.info.callerNode;
            ss>>sf.info.calleeNode;
            ss>>sf.info.fid;
            ss>>sf.info.fork_num;
          }
        }
        else if (sf.frameName == "thread_begin") {
          ss>>sf.task_id;
        }

        inst.frames.push_back(sf);
      }
    
      if (pre_instances.count(inst.taskID) == 0)
        pre_instances[inst.taskID] = inst;
      else
        cerr<<"Error: more than 1 pre-instance mapped to the same taskID "
          <<inst.taskID<<" in "<<traceName<<endl;
    }
    //Close the read file
    ifs_pre.close();
  }
  else
    cerr<<"Error: open file "<<traceName<<" fail"<<endl;
}


static void populateForkSamples(vector<Instance> &fork_instances, string traceName, string nodeName)
{
  ifstream ifs_fork(traceName);
  string line;
  int IT;

  if (traceName.find("fork_nb") != string::npos) 
    IT = FORK_NB_INST;
  else if (traceName.find("fork_fast") != string::npos)
    IT = FORK_FAST_INST;
  else
    IT = FORK_INST; // simply 'fork'

  if (ifs_fork.is_open()) {
    getline(ifs_fork, line);
    int numInstances = atoi(line.c_str());
    cout<<"Number of fork*_instances from "<<traceName<<" is "<<numInstances<<endl;
 
    for (int i = 0; i < numInstances; i++) {
      Instance inst;
      getline(ifs_fork, line);
      
      int numFrames;
      inst.instType = IT;
      sscanf(line.c_str(), "%d %d %d %d %d %d", &numFrames, &(inst.taskID),
              &(inst.info.callerNode), &(inst.info.calleeNode), 
              &(inst.info.fid), &(inst.info.fork_num));
      
      for (int j=0; j<numFrames; j++) {
        StackFrame sf;
        getline(ifs_fork, line);
        stringstream ss(line);
    
        ss>>sf.frameNumber;
        ss>>sf.lineNumber;
        ss>>sf.moduleName;
        ss>>std::hex>>sf.address>>std::dec;
        ss>>sf.frameName;

        if (isForkStarWrapper(sf.frameName)) {
          if (std::count(line.begin(), line.end(), ' ') > 4) {//fork_t info exists
            ss>>sf.info.callerNode;
            ss>>sf.info.calleeNode;
            ss>>sf.info.fid;
            ss>>sf.info.fork_num;
          }
        }
        else if (sf.frameName == "thread_begin") {
          ss>>sf.task_id;
        }
          
        inst.frames.push_back(sf);
      }
    
      fork_instances.push_back(inst);
      
      //fill in the nodeIDNameMap once for all
      if (i==0) {
        if(nodeIDNameMap.count(inst.info.callerNode)==0)
          nodeIDNameMap[inst.info.callerNode] = nodeName; 
        else {
          if (nodeIDNameMap[inst.info.callerNode] != nodeName)
            stack_info<<"Error: two compute nodes are mapped to same ID: "
                <<inst.info.callerNode<<" from "<<traceName<<endl;
        }
      }
    }
    //Close read file
    ifs_fork.close();
  }
  else
    cerr<<"Error: open file "<<traceName<<" fail"<<endl;
}


static void populateSamplesFromDirs(compInstanceHash &comp_instance_table, 
        preInstanceHash &pre_instance_table, forkInstanceHash &fork_instance_table)
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
      if ((ent->d_type == DT_REG) && 
          (strstr(ent->d_name, "Input-compute") != NULL)) {
        traceName = dirName + "/" + std::string(ent->d_name);
        pos = traceName.find("compute");
        nodeName = traceName.substr(pos);

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
          (strstr(ent->d_name, "Input-preSpawn") != NULL)) {
        traceName = dirName + "/" + std::string(ent->d_name);
        pos = traceName.find("compute");
        nodeName = traceName.substr(pos);

        InstanceHash pre_instances;
        populatePreSamples(pre_instances, traceName, nodeName);
        if (pre_instance_table.count(nodeName) == 0)
          pre_instance_table[nodeName] = pre_instances; //push this node's presample
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

  dirName = "FORK";
  if ((dir = opendir(dirName.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      if ((ent->d_type == DT_REG) && 
          (strstr(ent->d_name, "Input-fork") != NULL)) {
        traceName = dirName + "/" + std::string(ent->d_name);
        pos = traceName.find("compute");
        nodeName = traceName.substr(pos);
        // create a new instance vector if it's from new node
        if (fork_instance_table.count(nodeName) == 0) {
          vector<Instance> fork_instances;
          populateForkSamples(fork_instances, traceName, nodeName);
          fork_instance_table[nodeName] = fork_instances; 
        }
        else { //node's file existed before, for fork_nb, fork_fast files
          vector<Instance> &fork_instances = fork_instance_table[nodeName];
          populateForkSamples(fork_instances, traceName, nodeName);
        }
      }
      else if ((ent->d_type == DT_REG) && 
          (strstr(ent->d_name, "Input-compute") == NULL)) //avoid dir entries: .&..
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
  forkInstanceHash fork_instance_table; //Added by Hui 11/16/16
  globalForkInstMap gForkInsts; //Added by Hui 04/28/17
  // Import sample info from all Input- files
  populateSamplesFromDirs(comp_instance_table, pre_instance_table, 
                                                fork_instance_table);
  
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

  //Trim fork_instances frames 
  forkInstanceHash::iterator fh_i;
  vector<Instance>::iterator vec_I_i;
  //fh_i: <string nodeName, vector<Instance> fork_vec>
  for (fh_i=fork_instance_table.begin(); fh_i!=fork_instance_table.end(); fh_i++) {
    vector<Instance> &fork_vec = (*fh_i).second;
    int iCounter = 0;
    for (vec_I_i=fork_vec.begin(); vec_I_i!=fork_vec.end(); vec_I_i++) {
      (*vec_I_i).trimFrames(bp.blameModules, iCounter, (*fh_i).first);
      iCounter++;
    }
  }
  //Put everything from fork_instance_table to the global fork inst map
  for (fh_i=fork_instance_table.begin(); fh_i!=fork_instance_table.end(); fh_i++) {
    vector<Instance> &fork_vec = (*fh_i).second;
    for (vec_I_i=fork_vec.begin(); vec_I_i!=fork_vec.end(); vec_I_i++) {
      fork_t fork_key = (*vec_I_i).info;
      if (gForkInsts.count(fork_key) == 0)
        gForkInsts[fork_key] = *vec_I_i;
      else 
        cerr<<"Error: duplicate fork_key appear! "<<fork_key.callerNode<<" "
          <<fork_key.calleeNode<<" "<<fork_key.fid<<" "<<fork_key.fork_num<<endl;
    }
  }

  //Trim comp_instances frames
  compInstanceHash::iterator ch_i; //vec_I_i can be reused from above
  //fh_i: <string nodeName, vector<Instance> fork_vec>
  for (ch_i=comp_instance_table.begin(); ch_i!=comp_instance_table.end(); ch_i++) {
    vector<Instance> &comp_vec = (*ch_i).second;
    int iCounter = 0;
    for (vec_I_i=comp_vec.begin(); vec_I_i!=comp_vec.end(); vec_I_i++) {
      (*vec_I_i).trimFrames(bp.blameModules, iCounter, (*ch_i).first);
      iCounter++;
    }
  }

  //Now we need to glue those stack traces, we use a lazy way: 
  //Follow this order: comp->pre->fork->(fork/pre) until to top MAIN frame
  for (ch_i=comp_instance_table.begin(); ch_i!=comp_instance_table.end(); ch_i++) {
    vector<Instance> &comp_vec = (*ch_i).second;
    string node = (*ch_i).first;
    string outName = "PARSED_" + node;
    ofstream gOut(outName.c_str());
    int iCounter = 0;
    int numUnresolvedInsts = 0;
    int emptyInst = 0;

    for (vec_I_i=comp_vec.begin(); vec_I_i!=comp_vec.end(); vec_I_i++) {
      gOut<<"---INSTANCE "<<iCounter<<"  ---"<<std::endl;

      // glue stacktraces whenever necessary !
      if ((*vec_I_i).isMainThread == false) 
        glueStackTraces(node, iCounter, (*vec_I_i),
                        pre_instance_table, gForkInsts);

      // Result check, due to the BAD libunwind missing info !!!
      if ((*vec_I_i).frames.size()>1 && ((*vec_I_i).frames.back().frameName =="thread_begin"
            || isForkStarWrapper((*vec_I_i).frames.back().frameName)))
        numUnresolvedInsts++;

      // Polish stacktrace again: rm all wrap* functions such as wrapcoforall_fn_chpl
      (*vec_I_i).removeWrapFrames(node, iCounter);

      // Result check, invalid samples end up in polling task
      if ((*vec_I_i).frames.empty())
        emptyInst++;

      // concise_print the final "perfect" stacktrace for each compute sample
      stack_info<<"NOW final stacktrace for inst#"<<iCounter<<" on "<<node<<endl;
      (*vec_I_i).printInstance_concise();

      // handle the perfect instance now !
      (*vec_I_i).handleInstance(bp.blameModules, gOut, iCounter, verbose);
    
      gOut<<"$$$INSTANCE "<<iCounter<<"  $$$"<<std::endl; 
      iCounter++;
    }

    stack_info<<"#unresolved instances = "<<numUnresolvedInsts<<
        ", #emptyInst = "<<emptyInst<<" on "<<node<<endl;
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
