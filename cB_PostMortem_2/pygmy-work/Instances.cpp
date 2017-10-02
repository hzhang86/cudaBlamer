#include "Instances.h"
#include "BlameProgram.h"
#include "BlameFunction.h"
#include "BlameModule.h"

using namespace std;
extern bool isForkStarWrapper(std::string name);
extern bool isTopMainFrame(std::string name);
extern bool forkInfoMatch(StackFrame &sf, Instance &inst);

void Instance::printInstance()
{
  vector<StackFrame>::iterator vec_SF_i;
  for (vec_SF_i = frames.begin(); vec_SF_i != frames.end(); vec_SF_i++) {
    if ((*vec_SF_i).lineNumber > 0){
      cout<<"At Frame "<<(*vec_SF_i).frameNumber<<" at line num "<<(*vec_SF_i).lineNumber;
      cout<<" in module "<<(*vec_SF_i).moduleName<<endl;
    }
    else {
      cout<<std::hex;
      cout<<"At Frame "<<(*vec_SF_i).frameNumber<<" at address "<<(*vec_SF_i).address<<endl;
      cout<<std::dec;
    }
  }
}

void Instance::printInstance_concise()
{
  if (frames.empty()) {
    stack_info<<"Woops, the frames of this instance is empty !"<<endl;
    return;
  }

  vector<StackFrame>::iterator vec_SF_i;
  for (vec_SF_i = frames.begin(); vec_SF_i != frames.end(); vec_SF_i++) {
    stack_info<<(*vec_SF_i).frameNumber<<" "<<(*vec_SF_i).lineNumber<<" "<<
      (*vec_SF_i).moduleName<<" ";
    stack_info<<std::hex<<(*vec_SF_i).address<<std::dec;
    stack_info<<" "<<(*vec_SF_i).frameName;
    if ((*vec_SF_i).frameName == "thread_begin") 
      stack_info<<" "<<(*vec_SF_i).task_id;
    stack_info<<endl;
  }
}


void Instance::removeRedundantFrames(ModuleHash &modules, string nodeName)
{
  stack_info<<"In removeRedundantFrames on "<<nodeName<<endl;
  vector<StackFrame>::iterator vec_SF_i, minusOne;
  vector<StackFrame> newFrames(frames);
  bool isBottomParsed = true;
  frames.clear();

  for (vec_SF_i=newFrames.begin(); vec_SF_i!=newFrames.end(); vec_SF_i++) {
    if (isBottomParsed == true) { 
      isBottomParsed = false; 
      continue;              
    }
    //delete coforall/wrapcoforall except the first one, and last real frame
    //from user code also deleted except there is recursion
    minusOne = vec_SF_i - 1;
    if ((*vec_SF_i).moduleName== (*minusOne).moduleName && 
        (*vec_SF_i).frameName==(*minusOne).frameName) {
        
      //Check if there is recursion 
      std::vector<VertexProps*>::iterator vec_vp_i;
      bool hasRecursion = false;
      //BlameFunction bf has to exist since it was checked in trimFrames before  
      BlameFunction *bf = modules[(*vec_SF_i).moduleName]->findLineRange((*vec_SF_i).lineNumber);
      for (vec_vp_i = bf->callNodes.begin(); vec_vp_i != bf->callNodes.end(); vec_vp_i++) {
        VertexProps *vp = *vec_vp_i;
        if (vp->declaredLine==(*vec_SF_i).lineNumber && vp->name==(*vec_SF_i).frameName) 
          hasRecursion = true;
      }
      //If no recursion, then we should remove this frame"
      if (!hasRecursion) 
        (*vec_SF_i).toRemove = true;
    }
    // We don't need to keep chpl_gen_main now        
    if ((*vec_SF_i).frameName == "chpl_gen_main")
      (*vec_SF_i).toRemove = true;
  }

  //pick all the valid frames and push_back to "frames" again
  for (vec_SF_i=newFrames.begin(); vec_SF_i!=newFrames.end(); vec_SF_i++) {
    if ((*vec_SF_i).toRemove == false) {
      StackFrame sf(*vec_SF_i);
      frames.push_back(sf);
    }
    else
      stack_info<<"Redundant frame :"<<(*vec_SF_i).frameName<<", delete frame# "
          <<(*vec_SF_i).frameNumber<<" with line# "<<(*vec_SF_i).lineNumber<<endl;
  }
  //thorougly free the memory of newFrames
  vector<StackFrame>().swap(newFrames); 
}
  


