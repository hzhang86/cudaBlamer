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

#include "BFC.h"
#include "ModuleBFC.h"
#include "NodeProps.h"
#include "FunctionBFC.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>

using namespace std;

ofstream struct_file;
//populate blame relationship from externFuncions
void importSharedBFC(const char *path, ExternFuncBFCHash &externFuncInformation)
{
  ifstream bI(path);
  if (bI.is_open()) {
    string line;

    while (getline(bI, line)) {
      string buf;
      string name;
      stringstream ss(line);

      ss >> buf;
      name = buf; //assign name of the func

      ExternFunctionBFC *ef = new ExternFunctionBFC(name);

      while (ss >> buf) {
        ef->paramNums.insert(atoi(buf.c_str()));
      }
      externFuncInformation[name] = ef;

      if (externFuncInformation.size()==externFuncInformation.max_size()) {
        cerr<<"Damn it! we met the max size of externFunc hash"<<endl;
        return;
      }

#ifdef DEBUG_EXTERNFUNC
      cerr<<"Importing externFunc: "<<name<<", paramNums:"<<endl;
      set<int>::iterator pn_i;
      for(pn_i = ef->paramNums.begin(); pn_i != ef->paramNums.end(); pn_i++)
        cerr<<*pn_i<<" ";
      cerr<<endl;
#endif
    }

    bI.close();
  }
  else
    cerr<<"Shared lib path doesn't exist!"<<endl;
}

//Helper function
void exportUserFuncsInfo(FuncSigHash &kFI, ostream &O)
{
    FuncSigHash::iterator fsh_i;
    for (fsh_i = kFI.begin(); fsh_i != kFI.end(); fsh_i++) {
      string fname = fsh_i->first;
      FuncSignature *fs = fsh_i->second;
      string ftypeName = FunctionBFC::returnTypeName(fs->returnType, string(""));
      O<<"FUNCTION "<<fname<<" "<<ftypeName<<endl;
      
      vector<FuncFormalArg*>::iterator sa_i;
      for (sa_i=fs->args.begin(); sa_i!=fs->args.end(); sa_i++) {
        FuncFormalArg *arg = *sa_i;
        O<<arg->argIdx<<" "<<arg->name<<" "<<
            FunctionBFC::returnTypeName(arg->argType, string(""))<<endl;
      }
      
      O<<"END FUNCTION\n"<<endl;
    }

}


// check hasParOrDistCallsReal of each function to propagate that tag and produce warning 07/07/17
int calcFuncParDistTags (FuncSigHash &kFI)
{
    FuncSigHash::iterator fsh_i, fsh_e;
    int oldTotalTag = 0, newTotalTag = 0;

    for (fsh_i = kFI.begin(), fsh_e = kFI.end(); fsh_i != fsh_e; fsh_i++) {
      string fn = fsh_i->first;
      FuncSignature *fs = fsh_i->second;
        
      set<const char *, ltstr>::iterator si;
      for (si = fs->calleeNames.begin(); si != fs->calleeNames.end(); si++) {
        string callName(*si);
        
        if (kFI.count(callName)) {
          if (kFI[callName]->hasParOrDistCallsReal) {
            fs->hasParOrDistCallsReal = true;
            break;
          }
        }
      }
      newTotalTag += fs->hasParOrDistCallsReal;
    }

    // Iteratively check the hasParOrDistCallsReal
    while (newTotalTag != oldTotalTag) {
      oldTotalTag = newTotalTag; //reserve the old total tag val
      newTotalTag = 0; //recompute the new total tag val
      for (fsh_i = kFI.begin(), fsh_e = kFI.end(); fsh_i != fsh_e; fsh_i++) {
        string fn = fsh_i->first;
        FuncSignature *fs = fsh_i->second;
        
        set<const char *, ltstr>::iterator si;
        for (si = fs->calleeNames.begin(); si != fs->calleeNames.end(); si++) {
          string callName(*si);
          if (kFI.count(callName)) {
            if (kFI[callName]->hasParOrDistCallsReal) {
              fs->hasParOrDistCallsReal = true;
              break;
            }
          }
        }
        newTotalTag += fs->hasParOrDistCallsReal;
      }
    }

    return newTotalTag;
}