//Added by Hui 12/25/15: trim the stack trace from main thread again(NOT USED ANYMORE)
/*void Instance::secondTrim(ModuleHash &modules, string nodeName)
{
  stack_info<<"In secondTrim on "<<nodeName<<endl;
  vector<StackFrame>::iterator vec_SF_i;
  vector<StackFrame> newFrames(frames);
  bool isBottomParsed = true;
  frames.clear();
  
  for (vec_SF_i=newFrames.begin(); vec_SF_i!=newFrames.end(); vec_SF_i++) {
    if (isForkStarWrapper((*vec_SF_i).frameName))
      continue; //always keep the frame to the final glueStacks stage
    
    BlameModule *bm = NULL;
    bm = modules[(*vec_SF_i).moduleName];
    //BlameFunction *bf = bm->findLineRange((*vec_SF_i).lineNumber);
    BlameFunction *bf = bm->getFunction((*vec_SF_i).frameName);
    if (bf==NULL) {
      stack_info<<"Weird: bf should exist for "<<(*vec_SF_i).frameName<<endl;
      return;
    }
    else {
      if (isBottomParsed == true) { //Only valid for compute instances, not for pre&fork
        isBottomParsed = false; //if it's first frame, then it doesn't
        continue;               //have to have callNodes, but later does
      }
      else {
        std::vector<VertexProps*>::iterator vec_vp_i;
        VertexProps *callNode = NULL;
        std::vector<VertexProps*> matchingCalls;
          
        for (vec_vp_i = bf->callNodes.begin(); vec_vp_i != bf->callNodes.end(); vec_vp_i++) {
          VertexProps *vp = *vec_vp_i;
          if (vp->declaredLine == (*vec_SF_i).lineNumber) {
            //just for test 
            stack_info<<"matching callNode: "<<vp->name<<endl;
            matchingCalls.push_back(vp);
          }
        }
        // we only need to check middle frames mapped to "coforall_fn/wrapcoforall_fn"
        if (matchingCalls.size() >= 1) { //exclude frame that maps to "forall/coforall" loop lines
          callNode = NULL;              //changed >1 to >=1 by Hui 03/24/16: as long as the callnode doesn't match the ...
          stack_info<<">= one call node at that line number"<<std::endl;    //previous frame, we need to remove it ..
            // figure out which call is appropriate                         //usually it's a Chapel inner func call
          vector<StackFrame>::iterator minusOne = vec_SF_i - 1;
          BlameModule *bmCheck = modules[(*minusOne).moduleName];
          if (bmCheck == NULL) {
            stack_info<<"BM of previous frame is null ! delete frame "<<(*vec_SF_i).frameNumber<<endl;
            (*vec_SF_i).toRemove = true;
          }
          else {
            BlameFunction *bfCheck = bmCheck->findLineRange((*minusOne).lineNumber);
            if (bfCheck == NULL) {
              stack_info<<"BF of previous frame is null ! delete frame "<<(*vec_SF_i).frameNumber<<endl;
              (*vec_SF_i).toRemove = true;
            }
            else {
              std::vector<VertexProps *>::iterator vec_vp_i2;
              for (vec_vp_i2 = matchingCalls.begin(); vec_vp_i2 != matchingCalls.end(); vec_vp_i2++) {
                VertexProps *vpCheck = *vec_vp_i2;
                // Look for subsets since vpCheck will have the line number concatenated
                if (vpCheck->name.find(bfCheck->getName()) != std::string::npos)
                  callNode = vpCheck;
              }
          
              if (callNode == NULL) {
                  stack_info<<"No matching call nodes from multiple matches, delete frame "<<(*vec_SF_i).frameNumber<<endl;
                  (*vec_SF_i).toRemove = true;
              }
            }
          }
        }
      }
    }
  }

  //pick all the valid frames and push_back to "frames" again
  for (vec_SF_i=newFrames.begin(); vec_SF_i!=newFrames.end(); vec_SF_i++) {
    if ((*vec_SF_i).toRemove == false) {
        StackFrame sf(*vec_SF_i);
        frames.push_back(sf);
    }
  }
  //thorougly free the memory of newFrames
  vector<StackFrame>().swap(newFrames); 
}*/


//Added by Hui 12/20/15 : set all invalid frames's toRemove tag to TRUE in an instance
void Instance::trimFrames(ModuleHash &modules, int InstanceNum, string nodeName)
{
  if(this->instType == COMPUTE_INST)
    stack_info<<"Triming compute instance "<<InstanceNum<<" on "<<nodeName<<endl;
  else if(this->instType == PRESPAWN_INST)
    stack_info<<"Triming preSpawn instance "<<InstanceNum<<" on "<<nodeName<<endl;
  else stack_info<<"What am I triming ? instType="<<this->instType<<endl;

  vector<StackFrame>::iterator vec_SF_i;
  vector<StackFrame> newFrames(frames);
  bool isBottomParsed = true;
  frames.clear(); //clear up this::frames after we copy everything to newFrames
  
  for (vec_SF_i=newFrames.begin(); vec_SF_i!=newFrames.end(); vec_SF_i++) {
    if ((*vec_SF_i).lineNumber <= 0) {
      stack_info<<"LineNum <=0, delete frame "<<(*vec_SF_i).frameNumber<<endl;
      (*vec_SF_i).toRemove = true;
    }
    else { //lineNumber >0
      BlameModule *bm = NULL;
      if((*vec_SF_i).moduleName.empty()==false);
        bm = modules[(*vec_SF_i).moduleName];
      if (bm == NULL) {
        if ((*vec_SF_i).frameName != "thread_begin") {
          (*vec_SF_i).toRemove = true; //delete it if it's not fork*wrapper frame
          stack_info<<"BM is NULL and it's not thread_begin, delete frame "<<(*vec_SF_i).frameNumber<<endl;
        }
        else continue; //we always keep the frame with "thread_begin" name
      }
      else { //bm != NULL, it's from user code
        // Use the combination of the module and the line number to determine the function & compare with frameName
        string fName = (*vec_SF_i).frameName;
        BlameFunction *bf = bm->getFunction(fName);
        if (bf == NULL) { //would remove frames corresponding to wrapcoforall/wrapon,etc.
          if (fName == "chpl_gen_main") { // we keep it for now even it doesn't have bf
            isMainThread = true;
            continue;
          }
          else {
            BlameFunction *bf2 = bm->findLineRange((*vec_SF_i).lineNumber); //We still use this for single-locale
            if (bf2 && bf2->getName()!=fName && fName.find("coforall_fn_chpl")!=std::string::npos) {
              stack_info<<"We are changing frameName: "<<(*vec_SF_i).frameName<<" to "<<bf2->getName()<<endl;
              (*vec_SF_i).frameName = bf2->getName();
              continue;
            }
            else {
              stack_info<<"BF is NULL, delete frame #"<<(*vec_SF_i).frameNumber<<" "<<fName<<endl;
              (*vec_SF_i).toRemove = true;
            }
          }
        }
        else { //we found bf using frameName
          if (fName == "chpl_user_main") {
            if (bf->getBLineNum()==(*vec_SF_i).lineNumber) { //probably won't need this anymore 
              stack_info<<"Frame can't be main while the ln is BLineNum (main thread), delete frame "<<(*vec_SF_i).frameNumber<<endl;
              (*vec_SF_i).toRemove = true;
            }
            isMainThread = true;//we know it's from the main thread if this inst has a frame of chpl_user_main
          }
          else {
            if (isBottomParsed == true) {
              isBottomParsed = false; //if it's first frame, then it doesn't
              continue;               //have to have callNodes, but later does
            }
            else {
              std::vector<VertexProps*>::iterator vec_vp_i;
              VertexProps *callNode = NULL;
              std::vector<VertexProps*> matchingCalls;
              
              for (vec_vp_i=bf->callNodes.begin(); vec_vp_i!=bf->callNodes.end(); vec_vp_i++) {
                VertexProps *vp = *vec_vp_i;
                if (vp->declaredLine == (*vec_SF_i).lineNumber)
                  matchingCalls.push_back(vp);
              }
              if (matchingCalls.size() == 0) {
                stack_info<<"There's no matching callNode in this line, delete frame "
                          <<(*vec_SF_i).frameNumber<<endl;
                (*vec_SF_i).toRemove = true;
              }
            }
          }
        }
      }
    }
  }

  //pick all the valid frames and push_back to "frames" again
  for (vec_SF_i=newFrames.begin(); vec_SF_i!=newFrames.end(); vec_SF_i++) {
    if ((*vec_SF_i).toRemove == false) {
        StackFrame sf(*vec_SF_i); //using c++ struct implicit constructor, tested work on these cases
        frames.push_back(sf);
    }
  }

  //thorougly free the memory of newFrames
  vector<StackFrame>().swap(newFrames); //here is why this works: 
                //http://prateekvjoshi.com/2013/10/20/c-vector-memory-release/
  // return if the frames is empty
  if (frames.size() < 1) {
    stack_info<<"After trimFrames, Instance #"<<InstanceNum<<" on "<<nodeName<<" is Empty."<<endl;
    stack_info<<endl;
    return;
  }
  // This is rare case but if we only have one coforall/wrapcoforall frame, we blame 
  // it back to user func using the old 'findLineRange' function
  else if (frames.size() == 1) { 
    if ((*frames.begin()).frameName.find("coforall_fn_chpl") != std::string::npos) {
      BlameModule *bm = modules[(*frames.begin()).moduleName];
      BlameFunction *bf = bm->findLineRange((*frames.begin()).lineNumber);
      if (bf) 
        (*frames.begin()).frameName = bf->getName();
    }
  }

  StackFrame &vec_SF_r = frames.back();
  if (vec_SF_r.frameName != "thread_begin" && isMainThread == false)
    stack_info<<"Weird: #"<<InstanceNum<<" last frame: "<<vec_SF_r.frameName<<endl;
  // print the new instance
  stack_info<<"After trimFrames, Instance #"<<InstanceNum<<" on "<<nodeName<<endl;
  printInstance_concise();
  stack_info<<endl; //for separation between each instance
}