/* Main pass that takes in a module and runs analysis on each function */
bool BFC::runOnModule(Module &M)
{
    cerr<<"IN run on module"<<endl;
    cerr<<"Module name: "<<M.getModuleIdentifier()<<endl;
    ModuleBFC *bm = new ModuleBFC(&M); //address of a ref=address of the 
					   //address of the object it refers to
    if (getenv("EXCLUSIVE_BLAME") != NULL)
      exclusive_blame = true;
    else
      exclusive_blame = false;
    ////////////////// GLOBAL VARIABLES ////////////////////////////////////////////
    vector<NodeProps *> globalVars;
    int globalNumber = 0; //global variable index

    //To get chpl_ftable information
    GlobalVariable *ftable = M.getNamedGlobal(StringRef("chpl_ftable"));
    if (ftable && ftable->isConstant()) {
      if (ftable->hasInitializer()) {
        Value *ftable_value = ftable->getInitializer();
        if (isa<ConstantArray>(ftable_value)) {
          ConstantArray *ftable_ca = cast<ConstantArray>(ftable_value);
          ArrayType *ftable_ty = ftable_ca->getType();
          unsigned numElems = ftable_ty->getNumElements();
          for (unsigned i=0; i<numElems; i++) {
            Constant *elem = ftable_ca->getAggregateElement(i);
            if (elem->hasName()) { //likely would not happen
              string elemName = elem->getName().str();
              // store it in the module
              bm->funcPtrTable.push_back(elemName);
              cout<<"Odd: "<<i<<"  "<<elemName<<endl;
            }
            else {
              if (isa<ConstantExpr>(elem)) {
                ConstantExpr *ce = cast<ConstantExpr>(elem);
                if (ce->getOpcode() == Instruction::BitCast) {
                  Value *funcV = ce->getOperand(0);
                  if (funcV->hasName()) {
                    string funcName = funcV->getName().str();
                    // store it in the module
                    bm->funcPtrTable.push_back(funcName);
                    cout<<i<<"  "<<funcName<<endl;
                  }
                  else cout<<"funcV has no name"<<endl;
                }
                else cout<<"ce is not a bitcast instruction"<<endl;
              }
              else cout<<"elem is not a ConstantExpr"<<endl;   
            }
          }
        }
        else cout<<"ftable_value is not ConstantArray"<<endl;
      }
      else cout<<"ftable has no initializer"<<endl;
    }
      
/*
    for (Module::global_iterator gI = M.getGlobalList().begin(), 
        gE = M.getGlobalList().end(); gI != gE; ++gI)
    {
	    GlobalVariable *gv = gI;
#ifdef DEBUG_P
        cout<<"Processing Global Variable: "<<(gv->getName()).str()<<endl;
#endif
	    if (gv->hasName() && !gv->isConstant()) { //don't bother constants
#ifdef DEBUG_P
	        cout<<"GV: "<<(gv->getName()).str()<<" is real gv to be stored"<<endl;
#endif
            if (gv->hasInitializer()) {
                NodeProps* v = new NodeProps(globalNumber, gv->getName(), 0, gv->getInitializer());
                globalNumber++;
                globalVars.push_back(v);
            }
            else {
                NodeProps* v = new NodeProps(globalNumber, gv->getName(), 0, NULL);
                globalNumber++;
                globalVars.push_back(v);
            }
	    }

    }
*/
    Finder.processModule(M);//That's it, all DIDescriptors are stored in Vecs:
			                //CUs, SPs, GVs, TYs
    cout<<"Static Analyzing DIGlobalVariables"<<endl;
    for (DebugInfoFinder::iterator I = Finder.global_variable_begin(),
	    E = Finder.global_variable_end(); I != E; I++) {

        DIGlobalVariable *dgv = new DIGlobalVariable(*I);
        if (dgv->getDirectory().equals(StringRef(PRJ_HOME_DIR))) {
        //TODO: Distinguish user funcs from library funcs in the same dir
#ifdef DEBUG_P
            cout<<"DIGlobalVariable: "<<dgv->getName().str()
                <<" "<<dgv->getDirectory().str()<<endl;
#endif
            GlobalVariable *gv = dgv->getGlobal();
            unsigned line_num = dgv->getLineNumber();//the declaration linenum of this gv
            if (gv->hasName() && !gv->isConstant()) { //don't bother constants
#ifdef DEBUG_P
                cout<<"GV: "<<(gv->getName()).str()<<" is real gv to be stored"<<endl;
#endif
                if (gv->hasInitializer()) {
                    NodeProps *v = new NodeProps(globalNumber, gv->getName(), line_num, gv->getInitializer());
                    globalNumber++;
                    globalVars.push_back(v);
                }
                else {
                    NodeProps *v = new NodeProps(globalNumber, gv->getName(), line_num, NULL);
                    globalNumber++;
                    globalVars.push_back(v);
                }
            }
        }
    }

    cout<<"Static Analyzing DITypes"<<endl;
    for (DebugInfoFinder::iterator I = Finder.type_begin(),
	    E = Finder.type_end(); I != E; I++) {
        DIType *dt = new DIType(*I); //create an instance of DIType, not a ptr
        if (dt->getDirectory().equals(StringRef(PRJ_HOME_DIR))) {
#ifdef DEBUG_P
            cout<<"Same Path DIType: "<<dt->getName().str()
                <<" "<<dt->getDirectory().str()<<endl;
#endif
            
            bm->parseDITypes(dt);
        }
    }

//  Make records of the information of external library functions  //
    ExternFuncBFCHash externFuncInformation;
    importSharedBFC("/homes/hzhang86/chapel/chapel-11/third-party/llvm/build/linux64-gnu/lib/Transforms/BFC/SHARED/chapel_runtime.bs", externFuncInformation);
    /*
    importSharedBFC("/export/home/hzhang86/BForChapel/BFC_StaticAnalysis/SHARED/mpi.bs", externFuncInformation);
    importSharedBFC("/export/home/hzhang86/BForChapel/BFC_StaticAnalysis/SHARED/fortran.bs", externFuncInformation);
    importSharedBFC("/export/home/hzhang86/BForChapel/BFC_StaticAnalysis/SHARED/cblas.bs", externFuncInformation);
    */
//  NOT SURE THE ABOVE IS NEEDED FOR Chapel Program //

    // SETUP all the exports files 
    string blame_path("EXPORT/");
	string struct_path = blame_path;
	string calls_path = blame_path;
	string params_path = blame_path;
	string se_path = blame_path;
	string sea_path = blame_path;
	string alias_path = blame_path;
	string loops_path = blame_path;
	string conds_path = blame_path;
    string funcs_path = blame_path;
	
	string mod_name = M.getModuleIdentifier();
	string blame_extension(".blm");
	blame_path += mod_name;
	blame_path += blame_extension;
	ofstream blame_file(blame_path.c_str());
	
	string se_extension(".se");
	se_path += mod_name;
	se_path += se_extension;
	ofstream blame_se_file(se_path.c_str());
	
	string struct_extension(".structs");
	struct_path += mod_name;
	struct_path += struct_extension;
    //Here we make struct_file to be global in case we need to addin more later
	struct_file.open(struct_path.c_str());
	
	string calls_extension(".calls");
	calls_path += mod_name;
	calls_path += calls_extension;
	ofstream calls_file(calls_path.c_str());
	
	string params_extension(".params");
	params_path += mod_name;
	params_path += params_extension;
	ofstream params_file(params_path.c_str());
	
	string funcs_extension(".funcs");
	funcs_path += mod_name;
	funcs_path += funcs_extension;
	ofstream funcs_file(funcs_path.c_str());


	bm->exportStructs(struct_file);
    
    //////////////// PRE PASS for internal module functions //////////////////////
    FuncSigHash externKFN;
    for (DebugInfoFinder::iterator I = Finder.subprogram_begin(),
	     E = Finder.subprogram_end(); I != E; I++) {

      DISubprogram *dsp = new DISubprogram(*I);
      if (!dsp->getDirectory().equals(StringRef(PRJ_HOME_DIR))) {
        string dspName = dsp->getName().str();
        if (dspName.find("wrap") != 0) { //no 'wrap*'
           //if (dspName.compare("main")==0 || dspName.compare("sayhello")==0 ||\
                 dspName.compare("factorial")==0) 
            Function *F = dsp->getFunction();
#ifdef DEBUG_P
            cout<<"Same directory of DISP: "<<dsp->getName().str()<< \
                " is "<<dsp->getDirectory().str()<<endl;
            cout<<"Corresponding Function is: "<<F->getName().str()<<endl;
#endif
            if (F->isDeclaration())//GlobalValue->isDeclaration
              continue;
            else {
              FuncSignature *funcSig = new FuncSignature();
              funcSig->fname = F->getName().str();
              funcSig->returnType = F->getReturnType();
              
              int whichParam = 0;
              Function::arg_iterator af_i;
              for (af_i=F->arg_begin(); af_i!=F->arg_end(); af_i++) {
                Value *v = af_i;
                string argName = v->getName().str();
                Type *argType = v->getType();
                FuncFormalArg *arg = new FuncFormalArg();
                arg->name = argName;
                arg->argType = argType;
                arg->argIdx = whichParam;

                funcSig->args.push_back(arg);
                whichParam++;
              }
            
              externKFN[funcSig->fname] = funcSig;
            } //not declaration
        } //dspName cond 1
      } //directory
    } //all functions
    // We don't need to output func info for non-user module functions
    //exportUserFuncsInfo(externKFN, funcs_file);
    ////////////////////////////////////////////////////////////////////////////
    
    ////////////////////// EXTERN PASS for internal module functions //////////
    //Functions from internal modules:keep blamed paramNum in chapel_internals.bs
    ofstream args_file("./chapel_internal.bs");
    for (DebugInfoFinder::iterator I = Finder.subprogram_begin(),
	    E = Finder.subprogram_end(); I != E; I++) {

      DISubprogram *dsp = new DISubprogram(*I);
      if (!dsp->getDirectory().equals(StringRef(PRJ_HOME_DIR))) { //Non-user functions
        string dspName = dsp->getName().str();
        if (dspName.find("wrap") != 0) { //no 'wrap*'
          Function *F = dsp->getFunction();
          FunctionBFC *fb;
#ifdef DEBUG_P
          cout<<"Directory of DISP: "<<dsp->getName().str()<< \
                        " is "<<dsp->getDirectory().str()<<endl;
          cout<<"Corresponding Function is: "<<F->getName().str()<<endl;
#endif
          if (F->isDeclaration())//GlobalValue->isDeclaration
            continue;
          else {
            // Run regular blame calculation
            fb = new FunctionBFC(F, externKFN);
            fb->setModule(&M);
            fb->setModuleBFC(bm);
            fb->isExternFunc = true; //TO distinguish user func and extern funcs

#ifdef DEBUG_P  
            cout<<"Running externFuncPass on Func: "<<F->getName().str()<<endl;
#endif
          //mostly the same as firstPass, except we only do util genGraph.determineBFCHoldersLite
            fb->externFuncPass(F, globalVars, externFuncInformation, args_file);

            delete(fb);
          }
        }//isn't wrap*
      }//directory
    }//all functions
    args_file.close();
    // Retrive param info from the file we built 
    importSharedBFC("./chapel_internal.bs", externFuncInformation);
    externKFN.clear();

    cout<<"Static Analyzing DISubprograms"<<endl;
    /////////////////// PRE PASS ////////////////////////////////////
    FuncSigHash knownFuncsInfo;

    for (DebugInfoFinder::iterator I = Finder.subprogram_begin(),
	     E = Finder.subprogram_end(); I != E; I++) {

      DISubprogram *dsp = new DISubprogram(*I);
      if (dsp->getDirectory().equals(StringRef(PRJ_HOME_DIR))) {
        string dspName = dsp->getName().str();
        //dspName chopped everything from "_chpl", but we need to keep chpl_gen_main and init funcs
        if (!dspName.empty() && dspName.compare("=")!=0) { 
          //no '_XXX' or wrap* in interesting dspName
          if ((*(dspName.begin()) != '_') && (dspName.find("wrap") != 0)) { 
            Function *F = dsp->getFunction();
#ifdef DEBUG_P
            cout<<"Same directory of DISP: "<<dsp->getName().str()<< \
                " is "<<dsp->getDirectory().str()<<endl;
            cout<<"Corresponding Function is: "<<F->getName().str()<<endl;
#endif
            if (F->isDeclaration())//GlobalValue->isDeclaration
              continue;
            else {
              FuncSignature *funcSig = new FuncSignature();
              funcSig->fname = F->getName().str();
              funcSig->returnType = F->getReturnType();
              
              int whichParam = 0;
              Function::arg_iterator af_i;
              for (af_i=F->arg_begin(); af_i!=F->arg_end(); af_i++) {
                Value *v = af_i;
                string argName = v->getName().str();
                Type *argType = v->getType();
                FuncFormalArg *arg = new FuncFormalArg();
                arg->name = argName;
                arg->argType = argType;
                arg->argIdx = whichParam;

                funcSig->args.push_back(arg);
                whichParam++;
              }
            
              knownFuncsInfo[funcSig->fname] = funcSig;
            } //not declaration
          } //dspName cond 2
        } //dspName cond 1
      } //directory
    } //all subprograms

    exportUserFuncsInfo(knownFuncsInfo, funcs_file);
    //////////////// FIRST PASS ////////////////////////////////////

    int numMissing = 0;
    int numMEV = 0;
    int numMEV2 = 0;
    int numMEV3 = 0;

    //Generate relationships from first pass over functions
    for (DebugInfoFinder::iterator I = Finder.subprogram_begin(),
	    E = Finder.subprogram_end(); I != E; I++) {

      DISubprogram *dsp = new DISubprogram(*I);
      if (dsp->getDirectory().equals(StringRef(PRJ_HOME_DIR))) {
        string dspName = dsp->getName().str();
        //dspName chopped everything from "_chpl", but we need to keep chpl_gen_main and init funcs
        if (!dspName.empty() && dspName.compare("=")!=0) { 
          //no '_XXX' or wrap* in interesting dspName
          if ((*(dspName.begin()) != '_') && (dspName.find("wrap") != 0)) { 
           
            Function *F = dsp->getFunction();
            FunctionBFC *fb;

            if (F->isDeclaration())
              continue;
            else {
              // Run regular blame calculation
              fb = new FunctionBFC(F, knownFuncsInfo);
              fb->setModule(&M);
              fb->setModuleBFC(bm);
              //dspName won't have "_chpl" suffix
              calls_file<<"FUNCTION "<<F->getName().str()<<endl; //F->getName will have "_chpl" suffix
#ifdef DEBUG_P  
              cout<<"Running firstPass on Func: "<<F->getName().str()<<endl;
#endif
              fb->firstPass(F, globalVars, externFuncInformation, 
                       blame_file, blame_se_file, calls_file, numMissing);
              calls_file<<"END FUNCTION "<<endl;

              int numMEVL = 0;
              int numMEV2L = 0;
              int numMEV3L = 0;
                        
              fb->moreThanOneEV(numMEVL, numMEV2L, numMEV3L);

              numMEV += numMEVL;
              numMEV2 += numMEV2L;
              numMEV3 += numMEV3L;

              fb->exportParams(params_file);
              params_file<<"NEVs - "<<numMEVL<<" "<<numMEV2L<<" " \
                  <<numMEV3L<<" out of "<<knownFuncsInfo.size()<<endl;
              params_file<<endl;

              //store necessary information for each func to use after delete
              FuncSignature *fs = knownFuncsInfo[F->getName().str()];
              fs->calleeNames = fb->funcCallNames;
              fs->hasParOrDistCallsReal = fb->hasParOrDistCalls;
              
              //Hui 07/17/17: sizeof bf ~1.5kB, so we can keep fb for now
              delete(fb);
            }
          } //end of != '_'
        } //end of ==string::npos
      }
    }

    //07/17/17: we need to keep the inter-procedure info in order to set pids

    // 07/07/17 calculate hasParDistCalls for each FunctionBFC
    int tagTotal = calcFuncParDistTags(knownFuncsInfo);
    // Warn user if there're multiple callsites of the same ParDistCalls
    if (tagTotal) {
      FuncSigHash::iterator fsh_i, fsh_e;

      for (fsh_i = knownFuncsInfo.begin(), fsh_e = knownFuncsInfo.end(); 
                                                    fsh_i != fsh_e; fsh_i++) {
        string fn = fsh_i->first;
        FuncSignature *fs = fsh_i->second;
        
        set<const char *, ltstr>::iterator si;
        for (si = fs->calleeNames.begin(); si != fs->calleeNames.end(); si++) {
          string calleeName(*si);
          if (knownFuncsInfo.count(calleeName)) {
            FuncSignature *callee = knownFuncsInfo[calleeName];
            callee->callerNames.insert(fn);
          }
        }
      }

      for (fsh_i = knownFuncsInfo.begin(), fsh_e = knownFuncsInfo.end(); 
                                                    fsh_i != fsh_e; fsh_i++) {
        string fn = fsh_i->first;
        FuncSignature *fs = fsh_i->second;
        
        // if fb has par/dist construct and more than one callsites of it
        if (fs->hasParOrDistCallsReal && fs->callerNames.size() >= 2) { 
          set<string>::iterator si;
          for (si=fs->callerNames.begin(); si!=fs->callerNames.end(); si++) {
            string callerName(*si);
            if (knownFuncsInfo.count(callerName)) {
              FuncSignature *caller = knownFuncsInfo[callerName];
            
              caller->maybeMissBlamed = true;
              cerr<<"Attention: there might be incorrect blame to "<<
                  caller->fname<<" from one of its callees "<<fs->fname<<endl;
            }
          }
        }
      }
    }


    struct_file<<"END STRUCTS"<<endl; //IMPORTANT! Moved here from exportStructs
    params_file<<"NEVs - "<<numMEV<<" "<<numMEV3<<" "<<numMEV2<<" out of "<< \
        knownFuncsInfo.size()<<endl;

    //we really should close files properly
    blame_file.close();
    blame_se_file.close();
    struct_file.close();
    calls_file.close();
    params_file.close();
    funcs_file.close();
 
    globalVars.clear();
    externFuncInformation.clear();
    knownFuncsInfo.clear();
    return false;
}