void Instance::handleInstance(ModuleHash &modules, std::ostream &O, int InstanceNum, bool verbose)
{   
    cout<<"\nIn handleInstance for inst#"<<InstanceNum<<endl;
  
  // This is true in the case where we're at the last stack frame that can be parsed
  // Here, "bottom" comes from the growing style of stack since it goes down 
  //if main->foo->bar, then bar is the bottom stackframe
  bool isBottomParsed = true;
  vector<StackFrame>::iterator vec_SF_i;
  for (vec_SF_i = frames.begin(); vec_SF_i != frames.end(); vec_SF_i++) {
    if ((*vec_SF_i).lineNumber > 0 && (*vec_SF_i).toRemove == false) {
    // Get the module from the debugging information
    BlameModule *bm = modules[(*vec_SF_i).moduleName];   
      if (bm) {
        //BlameFunction *bf = bm->findLineRange((*vec_SF_i).lineNumber);
        BlameFunction *bf = bm->getFunction((*vec_SF_i).frameName); //now coforal should be found since name was changed 
        if (bf) { //after previous trim&glue, it should come down here without trouble
          std::set<VertexProps *> blamedParams;
          if (bf->getBlamePoint() > 0) {
            cout<<"In function "<<bf->getName()<<" is BP="<<bf->getBlamePoint()<<endl;
            //blamePoint can be 0,1,2, why always set true(1) here ??
            bf->resolveLineNum(frames, modules, vec_SF_i, blamedParams, 
                    true, isBottomParsed, NULL, O);
            return; //once the resolveLineNum returns from a frame, then the whole instance done
          }
          else {
            cout<<"In function "<<bf->getName()<<" is not BP"<<endl;
            bf->resolveLineNum(frames, modules, vec_SF_i, blamedParams, 
                    false, isBottomParsed, NULL, O);
            return; //once the resolveLineNum returns from a frame, the whole instance is done
          }
          // TODO: Delete isBottomParsed here and everything below since we won't come here
          cout<<"THIS SHOULD NEVER EVER GETS PRINTED OUT"<<endl;
          isBottomParsed = false;
        }
        else {
          if (isBottomParsed == false) {
            cerr<<"Break in stack debugging info, BF is NULL"<<endl;
            isBottomParsed = true;
          }
          cerr<<"Error: BF NULL-- At Frame "<<(*vec_SF_i).frameNumber<<" "<<
            (*vec_SF_i).lineNumber<<" "<<(*vec_SF_i).moduleName<<" "<<std::hex<<
            (*vec_SF_i).address<<std::dec<<(*vec_SF_i).frameName<<endl;
        }
      }
      else {
        if (isBottomParsed == false) {
          cerr<<"Break in stack debugging info, BM is NULL"<<endl;
          isBottomParsed = true;
        }
        
        cerr<<"Error: BM NULL-- At Frame "<<(*vec_SF_i).frameNumber<<" "<<
          (*vec_SF_i).lineNumber<<" "<<(*vec_SF_i).moduleName<<" "<<std::hex<<
          (*vec_SF_i).address<<std::dec<<" "<<(*vec_SF_i).frameName<<endl;
      }
    }
    else
      cerr<<"Error: LINE#<=0-- At Frame "<<(*vec_SF_i).frameNumber<<" "<<
        (*vec_SF_i).lineNumber<<" "<<(*vec_SF_i).moduleName<<" "<<std::hex<<
        (*vec_SF_i).address<<std::dec<<(*vec_SF_i).frameName<<endl;
  }
}



