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

#include "BlameProgram.h"
#include "BlameFunction.h"
#include "BlameModule.h"
#include "BlameStruct.h"
#include "Instances.h"

#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>

using namespace std;

VertexProps *BlameFunction::findOrCreateVP(std::string & name)
{
  
  VertexProps * vp = NULL;
  
  if (allVertices.count(name))
  {
    return allVertices[name];
  }
  else
  {
    std::string s(name);
    vp = new VertexProps(s);
    vp->BF = this;
    allVertices[s] = vp;
  }
  
  return vp;
  
}

VertexProps *BlameFunction::findOrCreateTempBlamees(std::set<VertexProps *> &blamees, std::string name, bool &found)
{
  ////std::cout<<"In findOrCreateTempBlamees for "<<getName()<<std::endl;
  
  VertexProps *vp = NULL;
  
  std::set<VertexProps *>::iterator set_vp_i;
  
  for (set_vp_i = blamees.begin(); set_vp_i != blamees.end(); set_vp_i++) {
    if (name ==  getFullStructName(*set_vp_i)) {
      vp = (*set_vp_i);
      ////std::cout<<"FOUND Temp Blamee "<<name<<std::endl;
      break;
    }
  }
  
  if (vp == NULL) {
    ////std::cout<<"Creating derived blamee "<<name<<std::endl;
    std::string s(name);
    vp = new VertexProps(s);
    vp->BF = this;
    //vp->fsName = s;
    vp->isDerived = true;
    found = false;
    vp->addedFromWhere = 90;
#ifdef DEBUG_BLAMEES
    cout<<"Blamees insert "<<vp->name<<" in findOrCreateTempBlamees" <<endl;
#endif
    blamees.insert(vp);
  }
  
  return vp;
}

void BlameFunction::calcSEDWritesRecursive(VertexProps * ev, VertexProps * vp, std::set<VertexProps *> & visited)
{
  
  if (visited.count(vp))
    return;
  
  visited.insert(vp);
  
  std::set<VertexProps *>::iterator s_vp_i;
  std::set<VertexProps *>::iterator s_vp_i2;
  
  
  for (s_vp_i = vp->children.begin(); s_vp_i != vp->children.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    
    if (child->eStatus > NO_EXIT)
      continue;
    
    ////std::cout<<"For EV - "<<ev->name<<" adding WV "<<child->name<<std::endl;
    ev->writableVals.insert(child);
  }
  
  
  /*
   for (s_vp_i = ivp->dfChildren.begin(); s_vp_i != ivp->dfChildren.end(); s_vp_i++)
   {
   VertexProps * child = *s_vp_i;
   if (child->calcAgg == false)
   calcAggregateLNRecursive(child);
   
   ivp->descLineNumbers.insert(child->descLineNumbers.begin(), child->descLineNumbers.end());
   }*/
  
  for (s_vp_i = vp->fields.begin(); s_vp_i != vp->fields.end(); s_vp_i++)
  {
    VertexProps * field = *s_vp_i;
    std::set<VertexProps *> fieldVisited;
    calcSEDWritesRecursive(field, field, fieldVisited);
    fieldVisited.clear();
  }
  
  for (s_vp_i = vp->dataPtrs.begin(); s_vp_i != vp->dataPtrs.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    
    if (child->isWritten)
    {
      calcSEDWritesRecursive(ev, child, visited);
    }      
  }
  
  for (s_vp_i = vp->aliases.begin(); s_vp_i != vp->aliases.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    calcSEDWritesRecursive(ev, child, visited);
    
  }
  
  for (s_vp_i = vp->dfAliases.begin(); s_vp_i != vp->dfAliases.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    calcSEDWritesRecursive(ev, child, visited);
  }  
}

void BlameFunction::calcSEDReadsRecursive(VertexProps * ev, VertexProps * vp, std::set<VertexProps *> & visited)
{
  
  if (visited.count(vp))
    return;
  
  visited.insert(vp);
  
  std::set<VertexProps *>::iterator s_vp_i;
  
  for (s_vp_i = vp->fields.begin(); s_vp_i != vp->fields.end(); s_vp_i++)
  {
    VertexProps * field = *s_vp_i;
    std::set<VertexProps *> fieldVisited;
    calcSEDReadsRecursive(field, field, fieldVisited);
    fieldVisited.clear();
  }
  
  for (s_vp_i = vp->dataPtrs.begin(); s_vp_i != vp->dataPtrs.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    
    if (!child->isWritten)
    {
      //std::cout<<"For EV - "<<ev->name<<" adding RV "<<child->name<<std::endl;
      ev->readableVals.insert(child);
      //calcSEDReadsRecursive(ev, child, visited);
    }      
  }
  
  for (s_vp_i = vp->aliases.begin(); s_vp_i != vp->aliases.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    calcSEDReadsRecursive(ev, child, visited);
    
  }
  
  for (s_vp_i = vp->dfAliases.begin(); s_vp_i != vp->dfAliases.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    calcSEDReadsRecursive(ev, child, visited);
  }  
}

void BlameFunction::calcSideEffectDependencies()
{
  //std::cout<<"Calcing SE Dependencies for "<<getName()<<std::endl;
  
  std::vector<VertexProps *>::iterator ivh_i, ivh_i2;
  for (ivh_i = exitVariables.begin(); ivh_i != exitVariables.end(); ivh_i++)
  {
    VertexProps * ivp = (*ivh_i);
    
    //clearAfter.clear();
    if (ivp->eStatus >= EXIT_VAR_PARAM) //changed by Hui 03/15/16
    {                                   //from >EXIT_VAR_PARAM to >=, same changes 
      //std::cout<<"For EV "<<ivp->name<<std::endl; //applied to other places
      std::set<VertexProps *> visited;
      calcSEDWritesRecursive(ivp, ivp, visited);
      visited.clear();
      calcSEDReadsRecursive(ivp, ivp, visited);
    }
  }
  
  for (ivh_i = exitVariables.begin(); ivh_i != exitVariables.end(); ivh_i++)
  {
    VertexProps * ivp = (*ivh_i);
    
    //clearAfter.clear();
    if (ivp->eStatus >= EXIT_VAR_PARAM)
    {
      for (ivh_i2 = exitVariables.begin(); ivh_i2 != exitVariables.end(); ivh_i2++)
      {
        VertexProps * ivp2 = (*ivh_i2);
        if (ivp2 == ivp)
          continue;
        
        std::set<VertexProps *> evIntersect;
        set_intersection(ivp->writableVals.begin(), ivp->writableVals.end(), 
                         ivp2->readableVals.begin(), ivp2->readableVals.end(), 
                         inserter(evIntersect,evIntersect.begin()) );
        
        
        
        if (evIntersect.size() > 0)
        {
          //std::cout<<"Relationship between "<<ivp->name<<" and "<<ivp2->name<<std::endl;
        }
        
      }
    }
  }
}


void BlameFunction::calcAggCallRecursive(VertexProps * ivp)
{
  if (ivp->calcAggCall)
    return;
  
  ivp->calcAggCall = true;
  
  if (ivp->nStatus[CALL_NODE])
    return;
  
  //ivp->descCalls.insert(ivp->calls.begin(), ivp->calls.end());
  
  if (ivp->nStatus[CALL_PARAM] )
  {  
    ivp->descParams.insert(ivp);
    ivp->aliasParams.insert(ivp);
    return;
  }
  
  if ( ivp->nStatus[CALL_RETURN] )
  {
    ivp->descParams.insert(ivp);
    ivp->aliasParams.insert(ivp);
    return;
  }
  
  
  std::set<VertexProps *>::iterator s_vp_i;
  //std::vector<VertexProps *>::iterator s_vp_i;
  
  for (s_vp_i = ivp->children.begin(); s_vp_i != ivp->children.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAggCall == false)
      calcAggCallRecursive(child);
    
    ivp->descParams.insert(child->descParams.begin(), child->descParams.end());
    
  }
  
  
  for (s_vp_i = ivp->storesTo.begin(); s_vp_i != ivp->storesTo.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    
    if (child->calcAggCall == false)
      calcAggCallRecursive(child);
    
    if (child->nStatus[CALL_RETURN])  
    {      
      ivp->descParams.insert(child->descParams.begin(), child->descParams.end());
    }  
  }
  
  for (s_vp_i = ivp->dfChildren.begin(); s_vp_i != ivp->dfChildren.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAggCall == false)
      calcAggCallRecursive(child);
    
    ivp->descParams.insert(child->descParams.begin(), child->descParams.end());
    
    
  }
  
  for (s_vp_i = ivp->dataPtrs.begin(); s_vp_i != ivp->dataPtrs.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAggCall == false)
      calcAggCallRecursive(child);
    
    ivp->descParams.insert(child->descParams.begin(), child->descParams.end());
    ivp->aliasParams.insert(child->descParams.begin(), child->descParams.end());
    
  }
  
  for (s_vp_i = ivp->aliases.begin(); s_vp_i != ivp->aliases.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAggCall == false)
      calcAggCallRecursive(child);
    
    ivp->descParams.insert(child->descParams.begin(), child->descParams.end());
    ivp->aliasParams.insert(child->descParams.begin(), child->descParams.end());
    
    
  }
  
  for (s_vp_i = ivp->dfAliases.begin(); s_vp_i != ivp->dfAliases.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAggCall == false)
      calcAggCallRecursive(child);
    
    ivp->descParams.insert(child->descParams.begin(), child->descParams.end());
    ivp->aliasParams.insert(child->descParams.begin(), child->descParams.end());
    
  }
  
}

/*
OLD PRE-DISSERTATION
*/
/*
void BlameFunction::calcAggregateLN_SE_Recursive(VertexProps * ivp)
{
  //std::cout<<"Entering calcAggregateLN_SE_Recursive for "<<ivp->name<<std::endl;
  
  if (ivp->calcAggSE)
  {
    //std::cout<<"Exiting calcAggregateLN_SE_Recursive(1) for "<<ivp->name<<std::endl;
    return;
  }
  
  
  ivp->calcAggSE = true;
  
  ivp->seLineNumbers.insert(ivp->lineNumbers.begin(), ivp->lineNumbers.end());
  ivp->seLineNumbers.insert(ivp->declaredLine);
  ivp->seLineNumbers.insert(ivp->tempSELines.begin(), ivp->tempSELines.end());
  
  if (ivp->storeFrom != NULL)
    ivp->seLineNumbers.insert(ivp->storeFrom->declaredLine);
  
  
  
  std::set<VertexProps *>::iterator s_vp_i;
  std::set<VertexProps *>::iterator s_vp_i2;
  
  
  for (s_vp_i = ivp->children.begin(); s_vp_i != ivp->children.end(); s_vp_i++)
  {
    //std::cout<<"CALN_SE_R(1) "<<(*s_vp_i)->name<<std::endl;
    VertexProps * child = *s_vp_i;
    if (child->calcAggSE == false)
      calcAggregateLN_SE_Recursive(child);
    
    
    // LOCAL VARS PASSED IN AS PARAMS ??? ANYTHING PASSED IN AS PARAMS????
    if (!(child->nStatus[LOCAL_VAR] && child->storesTo.size() > 1) && !(child->nStatus[CALL_NODE]) &&
        !(child->nStatus[LOCAL_VAR] && child->nStatus[CALL_PARAM]))
    {
      //std::cout<<"Inserting LNs to "<<ivp->name<<" from "<<child->name<<"(";
      for (int a = 0; a < NODE_PROPS_SIZE; a++)
      {
        //std::cout<<child->nStatus[a]<<" ";
      }
      //std::cout<<")"<<std::endl;
      
      
      ivp->seLineNumbers.insert(child->seLineNumbers.begin(), child->seLineNumbers.end());
    }
  }
  
  for (s_vp_i = ivp->dfChildren.begin(); s_vp_i != ivp->dfChildren.end(); s_vp_i++)
  {
    //std::cout<<"CALN_SE_R(2) "<<(*s_vp_i)->name<<std::endl;
    VertexProps * child = *s_vp_i;
    if (child->calcAggSE == false)
      calcAggregateLN_SE_Recursive(child);
    
    ivp->seLineNumbers.insert(child->seLineNumbers.begin(), child->seLineNumbers.end());
  }
  
  for (s_vp_i = ivp->dataPtrs.begin(); s_vp_i != ivp->dataPtrs.end(); s_vp_i++)
  {
    //std::cout<<"CALN_SE_R(3) "<<(*s_vp_i)->name<<std::endl;
    VertexProps * child = *s_vp_i;
    if (child->calcAggSE == false)
      calcAggregateLN_SE_Recursive(child);
    
    if (child->isWritten || child->tempIsWritten)
    {
      ivp->seLineNumbers.insert(child->seLineNumbers.begin(), child->seLineNumbers.end());
    }
    
  }
  
  for (s_vp_i = ivp->aliases.begin(); s_vp_i != ivp->aliases.end(); s_vp_i++)
  {
    //std::cout<<"CALN_SE_R(4) "<<(*s_vp_i)->name<<std::endl;
    
    VertexProps * child = *s_vp_i;
    if (child->calcAggSE == false)
      calcAggregateLN_SE_Recursive(child);
    
    ivp->seLineNumbers.insert(child->seLineNumbers.begin(), child->seLineNumbers.end());
  }
  
  for (s_vp_i = ivp->dfAliases.begin(); s_vp_i != ivp->dfAliases.end(); s_vp_i++)
  {
    //std::cout<<"CALN_SE_R(5) "<<(*s_vp_i)->name<<std::endl;
    
    VertexProps * child = *s_vp_i;
    if (child->calcAggSE == false)
      calcAggregateLN_SE_Recursive(child);
    
    ivp->seLineNumbers.insert(child->seLineNumbers.begin(), child->seLineNumbers.end());
  }
  
  for (s_vp_i = ivp->resolvedLSFrom.begin(); s_vp_i != ivp->resolvedLSFrom.end(); s_vp_i++)
  {
    //std::cout<<"CALN_SE_R(6) "<<(*s_vp_i)->name<<std::endl;
    
    VertexProps * child = *s_vp_i;
    if (child->calcAggSE == false)
      calcAggregateLN_SE_Recursive(child);
    
    for (s_vp_i2 = child->resolvedLSSideEffects.begin(); 
         s_vp_i2 != child->resolvedLSSideEffects.end(); s_vp_i2++)
    {
      VertexProps * sECause = *s_vp_i2;
      if (sECause->calcAggSE == false)
        calcAggregateLN_SE_Recursive(sECause);
      
      // TODO:  This technically only happens if sECause dominates ivp
      ivp->seLineNumbers.insert(sECause->seLineNumbers.begin(), sECause->seLineNumbers.end());
    }
  }
  
  for (s_vp_i = ivp->tempRelations.begin(); s_vp_i != ivp->tempRelations.end(); s_vp_i++)
  {
    //std::cout<<"CALN_SE_R(7) "<<(*s_vp_i)->name<<std::endl;
    
    VertexProps * child = *s_vp_i;
    if (child->calcAggSE == false)
      calcAggregateLN_SE_Recursive(child);
    
    ivp->seLineNumbers.insert(child->seLineNumbers.begin(), child->seLineNumbers.end());
  }
  
  
  for (s_vp_i = ivp->tempChildren.begin(); s_vp_i != ivp->tempChildren.end(); s_vp_i++)
  {
    //std::cout<<"CALN_SE_R(8) "<<(*s_vp_i)->name<<std::endl;
    
    VertexProps * child = *s_vp_i;
    if (child->calcAggSE == false)
      calcAggregateLN_SE_Recursive(child);
    
    ivp->seLineNumbers.insert(child->seLineNumbers.begin(), child->seLineNumbers.end());
    ivp->seLineNumbers.insert(child->declaredLine);
  }
  
}
*/


void BlameFunction::calcAggregateLN_SE_Recursive(VertexProps * ivp)
{
#ifdef DEBUG_SELINES
  std::cout<<"Entering calcAggregateLN_SE_Recursive for "<<ivp->name<<std::endl;
#endif 
  if (ivp->calcAggSE)
  {
    return;
  }
  
#ifdef DEBUG_SELINES
  set<int>::iterator si;
#endif

  ivp->calcAggSE = true;
  
  ivp->seLineNumbers.insert(ivp->lineNumbers.begin(), ivp->lineNumbers.end());
  ivp->seLineNumbers.insert(ivp->declaredLine);
#ifdef DEBUG_SELINES
  cout<<"After inserting its own lineNumbers and declaredLine:"<<endl;
  for(si=ivp->seLineNumbers.begin(); si !=ivp->seLineNumbers.end(); si++)
    cout<<*si<<" ";
  cout<<endl;
#endif

  ivp->seLineNumbers.insert(ivp->tempSELines.begin(), ivp->tempSELines.end());
#ifdef DEBUG_SELINES
  cout<<"After inserting its tempSELines:"<<endl;
  for(si=ivp->seLineNumbers.begin(); si !=ivp->seLineNumbers.end(); si++)
    cout<<*si<<" ";
  cout<<endl;
#endif

  if (ivp->storeFrom != NULL)
    ivp->seLineNumbers.insert(ivp->storeFrom->declaredLine);
#ifdef DEBUG_SELINES
  cout<<"After inserting from its storeFrom's declaredLine:"<<endl;
  for(si=ivp->seLineNumbers.begin(); si !=ivp->seLineNumbers.end(); si++)
    cout<<*si<<" ";
  cout<<endl;
#endif
  
  std::set<VertexProps *>::iterator s_vp_i;
  std::set<VertexProps *>::iterator s_vp_i2;

  
  for (s_vp_i = ivp->tempRelations.begin(); s_vp_i != ivp->tempRelations.end(); s_vp_i++)
  {
    //std::cout<<"CALN_SE_R(7) "<<(*s_vp_i)->name<<std::endl;
    
    VertexProps * child = *s_vp_i;
    if (child->calcAggSE == false)
      calcAggregateLN_SE_Recursive(child);
    
    ivp->seLineNumbers.insert(child->seLineNumbers.begin(), child->seLineNumbers.end());
  }
  
#ifdef DEBUG_SELINES
  cout<<"After inserting from its tempRelations:"<<endl;
  for(si=ivp->seLineNumbers.begin(); si !=ivp->seLineNumbers.end(); si++)
    cout<<*si<<" ";
  cout<<endl;
#endif
  
  for (s_vp_i = ivp->tempChildren.begin(); s_vp_i != ivp->tempChildren.end(); s_vp_i++)
  {
    //std::cout<<"CALN_SE_R(8) "<<(*s_vp_i)->name<<std::endl;
    
    VertexProps *child = *s_vp_i;
    if (child->calcAggSE == false)
      calcAggregateLN_SE_Recursive(child);
    
    ivp->seLineNumbers.insert(child->seLineNumbers.begin(), child->seLineNumbers.end());
    ivp->seLineNumbers.insert(child->declaredLine);
  }

#ifdef DEBUG_SELINES
  cout<<"After inserting from its tempChildren:"<<endl;
  for(si=ivp->seLineNumbers.begin(); si !=ivp->seLineNumbers.end(); si++)
    cout<<*si<<" ";
  cout<<endl;
#endif
#ifdef DEBUG_SELINES
  std::cout<<"Exiting calcAggregateLN_SE_Recursive(1) for "<<ivp->name<<std::endl;
#endif
}


/*
OLD PRE-DISSERTATION
*/
/*
void BlameFunction::calcAggregateLNRecursive(VertexProps * ivp)
{
  if (ivp->calcAgg)
    return;
  
  ivp->calcAgg = true;
  
  
  ivp->descLineNumbers.insert(ivp->lineNumbers.begin(), ivp->lineNumbers.end());
  ivp->descLineNumbers.insert(ivp->declaredLine);
  
  if (ivp->storeFrom != NULL)
    ivp->descLineNumbers.insert(ivp->storeFrom->declaredLine);
  
  
  
  std::set<VertexProps *>::iterator s_vp_i;
  std::set<VertexProps *>::iterator s_vp_i2;
  
  
  for (s_vp_i = ivp->children.begin(); s_vp_i != ivp->children.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAgg == false)
      calcAggregateLNRecursive(child);
    
    if (!(child->nStatus[LOCAL_VAR] && child->storesTo.size() > 1) && !(child->nStatus[CALL_NODE]))
      ivp->descLineNumbers.insert(child->descLineNumbers.begin(), child->descLineNumbers.end());
  }
  
  for (s_vp_i = ivp->dfChildren.begin(); s_vp_i != ivp->dfChildren.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAgg == false)
      calcAggregateLNRecursive(child);
    
    ivp->descLineNumbers.insert(child->descLineNumbers.begin(), child->descLineNumbers.end());
  }
  
  for (s_vp_i = ivp->dataPtrs.begin(); s_vp_i != ivp->dataPtrs.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAgg == false)
      calcAggregateLNRecursive(child);
    
    if (child->isWritten)
    {
      ivp->descLineNumbers.insert(child->descLineNumbers.begin(), child->descLineNumbers.end());
    }
    
  }
  
  for (s_vp_i = ivp->aliases.begin(); s_vp_i != ivp->aliases.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAgg == false)
      calcAggregateLNRecursive(child);
    
    ivp->descLineNumbers.insert(child->descLineNumbers.begin(), child->descLineNumbers.end());
  }
  
  for (s_vp_i = ivp->dfAliases.begin(); s_vp_i != ivp->dfAliases.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAgg == false)
      calcAggregateLNRecursive(child);
    
    ivp->descLineNumbers.insert(child->descLineNumbers.begin(), child->descLineNumbers.end());
  }
  
  for (s_vp_i = ivp->resolvedLSFrom.begin(); s_vp_i != ivp->resolvedLSFrom.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAgg == false)
      calcAggregateLNRecursive(child);
    
    for (s_vp_i2 = child->resolvedLSSideEffects.begin(); 
         s_vp_i2 != child->resolvedLSSideEffects.end(); s_vp_i2++)
    {
      VertexProps * sECause = *s_vp_i2;
      if (sECause->calcAgg == false)
        calcAggregateLNRecursive(sECause);
      
      // TODO:  This technically only happens if sECause dominates ivp
      ivp->descLineNumbers.insert(sECause->descLineNumbers.begin(), sECause->descLineNumbers.end());
    }
  }
  
}
*/


void BlameFunction::calcAggregateLNRecursive(VertexProps * ivp)
{
  if (ivp->calcAgg)
    return;
  
  ivp->calcAgg = true;
  
  
  //ivp->descLineNumbers.insert(ivp->lineNumbers.begin(), ivp->lineNumbers.end());
  ivp->descLineNumbers.insert(ivp->declaredLine);
  
  if (ivp->storeFrom != NULL)
    ivp->descLineNumbers.insert(ivp->storeFrom->declaredLine);
  
  
  /*
  std::set<VertexProps *>::iterator s_vp_i;
  std::set<VertexProps *>::iterator s_vp_i2;
  
  
  for (s_vp_i = ivp->children.begin(); s_vp_i != ivp->children.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAgg == false)
      calcAggregateLNRecursive(child);
    
    if (!(child->nStatus[LOCAL_VAR] && child->storesTo.size() > 1) && !(child->nStatus[CALL_NODE]))
      ivp->descLineNumbers.insert(child->descLineNumbers.begin(), child->descLineNumbers.end());
  }
  
  for (s_vp_i = ivp->dfChildren.begin(); s_vp_i != ivp->dfChildren.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAgg == false)
      calcAggregateLNRecursive(child);
    
    ivp->descLineNumbers.insert(child->descLineNumbers.begin(), child->descLineNumbers.end());
  }
  
  for (s_vp_i = ivp->dataPtrs.begin(); s_vp_i != ivp->dataPtrs.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAgg == false)
      calcAggregateLNRecursive(child);
    
    if (child->isWritten)
    {
      ivp->descLineNumbers.insert(child->descLineNumbers.begin(), child->descLineNumbers.end());
    }
    
  }
  
  for (s_vp_i = ivp->aliases.begin(); s_vp_i != ivp->aliases.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAgg == false)
      calcAggregateLNRecursive(child);
    
    ivp->descLineNumbers.insert(child->descLineNumbers.begin(), child->descLineNumbers.end());
  }
  
  for (s_vp_i = ivp->dfAliases.begin(); s_vp_i != ivp->dfAliases.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAgg == false)
      calcAggregateLNRecursive(child);
    
    ivp->descLineNumbers.insert(child->descLineNumbers.begin(), child->descLineNumbers.end());
  }
  
  for (s_vp_i = ivp->resolvedLSFrom.begin(); s_vp_i != ivp->resolvedLSFrom.end(); s_vp_i++)
  {
    VertexProps * child = *s_vp_i;
    if (child->calcAgg == false)
      calcAggregateLNRecursive(child);
    
    for (s_vp_i2 = child->resolvedLSSideEffects.begin(); 
         s_vp_i2 != child->resolvedLSSideEffects.end(); s_vp_i2++)
    {
      VertexProps * sECause = *s_vp_i2;
      if (sECause->calcAgg == false)
        calcAggregateLNRecursive(sECause);
      
      // TODO:  This technically only happens if sECause dominates ivp
      ivp->descLineNumbers.insert(sECause->descLineNumbers.begin(), sECause->descLineNumbers.end());
    }
  }
  */
  
}


void BlameFunction::calcAggregateLN()
{
  //std::vector<VertexProps *>::iterator ivh_i;
  VertexHash::iterator hash_vp_i;
  
  // Populate Edges
  for (hash_vp_i = allVertices.begin(); hash_vp_i != allVertices.end(); hash_vp_i++)
  {
    VertexProps * ivp = (*hash_vp_i).second;
    if (ivp->calcAgg == false)
      calcAggregateLNRecursive(ivp);
    
    if (ivp->calcAggCall == false)
      calcAggCallRecursive(ivp);
  }
}


void BlameFunction::calcAggregateLN_SE()
{
  
  //  std::vector<VertexProps *>::iterator ivh_i;
  VertexHash::iterator hash_vp_i;

  // Populate Edges
  for (hash_vp_i = allVertices.begin(); hash_vp_i != allVertices.end(); hash_vp_i++)
  //for (ivh_i = allVertices.begin(); ivh_i != allVertices.end(); ivh_i++)
  {
    VertexProps * ivp = (*hash_vp_i).second;
    //VertexProps * ivp = (*ivh_i);
    if (ivp->calcAggSE == false)
      calcAggregateLN_SE_Recursive(ivp);
  }
  
}


BlameFunction * BlameFunction::parseBlameFunction(ifstream & bI)
{
//////////////////////////////////////////////////////////////////
//  cout<<"parseBlameFunction is executed !"<<endl;
//////////////////////////////////////////////////////////////////
  std::string line;
  
  if (BP->sampledModules.count(moduleName.c_str()))
  {
    std::cout<<"MODULE "<<moduleName<<" FOUND as one containing a sampled function "<<line<<std::endl;
  }
  else
  {
    std::cout<<"MODULE NOT FOUND, no sampled function - "<<moduleName<<" "<<strlen(moduleName.c_str())<<std::endl;
    return NULL;
  }
  
  //----
  //BEGIN F_B_LINENUM
  getline(bI, line);
  
  // Begin Line Num
  getline(bI, line);
  int bln = atoi(line.c_str());
  setBLineNum(bln);
  
  //END F_B_LINENUM
  getline(bI, line);
  
  //----
  //BEGIN F_E_LINENUM
  getline(bI, line);
  
  // End Line Num
  getline(bI, line);
  int eln = atoi(line.c_str());
  setELineNum(eln);
  
  //END F_E_LINENUM
  getline(bI, line);
  
  //----
  //BEGIN F_BPOINT
  getline(bI, line);
  
  // function blame point
  getline(bI, line);
  int bPoint = atoi(line.c_str());
  setBlamePoint(bPoint);
  
  //END F_BPOINT
  getline(bI, line);
  
  //----
  //BEGIN ALL_LINES
  getline(bI, line);
  
  //record function allLines
  getline(bI, line);
  stringstream ss(line);
  int LN;
  while (ss >> LN) {
    allLineNums.insert(LN);
  }

  //END ALL_LINES
  getline(bI, line);

  //----
  bool proceed = true;
  while (proceed)
  {
    // SHOULD EITHER GET "BEGIN VAR" or "END FUNC"
    getline(bI, line);
    if (line.find("BEGIN VAR") != std::string::npos)
    {
      // BEGIN V_NAME
      getline(bI, line);
      
      // Get Variable Name
      getline(bI, line);

      VertexProps * vp = findOrCreateVP(line);
      
      // END V_NAME
      getline(bI, line);
      
      vp->parseVertex(bI, this);  //this points to this particular object of the BlameFunction class
      vp->adjustVertex();
    }
    else
      proceed = false;
  }
  //added by Hui 12/30/15 for testing callNodes
/*  std::vector<VertexProps*>::iterator vp_i;
  cout<<"The callNodes of function "<<this->getName()<<" are:"<<endl;
  for(vp_i = this->callNodes.begin(); vp_i != this->callNodes.end(); vp_i++){
    VertexProps *node = *vp_i;
    cout<<node->name<<" declaredLine="<<node->declaredLine<<endl;
  }
*/
  /////////////////////////////////////////////

  return this;
}


void BlameFunction::printParsed(std::ostream &O)
{
  O<<"BEGIN FUNC "<<std::endl;
  
  O<<"BEGIN F_NAME "<<std::endl;
  O<<realName<<std::endl;
  O<<"END F_NAME"<<std::endl;
  
  O<<"BEGIN M_NAME_PATH "<<std::endl;
  O<<modulePathName<<std::endl;
  O<<"END M_NAME_PATH "<<std::endl;
  
  O<<"BEGIN_M_NAME"<<std::endl;
  O<<moduleName<<std::endl;
  O<<"END M_NAME"<<std::endl;
  
  O<<"BEGIN F_B_LINENUM"<<std::endl;
  O<<beginLineNum<<std::endl;
  O<<"END F_B_LINENUM"<<std::endl;
  
  O<<"BEGIN F_E_LINENUM"<<std::endl;
  O<<endLineNum<<std::endl;
  O<<"END F_E_LINENUM"<<std::endl;
  
    
  VertexHash::iterator hash_vp_i;

  // Print out all of the vertices
  for (hash_vp_i = allVertices.begin(); hash_vp_i != allVertices.end(); hash_vp_i++)
  {
    VertexProps * vp = (*hash_vp_i).second;

    vp->printParsed(O);
  }
}




void BlameFunction::addBlameToFieldParents(VertexProps * vp, std::set<VertexProps*> & blamees, short fromWhere)
{
  //std::cout<<"In ABTFP"<<std::endl;
  if (vp->nStatus[EXIT_VAR_FIELD] || vp->nStatus[LOCAL_VAR_FIELD]) {
    std::set<VertexProps *> visited;
    VertexProps *upPtr = vp->fieldUpPtr;
    while (upPtr != NULL && visited.count(upPtr) == 0) {//Here visited is new for each call of this func
                                                   //TOCHECK: shall we make it global for each frame???
      visited.insert(upPtr);
      //std::cout<<"Adding field parent "<<upPtr->name<<std::endl;
      upPtr->addedFromWhere = fromWhere;
#ifdef DEBUG_BLAMEES
      cout<<"Blamees insert "<<upPtr->name<<" in addBlameToFieldParents"<<endl;
#endif
      blamees.insert(upPtr);
      
      // Recursive parent situation, field is parent to container struct
      // TODO: Fix why this happens at the source
      if (upPtr->fields.count(upPtr->fieldUpPtr) && upPtr->eStatus > NO_EXIT) {
        cout<<"Wooh,what's wrong with "<<upPtr->name<<", its fieldUpPtr: "<<upPtr->fieldUpPtr->name<<endl;
        upPtr = NULL;
      }
      else {
        VertexProps *oldVP = upPtr;
        upPtr = upPtr->fieldUpPtr;
#ifdef DEBUG_UPPTRS
        if(upPtr != NULL)
            cout<<"In place1: new upPtr="<<upPtr->name<<endl;
#endif
        if (upPtr == NULL) {
          std::set<VertexProps *>::iterator s_vp_i;
          for (s_vp_i = oldVP->aliases.begin(); s_vp_i != oldVP->aliases.end(); s_vp_i++) {
            VertexProps * vpAlias = *s_vp_i;
            //calcSEDWritesRecursive(ev, child, visited);
            if (vpAlias != oldVP) {
              ////std::cout<<"21a - Alias "<<vpAlias->name<<std::endl;
              
              if (vpAlias->fieldUpPtr != NULL) {
                upPtr = vpAlias->fieldUpPtr;
#ifdef DEBUG_UPPTRS
                cout<<"In place2: new upPtr="<<upPtr->name<<endl;
#endif
                upPtr->temptempFields.insert(oldVP);

                break;
              }
            }
          }
        }
        
        if (upPtr == NULL) {
          if (oldVP->aliasUpPtr != NULL) {
            ////std::cout<<"22a - AliasUpPtr "<<oldVP->aliasUpPtr->name<<std::endl;
            upPtr = oldVP->aliasUpPtr;
#ifdef DEBUG_UPPTRS
            cout<<"In place3: new upPtr="<<upPtr->name<<endl;
#endif
            // TODO: Need to change this intermediary step
            upPtr->temptempFields.insert(oldVP);
          }
        }
      }
    }
  }
}


// Some of the fields are just repeats of existing exit fields
bool BlameFunction::notARepeat(VertexProps *vp, std::set<VertexProps *> &blamees)
{
  return true; //ALWAYS true ?????
  
  std::set<VertexProps * >::iterator set_vp_i;
  
  std::string vpFull = getFullStructName(vp);
  
  for (set_vp_i = blamees.begin(); set_vp_i != blamees.end(); set_vp_i++) {
    std::string blameeFull =   getFullStructName(*set_vp_i);
    
    if ( vpFull.compare(blameeFull) == 0) {
      //std::cout<<"Found duplicate for "<<vpFull<<" raw "<<vp->name<<std::endl;
      return false;
    }
  }
  
  return true;
}


void BlameFunction::determineBlameHolders(std::set<VertexProps *> & blamees,std::set<VertexProps *> & oldBlamees,VertexProps * callNode,int lineNum, short isBlamePoint, std::set<VertexProps *> &localBlamees,std::set<VertexProps *> &DQblamees)
{
  std::set<VertexProps *> visited;
  bool foundOne = false;
  
  VertexHash::iterator hash_vp_i;
  std::set<VertexProps *>::iterator set_vp_i;
  // we can have multiple varibales to be blamed within a single line of src code
  if (allLines.count(lineNum) > 0)  
    foundOne = true;
  pair<multimap<int,VertexProps *>::iterator, multimap<int, VertexProps *>::iterator> ii;
  multimap<int, VertexProps *>::iterator it; //Iterator to be used along with ii
  ii = allLines.equal_range(lineNum); //We get the first and last entry in multimap;
  //ii.first, ii.second are pair<int, VertexProps*> where int ln=lineNum
  
  for(it = ii.first; it != ii.second; ++it) {
    VertexProps * vp = it->second;
#ifdef DEBUG_DETER_BH
    cout<<"In allLines: Key="<<it->first<<", Value="<<it->second->name<<
        ", eStatus="<<it->second->eStatus<<endl;
#endif
    if (vp->eStatus > NO_EXIT || vp->nStatus[EXIT_VAR_FIELD]) {
#ifdef DEBUG_DETER_BH
      cout<<"determinBlameHolders: in case 1"<<endl;
#endif
      vp->findSEExits(blamees);
      // Make sure the callee EV param num matches the caller param num
      // notARepeat always return true, if callNode==NULL, tansferFuncApplies also return true
      if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees)) { 
#ifdef DEBUG_BLAMEES
        cout<<"Blamees insert "<<vp->name<<" in determineBlameHolders(1)"<<endl;
#endif
        blamees.insert(vp);
        vp->addedFromWhere = 1;
        addBlameToFieldParents(vp, blamees, 11);
      }
      else {
        vp->addedFromWhere = 61;
        DQblamees.insert(vp);
      }
    }
    else if (isBlamePoint && (vp->nStatus[EXIT_VAR_FIELD] || 
            vp->nStatus[LOCAL_VAR] || vp->nStatus[LOCAL_VAR_FIELD])) {
#ifdef DEBUG_DETER_BH
      cout<<"determinBlameHolders: in case 2"<<endl;
#endif
      vp->findSEExits(blamees);
        
      // Make sure the callee EV param num matches the caller param num
      //vp could be the local var in this frame holding the retval from callNode
      if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees)){
#ifdef DEBUG_BLAMEES
        cout<<"Blamees insert "<<vp->name<<" in determineBlameHolders(2)"<<endl;
#endif
        blamees.insert(vp);
        vp->addedFromWhere = 5;
        addBlameToFieldParents(vp, blamees, 15);
            
      }
      else {
        vp->addedFromWhere = 65;
        DQblamees.insert(vp);
      }          
    }
    else if (vp->nStatus[LOCAL_VAR] || vp->nStatus[LOCAL_VAR_FIELD]) {
#ifdef DEBUG_DETER_BH      
        cout<<"determinBlameHolders: in case 3"<<endl;
#endif
        vp->findSEExits(blamees);

        localBlamees.insert(vp);
        vp->addedFromWhere = 21;
        addBlameToFieldParents(vp, localBlamees, 31);
    }
  }// end of for loop
  
  
#ifdef DEBUG_DETER_BH
  cout<<"Temp lines count "<<tempLines.count(lineNum)<<std::endl;
#endif
  //tempLines has pairs of (call line, blameParamNode) 
  ii = tempLines.equal_range(lineNum); //We get the first and last entry in ii;
  
  for(it = ii.first; it != ii.second; ++it) {
#ifdef DEBUG_DETER_BH
    cout<<"In tempLines: Key = "<<it->first<<"    Value = "<<it->second->name<<endl;
#endif
    VertexProps * vp = it->second;
    if (vp->eStatus > NO_EXIT || vp->nStatus[EXIT_VAR_FIELD]) {
      vp->findSEExits(blamees);
        
      // Make sure the callee EV param num matches the caller param num
      if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees)) {
#ifdef DEBUG_BLAMEES
        cout<<"Blamees insert "<<vp->name<<" in determineBlameHolders(3)"<<endl;
#endif
        blamees.insert(vp);
        vp->addedFromWhere = 3;
        addBlameToFieldParents(vp, blamees, 13);
      }
      else {
        vp->addedFromWhere = 63;
        DQblamees.insert(vp);
      }
    }
    
    else if (isBlamePoint && (vp->nStatus[EXIT_VAR_FIELD] || 
            vp->nStatus[LOCAL_VAR] || vp->nStatus[LOCAL_VAR_FIELD])) {
      // This comes in handy when local variables are passed by reference,
      //  otherwise that line number would be masked by the line number of
      //  the function
        
      vp->findSEExits(blamees);
          
      // Make sure the callee EV param num matches the caller param num
      if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees)) {
#ifdef DEBUG_BLAMEES
        cout<<"Blamees insert "<<vp->name<<" in determineBlameHolders(4)"<<endl;
#endif
        blamees.insert(vp);
        vp->addedFromWhere = 7;
        addBlameToFieldParents(vp, blamees, 17);
      }
      else {
        vp->addedFromWhere = 67;
        DQblamees.insert(vp);
      }          
    }
    
    else if (vp->nStatus[LOCAL_VAR] || vp->nStatus[LOCAL_VAR_FIELD]) {
      vp->findSEExits(blamees);
#ifdef DEBUG_BLAMEES
      std::cout<<"Blamees insert(L3) "<<vp->name<<std::endl;
#endif
      localBlamees.insert(vp);
      vp->addedFromWhere = 23;
      addBlameToFieldParents(vp, localBlamees, 33);
    }
  } // end of tempLines
  

#ifdef DEBUG_DETER_BH
  cout<<"Size of hasParams "<<hasParams.size()<<std::endl;
#endif
  for (set_vp_i = hasParams.begin(); set_vp_i != hasParams.end(); set_vp_i++) {
    VertexProps * vp = (*set_vp_i);
#ifdef DEBUG_BLAMEES
    cout<<"Checking inside "<<this->getName()<<": hasParams "<<vp->name<<", vp->eStatus="<<vp->eStatus<<", lineNum="<<lineNum<<endl;
#endif
    visited.clear();
    
    if (vp->eStatus > NO_EXIT || vp->nStatus[EXIT_VAR_FIELD]) {
      if (vp->findBlamedExits(visited, lineNum)) {
        vp->findSEExits(blamees);
        
        // Make sure the callee EV param num matches the caller param num
        if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees)){
#ifdef DEBUG_BLAMEES
          cout<<"Blamees insert "<<vp->name<<" in determineBlameHolders(5)"<<endl;
#endif
          blamees.insert(vp);
          vp->addedFromWhere = 4;
          addBlameToFieldParents(vp, blamees, 14);
        }
        else {
          vp->addedFromWhere = 64;
          DQblamees.insert(vp);
        }        
      }
    }
    else if (isBlamePoint) {
      if (vp->nStatus[EXIT_VAR_FIELD] || vp->nStatus[LOCAL_VAR] ||
          vp->nStatus[LOCAL_VAR_FIELD]) {
        if (vp->findBlamedExits(visited, lineNum)) {
          vp->findSEExits(blamees);
          
          // Make sure the callee EV param num matches the caller param num
          if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees)){
#ifdef DEBUG_BLAMEES
            cout<<"Blamees insert "<<vp->name<<" in determineBlameHolders(6)"<<endl;
#endif
            blamees.insert(vp);
            vp->addedFromWhere = 8;
            addBlameToFieldParents(vp, blamees, 18);
            
          }
          else {
            vp->addedFromWhere = 68;
            DQblamees.insert(vp);
          }            
        } //end of findBlamedExits
      }    
    }
    // Straight up Local Variables that don't count towards blame
    else if (vp->nStatus[LOCAL_VAR] || vp->nStatus[LOCAL_VAR_FIELD]) {
      if (vp->findBlamedExits(visited, lineNum)) {
        vp->findSEExits(blamees);
        localBlamees.insert(vp);
        vp->addedFromWhere = 24;
        addBlameToFieldParents(vp, localBlamees, 34);
      }
    }        
  }  
  
  
  bool proceed = false;
  // Only care about SE when only blamee is a return val
#ifdef DEBUG_DETER_BH
  cout<<"Before proceed, blamees.size= "<<blamees.size()<<endl;
#endif
  if (blamees.size() == 0)
    proceed = true;
  //the only blamee in this stackframe(func) is the return value to the call node
  if (blamees.size() == 1) {
    std::set<VertexProps *>::iterator set_vp_i;
    for (set_vp_i = blamees.begin(); set_vp_i != blamees.end(); set_vp_i++) {
      VertexProps *vp = *set_vp_i;
      if (vp->eStatus == EXIT_VAR_RETURN)
        proceed = true;
    }  
  }
  
  
  if (proceed) {
#ifdef DEBUG_DETER_BH
      cout<<"I'm in proceed=true"<<endl;
#endif
    // Populate side effect relations for the blame function
    populateTempSideEffectsRelations();
    calcAggregateLN_SE();
    
    // Populate Edges
    for (hash_vp_i = allVertices.begin(); hash_vp_i != allVertices.end(); hash_vp_i++) {
      VertexProps * vp = (*hash_vp_i).second;
      
      // The side effects play by a different set of rules then the others, deal with aliases
      //vp->findSEExits(blamees);
      if (vp->eStatus > NO_EXIT || vp->nStatus[EXIT_VAR_FIELD]) {
        if (vp->seLineNumbers.count(lineNum)) {
          vp->addedFromWhere = 40;
          // Make sure the callee EV param num matches the caller param num
          //if (transferFuncApplies(vp, oldBlamees, callNode)) {
#ifdef DEBUG_BLAMEES
          cout<<"Blamees insert "<<vp->name<<" in determineBlameHolders(7)"<<endl;
#endif
          blamees.insert(vp);
          addBlameToFieldParents(vp, blamees, 50);
          //}
        }
      }
      else if ( isBlamePoint ) {
        if (vp->nStatus[EXIT_VAR_FIELD] || vp->nStatus[LOCAL_VAR] ||
            vp->nStatus[LOCAL_VAR_FIELD]) {
          if (vp->seLineNumbers.count(lineNum)) {
            // Make sure the callee EV param num matches the caller param num
            if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees))  {
              vp->addedFromWhere = 41;
#ifdef DEBUG_BLAMEES
              cout<<"Blamees insert "<<vp->name<<" in determineBlameHolders(8)"<<endl;
#endif
              blamees.insert(vp);
              addBlameToFieldParents(vp, blamees, 51);
            }
          }
        }    
      }
      else if (vp->nStatus[EXIT_VAR_FIELD] || vp->nStatus[LOCAL_VAR] ||
               vp->nStatus[LOCAL_VAR_FIELD]) {
        
        if (vp->seLineNumbers.count(lineNum)) {
          vp->addedFromWhere = 42;
          localBlamees.insert(vp);
          addBlameToFieldParents(vp, localBlamees, 52);
        }
      }
    }  
  }// end of proceed=true
  
  if (blamees.size() == 0) {
    if (DQblamees.size() > 0) {
#ifdef DEBUG_BLAMEES
      cout<<"Insert blamee again from DQblamees in determineBlameHolders(9)!"<<endl;
#endif
      blamees.insert(DQblamees.begin(), DQblamees.end());
    }
  }
 
  //added by Hui 02/09/16
 /*
  std::set<VertexProps *>::iterator bi, ai;
  for(bi = blamees.begin(); bi != blamees.end(); bi++) {
    VertexProps *vp = *bi;
    for(ai = vp->aliases.begin(); ai != vp->aliases.end(); ai++) {
      if((*ai)->eStatus == EXIT_VAR_GLOBAL)
        blamees.insert((*ai));
    }
  }
*/
  if (foundOne == false) {
    std::cerr<<"No VPs have the line number "<<std::endl;
  }

}


/*
void BlameFunction::determineBlameHolders(std::set<VertexProps *> & blamees,
                                          std::set<VertexProps *> & oldBlamees,
                                          VertexProps * callNode,
                                          int lineNum, short isBlamePoint, std::set<VertexProps *> & localBlamees,
                                          std::set<VertexProps *> & DQblamees)
{
  //std::cout<<"In determineBlameHolders for "<<getName()<<"  LN - "<<lineNum<<std::endl;
  
  //std::vector<VertexProps *>::iterator vec_vp_i;
  std::set<VertexProps *> visited;
  
  bool foundOne = false;
    
  VertexHash::iterator hash_vp_i;
    
  // Populate Edges
  for (hash_vp_i = allVertices.begin(); hash_vp_i != allVertices.end(); hash_vp_i++)
  {
    VertexProps * vp = (*hash_vp_i).second;
  
    visited.clear();
    
    //std::cout<<"DBRaw VP - "<<vp->name<<" "<<vp->declaredLine<<" "<<vp->tempLine<<std::endl;
    
    if (vp->lineNumbers.count(lineNum) || vp->declaredLine == lineNum || vp->tempLine == lineNum)
    {
      //std::cout<<"DBH VP - "<<vp->name<<" "<<vp->declaredLine<<" "<<vp->tempLine<<std::endl;
      //std::cout<<"EStatus - "<<vp->eStatus<<std::endl;
      //std::cout<<"NStatus ";
      //for (int a = 0; a < NODE_PROPS_SIZE; a++)
      //{
        //std::cout<<vp->nStatus[a]<<" ";
      //}
      //std::cout<<std::endl;
      foundOne = true;
    }
    // The side effects play by a different set of rules then the others, deal with aliases
    //vp->findSEExits(blamees);
    
    if (vp->eStatus > NO_EXIT || vp->nStatus[EXIT_VAR_FIELD])
    {
      if (vp->lineNumbers.count(lineNum)) 
      {
        //std::cout<<"Blamees insert(1) "<<vp->name<<std::endl;
        vp->findSEExits(blamees);
        
        
        // Make sure the callee EV param num matches the caller param num
        if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees))
        {
          blamees.insert(vp);
          vp->addedFromWhere = 1;
          addBlameToFieldParents(vp, blamees, 11);
        }
        else
        {
          vp->addedFromWhere = 61;
          DQblamees.insert(vp);
        }
      }
      else if (vp->tempLine == lineNum)
      {
        //std::cout<<"Blamees insert(3) "<<vp->name<<std::endl;
        
        vp->findSEExits(blamees);
        
        // Make sure the callee EV param num matches the caller param num
        if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees))        {
          blamees.insert(vp);
          vp->addedFromWhere = 3;
          addBlameToFieldParents(vp, blamees, 13);
          
        }
        else
        {
          vp->addedFromWhere = 63;
          DQblamees.insert(vp);
        }
      }
      else if (vp->findBlamedExits(visited, lineNum))
      {
        
              
        vp->findSEExits(blamees);
        
        // Make sure the callee EV param num matches the caller param num
        if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees))        {
          blamees.insert(vp);
          vp->addedFromWhere = 4;
          addBlameToFieldParents(vp, blamees, 14);
          
        }
        else
        {
          vp->addedFromWhere = 64;
          DQblamees.insert(vp);
        }        
      }
      
    }
    else if ( isBlamePoint )
    {
      if (vp->nStatus[EXIT_VAR_FIELD] || vp->nStatus[LOCAL_VAR] ||
          vp->nStatus[LOCAL_VAR_FIELD])
      {
        if (vp->lineNumbers.count(lineNum)) 
        {
          //std::cout<<"Blamees insert(5) "<<vp->name<<std::endl;
          vp->findSEExits(blamees);
          
          
          // Make sure the callee EV param num matches the caller param num
          if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees))          {
            blamees.insert(vp);
            vp->addedFromWhere = 5;
            addBlameToFieldParents(vp, blamees, 15);
            
          }
          else
          {
            vp->addedFromWhere = 65;
            DQblamees.insert(vp);
          }          
        }
          // This comes in handy when local variables are passed by reference,
        //  otherwise that line number would be masked by the line number of
        //  the function
        else if (vp->tempLine == lineNum)
        {
          //std::cout<<"Blamees insert(7) "<<vp->name<<std::endl;
          
          vp->findSEExits(blamees);
          
          // Make sure the callee EV param num matches the caller param num
          if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees))          {
            blamees.insert(vp);
            vp->addedFromWhere = 7;
            addBlameToFieldParents(vp, blamees, 17);
            
          }
          else
          {
            vp->addedFromWhere = 67;
            DQblamees.insert(vp);
          }          
        }
        else if (vp->findBlamedExits(visited, lineNum))
        {
          //std::cout<<"Blamees insert(8) "<<vp->name<<std::endl;
          
          vp->findSEExits(blamees);
          
          // Make sure the callee EV param num matches the caller param num
          if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees))          {
            blamees.insert(vp);
            vp->addedFromWhere = 8;
            addBlameToFieldParents(vp, blamees, 18);
            
          }
          else
          {
            vp->addedFromWhere = 68;
            DQblamees.insert(vp);
          }            
        }
      }    
    }
    // Straight up Local Variables that don't count towards blame
    else if (vp->nStatus[LOCAL_VAR] || vp->nStatus[LOCAL_VAR_FIELD])
    {
      if (vp->lineNumbers.count(lineNum)) 
      {
              vp->findSEExits(blamees);

        //std::cout<<"Blamees insert(L1) "<<vp->name<<std::endl;
        localBlamees.insert(vp);
        vp->addedFromWhere = 21;
        addBlameToFieldParents(vp, localBlamees, 31);
      }
      else if (vp->declaredLine == lineNum)
      {
        vp->findSEExits(blamees);

        //std::cout<<"Blamees insert(L2) "<<vp->name<<std::endl;
        localBlamees.insert(vp);
        vp->addedFromWhere = 22;
        addBlameToFieldParents(vp, localBlamees, 32);
      }
      // This comes in handy when local variables are passed by reference,
      //  otherwise that line number would be masked by the line number of
      //  the function
      else if (vp->tempLine == lineNum)
      {
        vp->findSEExits(blamees);

        //std::cout<<"Blamees insert(L3) "<<vp->name<<std::endl;
        localBlamees.insert(vp);
        vp->addedFromWhere = 23;
        addBlameToFieldParents(vp, localBlamees, 33);
      }
      else if (vp->findBlamedExits(visited, lineNum))
      {
              vp->findSEExits(blamees);

      
        //std::cout<<"Blamees insert(L4) "<<vp->name<<std::endl;
        localBlamees.insert(vp);
        vp->addedFromWhere = 24;
        addBlameToFieldParents(vp, localBlamees, 34);
      }
    }        
  }  
  
  // Check for side effect relations if necessary
  // TODO: blamees.size() == 0 || one blamee and it is a retval
  
  bool proceed = false;
  
  // SE MAX
  //if (blamees.size() <= 100)
  //  proceed = true;
  
  // ------
  
  // No SE unless nothing else is found
  // if (blamees.size() == 0)
  //  proceed   = true;
  
  // -------
  
  // Only care about SE when only blamee is a return val
  if (blamees.size() == 0)
    proceed = true;
  
  if (blamees.size() == 1)
  {
    std::set<VertexProps *>::iterator set_vp_i;
    for (set_vp_i = blamees.begin(); set_vp_i != blamees.end(); set_vp_i++)
    {
      VertexProps * vp = *set_vp_i;
      if (vp->eStatus == EXIT_VAR_RETURN)
        proceed = true;
    }  
  }
  
  
  if (proceed)
  {
    // Populate side effect relations for the blame function
    populateTempSideEffectsRelations();

    calcAggregateLN_SE();
    
    // Populate Edges
    for (hash_vp_i = allVertices.begin(); hash_vp_i != allVertices.end(); hash_vp_i++)
    {
      VertexProps * vp = (*hash_vp_i).second;

      //if (vp->lineNumbers.count(lineNum) || vp->declaredLine == lineNum || vp->tempLine == lineNum)
      //foundOne = true;
      
      // The side effects play by a different set of rules then the others, deal with aliases
      //vp->findSEExits(blamees);
      
      if (vp->eStatus > NO_EXIT || vp->nStatus[EXIT_VAR_FIELD])
      {
        if (vp->seLineNumbers.count(lineNum)) 
        {
          //std::cout<<"Blamees insert(9) "<<vp->name<<std::endl;
          
          vp->addedFromWhere = 40;
          // Make sure the callee EV param num matches the caller param num
          //if (transferFuncApplies(vp, oldBlamees, callNode))
          //{
          blamees.insert(vp);
          addBlameToFieldParents(vp, blamees, 50);
          //}
        }
      }
      else if ( isBlamePoint )
      {
        if (vp->nStatus[EXIT_VAR_FIELD] || vp->nStatus[LOCAL_VAR] ||
            vp->nStatus[LOCAL_VAR_FIELD])
        {
          if (vp->seLineNumbers.count(lineNum)) 
          {
            
            // Make sure the callee EV param num matches the caller param num
            if (transferFuncApplies(vp, oldBlamees, callNode) && notARepeat(vp, blamees))  {
              //std::cout<<"Blamees insert(10) "<<vp->name<<std::endl;
              
              vp->addedFromWhere = 41;
              blamees.insert(vp);
              addBlameToFieldParents(vp, blamees, 51);
              
            }
          }
        }    
      }
      else if (vp->nStatus[EXIT_VAR_FIELD] || vp->nStatus[LOCAL_VAR] ||
               vp->nStatus[LOCAL_VAR_FIELD])
      {
        
        if (vp->seLineNumbers.count(lineNum)) 
        {
          //std::cout<<"Blamees insert(L10) "<<vp->name<<std::endl;
          vp->addedFromWhere = 42;
          localBlamees.insert(vp);
          addBlameToFieldParents(vp, localBlamees, 52);
        }
      }
      
    }  
  }
  
  if (blamees.size() == 0)
  {
    if (DQblamees.size() > 0)
    {
      blamees.insert(DQblamees.begin(), DQblamees.end());
    }
  }
  
  
  
  if (foundOne == false)
  {
    //std::cerr<<"No VPs have the line number "<<std::endl;
    //std::cout<<"No VPs have the line number "<<std::endl;
  }
  
}

*/

void BlameFunction::resolvePAForParamNode(VertexProps *param)
{
  std::cout<<"Func: "<<getName()<<", #EVs="<<exitVariables.size()<<", calling rPAPN for "<<param->name<<endl;
  std::vector<VertexProps *>::iterator ivh_i;
  std::set<VertexProps *>::iterator ivh_i2, ivh_i3;
  for (ivh_i = exitVariables.begin(); ivh_i != exitVariables.end(); ivh_i++) {
    VertexProps *ivp = *ivh_i;
    std::cout<<"Checking EV "<<ivp->name<<endl;
    if (ivp->eStatus >= EXIT_VAR_PARAM) {
      for (ivh_i2 = ivp->PPAs.begin(); ivh_i2 != ivp->PPAs.end(); ivh_i2++) {
        VertexProps *ivp2 = *ivh_i2;
        std::cout<<"Checking EV's PPA "<<ivp2->name<<endl;
        // if this param is in some EV's PPAs, then we set all PPAs as pidAliases to param
        if (ivp2->name.compare(param->name) == 0) {
          //just for checking TOBEDELETED
          if (ivp2 != param) {
            std::cerr<<"CHECK: why name match but node mismatch for "<<param->name<<endl;
            return;
          }

          //first set the EV to be pid
          ivp->isPid = true;
          ivp->pidAliases.insert(ivp->PPAs.begin(), ivp->PPAs.end());
          //second set the EV's PPAs all to be pid
          for (ivh_i3 = ivp->PPAs.begin(); ivh_i3 != ivp->PPAs.end(); ivh_i3++) {
            VertexProps *ivp3 = *ivh_i3;
            ivp3->isPid = true;
            ivp3->pidAliases.insert(ivp); //EV's PPAs didn't include EV itself
            ivp3->pidAliases.insert(ivp->PPAs.begin(), ivp->PPAs.end());
            ivp3->pidAliases.erase(ivp3); //PPA's pidAliases shouldn't include itself
          }

          //check the result
          std::cout<<"We've resolved pidAliases for param: "<<param->name<<endl;
          for (ivh_i3 = param->pidAliases.begin(); ivh_i3 != param->pidAliases.end(); ivh_i3++)
            std::cout<<(*ivh_i3)->name<<" ";
          std::cout<<endl;
          return;
        } //found the EV
      } //end of all EV's PPAs
    } //EV is formalArg
  } //end of all EVs
}


void BlameFunction::handleTransferFunction(VertexProps *callNode, std::set<VertexProps *> &blamedParams)
{
  std::cout<<"Need to do transfer function for "<<callNode->name<<std::endl;
  
  std::set<VertexProps *>::iterator vec_int_i;
  
  std::set<int> blamers; // the values feeding into the blame
  std::set<int> blamed; // the values that are blamed for call
  std::set<int> blamedPidParams; //the paramNums that are Pids reflected by the callNode
  std::set<VertexProps *> blamerVP; 
  std::set<VertexProps *> blamedVP;
  //blamedParams are vars in the frame that maps to this callNode 
  //(storage for its formal arg)
  for (vec_int_i = blamedParams.begin(); vec_int_i != blamedParams.end(); vec_int_i++) {
    VertexProps * vp = (*vec_int_i);
    std::cout<<"blamedParams: "<<vp->name<<",eStatus="<<vp->eStatus
        <<" ,isPid="<<vp->isPid<<std::endl;
    
    if (vp->eStatus >= EXIT_VAR_GLOBAL) {//search blamed params in pre frames, 
                                     //if they are global variables/param/retval
      int paramNum = vp->eStatus - EXIT_VAR_PARAM;
      if (paramNum >= 0) {//if it's an EV in the callNode function(formal arg)
        //Added07/18/17: resolve Pids in this bf by checking the callNode's EV
        if (vp->isPid) 
          blamedPidParams.insert(paramNum);
        //---------------------^^^^^^---------------------------------------------//
        blamed.insert(paramNum);
        std::cout<<"inserted with paramNum="<<paramNum<<endl;
      }  
    }
  }
  
  std::cout<<std::endl;
  //Now calls are actual params for this callNode, so they are vars in the upper
  //frame to the one that maps to this callNode
  std::vector<FuncCall *>::iterator vec_fc_i;
  for (vec_fc_i = callNode->calls.begin(); vec_fc_i != callNode->calls.end(); vec_fc_i++) {
    FuncCall *fc = (*vec_fc_i);
    if (blamed.count(fc->paramNumber) > 0) {
      std::cout<<"Param Num "<<fc->paramNumber<<" is blamed "<<fc->Node->name
          <<", fc->Node->isPid="<<fc->Node->isPid<<std::endl;
      blamedVP.insert(fc->Node);
      //we need to know which acutal param is blamed for this func call
      fc->Node->paramIsBlamedForTheCall = true; 
      
      //makeup pids for "this" func, starting from fc->Node 07/18/17
      //only do once for each such fc->Node, since it'll be same in each function
      if (blamedPidParams.count(fc->paramNumber) && !fc->Node->isPid) {
        fc->Node->isPid = true;
        resolvePAForParamNode(fc->Node);
      }

      fc->Node->tempLine = callNode->declaredLine;
      tempLines.insert(pair<int, VertexProps *>(fc->Node->tempLine, fc->Node));
      
      // Propagate temp line up 
      std::set<VertexProps *> visited;
      fc->Node->propagateTempLineUp(visited, fc->Node->tempLine);
    }
    else{
      std::cout<<"Param Num "<<fc->paramNumber<<" is blamer"<<std::endl;
      blamerVP.insert(fc->Node);
    }
  }
  
  std::set<VertexProps *>::iterator set_vp_i;
  std::set<VertexProps *>::iterator set_vp_i2;
  
  for (set_vp_i = blamerVP.begin(); set_vp_i != blamerVP.end(); set_vp_i++) {
    VertexProps *bE = (*set_vp_i);
    for (set_vp_i2 = blamedVP.begin(); set_vp_i2 != blamedVP.end(); set_vp_i2++) {
      VertexProps * bD = *set_vp_i2;
      bD->tempChildren.insert(bE);
      bE->tempParents.insert(bD);
      
      tempChildrenHolder.insert(bD);
      tempParentsHolder.insert(bE);
      //bD->tempIsWritten = true;  // PRE-DISSERTATION
      std::cout<<bE->name<<" is tempChild to "<<bD->name<<std::endl;
    }
  }
}


void BlameFunction::clearPastData()
{
  
  //pair<multimap<int,VertexProps *>::iterator, multimap<int, VertexProps *>::iterator> ii;
  multimap<int, VertexProps *>::iterator it; //Iterator to be used along with ii
  //ii = tempLines.equal_range(lineNum); //We get the first and last entry in ii;
  //cout<<"\n\nPrinting all Joe and then erasing them"<<endl;
  
  for(it = tempLines.begin(); it != tempLines.end(); ++it)
  {
    VertexProps * vp = it->second;
    vp->tempLine = 0;
  }
  
  tempLines.clear();
    
    
  std::set<VertexProps *>::iterator set_vp_i;
  
  for (set_vp_i = tempSEs.begin(); set_vp_i != tempSEs.end(); set_vp_i++)
  {
    VertexProps * vp = *set_vp_i;
    vp->tempRelations.clear();
    vp->tempSELines.clear();
  }
  
  for (set_vp_i = tempSEParents.begin(); set_vp_i != tempSEParents.end(); set_vp_i++)
  {
    VertexProps * vp = *set_vp_i;
    vp->tempRelationsParent.clear();
  }

  for (set_vp_i = tempParentsHolder.begin(); set_vp_i != tempParentsHolder.end(); set_vp_i++)
  {
    VertexProps * vp = *set_vp_i;
    vp->tempParents.clear();
  }

  for (set_vp_i = tempChildrenHolder.begin(); set_vp_i != tempChildrenHolder.end(); set_vp_i++)
  {
    VertexProps * vp = *set_vp_i;
    vp->tempChildren.clear();
  }

    
  VertexHash::iterator hash_vp_i;

  // Populate Edges
  for (hash_vp_i = allVertices.begin(); hash_vp_i != allVertices.end(); hash_vp_i++)
  {
    VertexProps * vp = (*hash_vp_i).second;

    vp->seLineNumbers.clear();    
    vp->calcAggSE = false;
  }
}


std::string BlameFunction::getFullStructName(VertexProps * vp)
{
  // Already been calculated
  if (vp->fsName.size() > 0)
    return vp->fsName;
#ifdef DEBUG_GFSN0
  std::cout<<"In GFSN for "<<vp->name<<std::endl;
  //std::cout<<"vp->sField is "<<vp->sField<<std::endl;
#endif
  
  std::string fName;
  std::string aName;
  
  aName.clear();
    
  if (vp->fieldAlias != NULL && vp->eStatus == NO_EXIT)
  {
      
    if (vp->fieldAlias->sField != NULL)
    {
#ifdef DEBUG_GFSN
      std::cout<<"  7a - "<<vp->fieldAlias->sField->fieldName<<std::endl;
#endif
      aName.insert(0, vp->fieldAlias->sField->fieldName);
    }
    else
    {
      int pos = vp->fieldAlias->name.find_last_of('.');
        
      if (pos == std::string::npos)
        pos = 0;
      else
        pos++;
        
      std::string modName = vp->fieldAlias->name.substr(pos);
#ifdef DEBUG_GFSN  
      std::cout<<"  7b - "<<modName<<std::endl;
#endif
      aName.insert(0, modName);
    }
  }
    
  if (vp->sField == NULL)
  {
#ifdef DEBUG_GFSN
    std::cout<<"1a - vp->sField is NULL"<<std::endl;
#endif    
    // Uncomment these two lines to return behavior to old
    //fName = vp->name;
    //return fName;
    
    int pos = vp->name.find_last_of('.');
    
    if (pos == std::string::npos)
      pos = 0;
    else
      pos++;
    
    //fName = vp->name.substr(pos);
    
    
    if (aName.size() > 0 && aName != vp->name.substr(pos))
    {
      fName.insert(0,")");
      fName.insert(0, aName);
      fName.insert(0, "(");
    }
    
    fName.insert(0, vp->name.substr(pos));
  }
  else
  {
#ifdef DEBUG_GFSN
    std::cout<<"1b - vp->fsName is "<<vp->fsName<<std::endl;
    //fName = vp->sField->fieldName;
#endif
    if (aName.size() > 0 && aName != vp->sField->fieldName)
    {
      fName.insert(0,")");
      fName.insert(0, aName);
      fName.insert(0, "(");
    }
    
    fName.insert(0, vp->sField->fieldName);
  }
  
  std::set<VertexProps *> visited;
  VertexProps * upPtr = vp->fieldUpPtr;
  while (upPtr != NULL && visited.count(upPtr) == 0)
  {
    visited.insert(upPtr);
    fName.insert(0, ".");
    
    aName.clear();
    
    if (upPtr->fieldAlias != NULL && upPtr->eStatus == NO_EXIT)
    {
      
      if (upPtr->fieldAlias->sField != NULL)
      {
        std::cout<<"  5a - "<<upPtr->fieldAlias->sField->fieldName<<std::endl;
        aName.insert(0, upPtr->fieldAlias->sField->fieldName);
      }
      else
      {
        
        int pos = upPtr->fieldAlias->name.find_last_of('.');
        
        if (pos == std::string::npos)
          pos = 0;
        else
          pos++;
        
        std::string modName = upPtr->fieldAlias->name.substr(pos);
#ifdef DEBUG_GFSN  
        std::cout<<"  5b - "<<modName<<std::endl;
#endif
        aName.insert(0, modName);
      }
      
    }
    
      
    
    if (upPtr->sField != NULL)
    {
      std::cout<<"  2a - "<<upPtr->sField->fieldName<<std::endl;
      
      if (aName.size() > 0 && aName != upPtr->sField->fieldName)
      {
        fName.insert(0,")");
        fName.insert(0, aName);
        fName.insert(0, "(");
      }

      fName.insert(0, upPtr->sField->fieldName);
    }
    else
    {
      
      int pos = upPtr->name.find_last_of('.');
      
      if (pos == std::string::npos)
        pos = 0;
      else
        pos++;
      
      std::string modName = upPtr->name.substr(pos);
#ifdef DEBUG_GFSN      
      std::cout<<"  2b - "<<modName<<std::endl;
#endif
      if (aName.size() > 0 && aName != modName)
      {
        fName.insert(0,")");
        fName.insert(0, aName);
        fName.insert(0, "(");
      }

      fName.insert(0, modName);
    }
    
    
    // Recursive parent situation, field is parent to container struct
    // TODO: Fix why this happens at the source
    if (upPtr->fields.count(upPtr->fieldUpPtr) && upPtr->eStatus > NO_EXIT)
      upPtr = NULL;
    else
    {
      VertexProps * oldVP = upPtr;
      upPtr = upPtr->fieldUpPtr;
      
      if (upPtr == NULL)
      {
        std::set<VertexProps *>::iterator s_vp_i;
        for (s_vp_i = oldVP->aliases.begin(); s_vp_i != oldVP->aliases.end(); s_vp_i++)
        {
          VertexProps * vpAlias = *s_vp_i;
          //calcSEDWritesRecursive(ev, child, visited);
          if (vpAlias != oldVP)
          {
#ifdef DEBUG_GFSN
              std::cout<<"21a - Alias "<<vpAlias->name<<std::endl;
#endif
            if (vpAlias->fieldUpPtr != NULL)
            {
              upPtr = vpAlias->fieldUpPtr;
              break;
            }
          }
        }
        
        if (upPtr == NULL)
        {
          if (oldVP->aliasUpPtr != NULL)
          {
#ifdef DEBUG_GFSN
              std::cout<<"22a - AliasUpPtr "<<oldVP->aliasUpPtr->name<<std::endl;
#endif
              upPtr = oldVP->aliasUpPtr;
          }
        }
      }
    }
    
    
    //upPtr = upPtr->fieldUpPtr;
  }
  
#ifdef DEBUG_GFSN0
  std::cout<<"After calculation GFSN of "<<vp->name
      <<": "<<fName<<std::endl;
#endif
  
  vp->fsName = fName;
  return fName;
}


void BlameFunction::outputParamInfo(std::ostream &O, VertexProps * vp)
{
  // First we put the number of the parameter number for the callee
  
  O<<"IP "<<" ";
  O<<vp->calleePar<<" ";
  
  O<<"OP "<<" ";
  std::set<int>::iterator set_i_i;
  for (set_i_i = vp->callerPars.begin(); set_i_i != vp->callerPars.end(); set_i_i++)
  {
    O<<*set_i_i<<" ";
  }
  
  /*
   int calleePar = 99;
   
   if (vp->eStatus >= EXIT_VAR_RETURN)
   {
   calleePar = vp->eStatus - EXIT_VAR_RETURN;
   }
   
   if (vp->nStatus[EXIT_VAR_FIELD])
   {
   VertexProps * upPtr = vp->fieldUpPtr;
   while (upPtr != NULL)
   {
   if (upPtr->eStatus >= EXIT_VAR_RETURN)
   {
   calleePar = upPtr->eStatus - EXIT_VAR_RETURN;
   break;
   }
   upPtr = upPtr->fieldUpPtr;
   }
   }
   
   
   O<<"IP "<<" ";
   O<<calleePar<<" ";
   
   // Then we output the parameter number for the caller(s)
   
   O<<"OP "<<" ";
   if (callNode != NULL)
   {
   std::vector<FuncCall *>::iterator vec_fc_i;
   for (vec_fc_i = callNode->calls.begin(); vec_fc_i != callNode->calls.end(); vec_fc_i++)
   {
   FuncCall * fc = (*vec_fc_i);
   if (vp->params.count(fc->Node))
   O<<fc->paramNumber<<" ";
   }
   }    
   */
  
}

bool BlameFunction::transferFuncApplies(VertexProps *caller, std::set<VertexProps *> &oldBlamees,
                                        VertexProps *callNode)
{
  if (callNode == NULL)
    return true;
  
  std::vector<FuncCall *>::iterator vec_fc_i;
  for (vec_fc_i = callNode->calls.begin(); vec_fc_i != callNode->calls.end(); vec_fc_i++) {
    FuncCall *fc = (*vec_fc_i);
    if (caller->params.count(fc->Node)) {
#ifdef DEBUG_TANSFUNC_APP
      std::cout<<"In TFA, examining "<<caller->name<<" "<<fc->paramNumber<<" "<<std::endl;
#endif
      std::set<VertexProps *>::iterator set_vp_i;
      for (set_vp_i = oldBlamees.begin(); set_vp_i != oldBlamees.end(); set_vp_i++) {
        VertexProps *vp = *set_vp_i;
        
        if (vp->isDerived)
          continue;
        
        int calleePar = 99;
        /*
        if (vp->eStatus >= EXIT_VAR_RETURN)
        {
          calleePar = vp->eStatus - EXIT_VAR_RETURN;
        }*/ //block is replaced by the one below, by Hui 03/15/16
        if(vp->eStatus == EXIT_VAR_RETURN) {
          calleePar = -1; //Changed from 0 to -1 12/19/16
        }
        else if(vp->eStatus >= EXIT_VAR_PARAM) {
          calleePar = vp->eStatus - EXIT_VAR_PARAM;
        } 
        
        else if (vp->nStatus[EXIT_VAR_FIELD]) { //changed from 'if' to 'else if' 12/19/16
          VertexProps *upPtr = vp->fieldUpPtr;
          std::set<VertexProps *> visited;
          while (upPtr != NULL && visited.count(upPtr) == 0) {
            visited.insert(upPtr);
            /*
            if (upPtr->eStatus >= EXIT_VAR_RETURN)
            {
              calleePar = upPtr->eStatus - EXIT_VAR_RETURN;
              break;
            }*/
            if (upPtr->eStatus == EXIT_VAR_RETURN) {
              calleePar = -1; //Changed from 0 to -1 12/19/16
              break;
            }
            else if (upPtr->eStatus >= EXIT_VAR_PARAM) {
              calleePar = upPtr->eStatus - EXIT_VAR_PARAM;
              break;
            } 
            upPtr = upPtr->fieldUpPtr;
          }
        }
        
        if (fc->paramNumber == calleePar) {  
#ifdef DEBUG_TANSFUNC_APP
          std::cout<<"In TFA, match found for "<<vp->name<<" and "<<caller->name<<" "<<calleePar<<std::endl;
#endif
          ////std::cout<<calleePar<<" ";
          return true;
        }
      }
    }
  }
  
  //std::cout<<"In TFA, no match found for "<<caller->name<<std::endl;
  return false;
}
 


VertexProps * BlameFunction::resolveSideEffectsCheckParentEV(VertexProps * vp, std::set<VertexProps *> & visited)
{
  ////std::cout<<"In resolveSideEffectsCheckParentEV for "<<vp->name<<" "<<vp->name<<std::endl;
  
  if (visited.count(vp) > 0)
    return NULL;
  
  visited.insert(vp);
  
  if (vp->eStatus >= EXIT_VAR_PARAM || vp->nStatus[EXIT_VAR_FIELD])
    return vp;
  
  if (vp->dpUpPtr != NULL && vp->dpUpPtr != vp)
    return resolveSideEffectsCheckParentEV(vp->dpUpPtr, visited);
  
  if (vp->dfaUpPtr != NULL && vp->dfaUpPtr != vp)
    return resolveSideEffectsCheckParentEV(vp->dfaUpPtr, visited);
  
  return NULL;
}


VertexProps * BlameFunction::resolveSideEffectsCheckParentLV(VertexProps * vp, std::set<VertexProps *> & visited)
{
  ////std::cout<<"In resolveSideEffectsCheckParentLV for "<<vp->name<<" "<<vp->name<<std::endl;
  
  if (visited.count(vp) > 0)
    return NULL;
  
  visited.insert(vp);
  
  if (vp->nStatus[LOCAL_VAR] || vp->nStatus[LOCAL_VAR_FIELD])
    return vp;
  
  if (vp->dpUpPtr != NULL && vp->dpUpPtr != vp)
    return resolveSideEffectsCheckParentLV(vp->dpUpPtr, visited);
  
  if (vp->dfaUpPtr != NULL && vp->dfaUpPtr != vp)
    return resolveSideEffectsCheckParentLV(vp->dfaUpPtr, visited);
  
  return NULL;
}


void BlameFunction::populateTempSideEffectsRelations()
{
  std::vector<VertexProps *>::iterator v_vp_i;
  for (v_vp_i = callNodes.begin(); v_vp_i != callNodes.end(); v_vp_i++)
  {
    VertexProps * callNode = *v_vp_i;
    
    size_t endChar = callNode->name.find("--");
    if (endChar != std::string::npos)
    {
      string callTrunc = callNode->name.substr(0, endChar);
      
      BlameFunction * bf = NULL;
      FunctionHash::iterator fh_i_check;  // blameFunctions;
      fh_i_check = BP->blameFunctions.find(callTrunc);
      if (fh_i_check != BP->blameFunctions.end())
      {
        bf = BP->blameFunctions[callTrunc];
      }
      
      if (bf == NULL)
      {
        if (callTrunc.find("tmp") == std::string::npos ) //function pointers
        {
          //TODO: Look into this
          //std::cerr<<"Call "<<callTrunc<<" not found!"<<std::endl;
          //std::cout<<"Call "<<callTrunc<<" not found!"<<std::endl;
        }
      }
      else
      {      
        //std::cout<<"Found SE info(2) for "<<bf->realName<<std::endl;
        if (callNode == NULL)
          continue;          
        
        std::vector<FuncCall *>::iterator vec_fc_i;
        
        ////std::cout<<"CALLS(2): "<<std::endl;
        
        // This is essentially going through all the parameters for the call
        for (vec_fc_i = callNode->calls.begin(); vec_fc_i != callNode->calls.end(); vec_fc_i++)
        {
          FuncCall *fc = (*vec_fc_i);
          
          std::vector<SideEffectRelation *>::iterator vec_ser_i;
          std::vector<SideEffectCall *>::iterator vec_sec_i;
          
          
          if (bf->sideEffectRelations.size() > 0)
          {
            for (vec_ser_i = bf->sideEffectRelations.begin(); vec_ser_i != bf->sideEffectRelations.end(); vec_ser_i++)
            {
              SideEffectRelation * ser = *vec_ser_i;
              std::set<SideEffectParam *>::iterator set_s_i;
              
              VertexProps * matchVP = NULL;
              int matchParamNum = 0;
              
              // Check to see if the parent SE relation matches the param number of the call
              if (ser->parent->paramNumber == fc->paramNumber)
              {
                matchVP = fc->Node;
                matchParamNum = fc->paramNumber;
              }
              
              // We do have a match and we proceed
              if (matchVP != NULL)
              {
                // We go through the relation set and
                //   we are going to assign things to based on the parameters to the function
                for (set_s_i = ser->children.begin(); set_s_i != ser->children.end(); set_s_i++)
                {
                  SideEffectParam * sep = *set_s_i;
                  
                  // We don't want to assign an alias to ourselves
                  if (sep->paramNumber != matchParamNum )
                  {
                    
                    std::vector<FuncCall *>::iterator vec_fc_i2;
                    
                    // We now go through all the other parameters to get VP information from them so we can
                    //  assign the proper alias
                    for (vec_fc_i2 = callNode->calls.begin(); vec_fc_i2 != callNode->calls.end(); vec_fc_i2++)
                    {
                      // A given parameter
                      FuncCall *fc2 = (*vec_fc_i2);
                      
                      // Want to make sure we find the parameter(from call) that matches the param (from side effect)
                      if (fc2->paramNumber == sep->paramNumber)
                      {
                        //matchSEP = sep;
                        
                        std::set<VertexProps *> visited2;
                        
                        // This gets the actual EV the param is associated with
                        VertexProps * evAnc = resolveSideEffectsCheckParentEV(fc2->Node, visited2);
                        visited2.clear();
                        
                        if (evAnc == NULL)
                          evAnc = resolveSideEffectsCheckParentLV(fc2->Node, visited2);
                        
                        if (evAnc == NULL)
                          evAnc = fc2->Node;
                        
                        visited2.clear();
                        // This gets the actual EV the match is associated with
                        
                        // We need to set this because of rules we have about propagating blame
                        // through read only data values, if we say it's written (which it is somewhere
                        // in the call) then we allow the line number of the statement to be propagated up
                        matchVP->tempIsWritten = true;
                        VertexProps * evAnc2 = resolveSideEffectsCheckParentEV(matchVP, visited2);
                        visited2.clear();
                        
                        if (evAnc2 == NULL)
                          evAnc2 = resolveSideEffectsCheckParentLV(matchVP, visited2);
                        
                        if (evAnc2 == NULL)
                          evAnc2 = matchVP;  
                        
                        
                        // vpValue;
                        evAnc2->tempRelations.insert(evAnc);
                        evAnc->tempRelationsParent.insert(evAnc2);
                        evAnc2->tempSELines.insert(callNode->declaredLine);
                        
                        tempSEs.insert(evAnc2);
                        tempSEParents.insert(evAnc);
                        
#ifdef DEBUG_SELINES
                        std::cout<<"Adding relation(from SE) from "<<evAnc2->name<<" to "<<evAnc->name;
                        std::cout<<" evAnc2's tempSELines insert "<<callNode->declaredLine<<std::endl;
#endif                       
                      }
                    }
                  }                    
                }
              }                
            }
          }
        }
      }          
    }
  }
}






//if (fieldApplies(matchSEP, blamees)
bool BlameFunction::fieldApplies(SideEffectParam * matchSEP, std::set<VertexProps *> & blamees)
{
  std::set<VertexProps *>::iterator set_vp_i;
  
  // if we're not dealing with fields it's moot, we just exit out
  if (matchSEP->paramName.find_last_of('.') == std::string::npos)
    return true;
  
  for (set_vp_i = blamees.begin(); set_vp_i != blamees.end(); set_vp_i++)
  {
    VertexProps * vp = (*set_vp_i);
    if (vp->isDerived)
      continue;
    
    if (vp->nStatus[LOCAL_VAR_FIELD] || vp->nStatus[EXIT_VAR_FIELD])
    {
      std::string sName = getFullStructName(vp);
      
      size_t found, found2;
      found=sName.find_last_of('.');
      if (found == std::string::npos)
        return false;
      
      std::string afterDot = sName.substr(found+1);
      found2 = matchSEP->paramName.find_last_of('.');
      
      std::string afterDot2 = matchSEP->paramName.substr(found2+1);
      
      if (afterDot == afterDot2)
        return true;
      
    }    
  }
  
  return false;
}

void BlameFunction::populateTempSideEffects(int lineNum, std::set<VertexProps *> & blamees)
{
  std::vector<VertexProps *>::iterator v_vp_i;
  for (v_vp_i = callNodes.begin(); v_vp_i != callNodes.end(); v_vp_i++)
  {
    VertexProps * checkCall = *v_vp_i;
    
    if (checkCall->domLineNumbers.count(lineNum) > 0)
    {  
      //std::cout<<"Call "<<checkCall->name<<" dominates this sample"<<std::endl;
      
      size_t endChar = checkCall->name.find("--");
      if (endChar != std::string::npos)
      {
        string callTrunc = checkCall->name.substr(0, endChar);
        
        BlameFunction * bf = NULL;
        FunctionHash::iterator fh_i_check;  // blameFunctions;
        fh_i_check = BP->blameFunctions.find(callTrunc);
        if (fh_i_check != BP->blameFunctions.end())
        {
          bf = BP->blameFunctions[callTrunc];
        }
        
        if (bf == NULL)
        {
          if (callTrunc.find("tmp") == std::string::npos)
          {
            // TODO: Look into this
            //std::cerr<<"Call "<<callTrunc<<" not found!(1)"<<std::endl;
            //std::cout<<"Call "<<callTrunc<<" not found!(1)"<<std::endl;
          }
        }
        else
        {      
          //std::cout<<"Found SE info for "<<bf->realName<<std::endl;
          
          VertexProps * callNode = checkCall;
          
          if (callNode == NULL)
            continue;          
          
          std::vector<FuncCall *>::iterator vec_fc_i;
          
          ////std::cout<<"CALLS: "<<std::endl;
          
          // This is essentially going through all the parameters for the call
          for (vec_fc_i = callNode->calls.begin(); vec_fc_i != callNode->calls.end(); vec_fc_i++)
          {
            FuncCall *fc = (*vec_fc_i);
            
            std::vector<SideEffectAlias *>::iterator vec_sea_i;
            
            // We want to check all the side effect aliases that are created by making this call
            if (bf->sideEffectAliases.size() > 0)
            {
              for (vec_sea_i = bf->sideEffectAliases.begin(); vec_sea_i != bf->sideEffectAliases.end();
                   vec_sea_i++)
              {
                SideEffectAlias * sea = *vec_sea_i;
                std::set<SideEffectParam *>::iterator set_s_i;
                
                VertexProps * matchVP = NULL;
                int matchParamNum = 0;
                SideEffectParam * matchSEP = NULL;
                
                // This is going through each individual alias from a given alias set to see
                //   if/where we have a match
                for (set_s_i = sea->aliases.begin(); set_s_i != sea->aliases.end(); set_s_i++)
                {
                  SideEffectParam * sep = *set_s_i;
                  
                  if (sep->paramNumber == fc->paramNumber)
                  {  
                    matchVP = fc->Node;
                    matchParamNum = fc->paramNumber;
                    matchSEP = sep;
                  }
                }
                
                // We do have a match and we proceed
                if (matchVP != NULL && (matchVP->eStatus > 0 || matchVP->nStatus[LOCAL_VAR] || 
                                        matchVP->nStatus[EXIT_VAR_FIELD] || matchVP->nStatus[LOCAL_VAR_FIELD]))
                {
                  // We go through the alias set again only now we have a specific alias VP that
                  //   we are going to assign things to based on the parameters to the function
                  for (set_s_i = sea->aliases.begin(); set_s_i != sea->aliases.end(); set_s_i++)
                  {
                    SideEffectParam * sep = *set_s_i;
                    
                    // We don't want to assign an alias to ourselves
                    if (sep->paramNumber != matchParamNum )
                    {
                      std::vector<FuncCall *>::iterator vec_fc_i2;
                      
                      // We now go through all the other parameters to get VP information from them so we can
                      //  assign the proper alias
                      for (vec_fc_i2 = callNode->calls.begin(); vec_fc_i2 != callNode->calls.end(); vec_fc_i2++)
                      {
                        // A given parameter
                        FuncCall *fc2 = (*vec_fc_i2);
                        
                        // Want to make sure we find the parameter(from call) that matches the param (from side effect)
                        if (fc2->paramNumber == sep->paramNumber)
                        {
                          //matchSEP = sep;
                          std::set<VertexProps *> visited2;
                          
                          if (fc2->Node == matchVP)
                            continue;
                          
                          
                          // This gets the actual EV the param is associated with
                          VertexProps * evAnc = resolveSideEffectsCheckParentEV(fc2->Node, visited2);
                          visited2.clear();
                          
                          if (evAnc == NULL)
                            evAnc = resolveSideEffectsCheckParentLV(fc2->Node, visited2);
                          
                          if (evAnc == NULL)
                            evAnc = fc2->Node;
                          
                          sep->vpValue = evAnc;
                          
                          
                          if (fieldApplies(matchSEP, blamees))
                          {
                            //matchVP->tempAliases.insert(evAnc);
                            matchVP->tempAliases.insert(sep);
                            
                            //std::cout<<"Adding alias(from SE) from "<<matchVP->name<<" to "<<evAnc->name<<std::endl;
                            //std::cout<<"MSEP - "<<matchSEP->paramNumber<<" "<<matchSEP->paramName<<" "<<matchSEP->calleeNumber<<std::endl;
                            //std::cout<<"SEP -  "<<sep->paramNumber<<" "<<sep->paramName<<" "<<sep->calleeNumber<<std::endl;
                          }
                        }
                      }
                    }                    
                  }
                }                
              }
            }
          }
        }  
      }          
    }
  }
}

// For any blamees(blamed for this callNode) in the current frame,
// if it's an exit var (has valid eStatus) then set calleePar for
// the next frame; Or if vp is EF, then get the calleePar of its fieldUpPtr(can
// be grand*Parent..) ...else set its callerPars for this callNode(since
// a same vp can be blamed for multi-calls in the same frame, it
// thus can have multi-callerPars("params"as real params in calls)
void BlameFunction::calcParamInfo(std::set<VertexProps *> &blamees, VertexProps *callNode)
{
  std::set<VertexProps *>::iterator set_vp_i;
  
  for (set_vp_i = blamees.begin(); set_vp_i != blamees.end(); set_vp_i++) {
    VertexProps *vp = *set_vp_i;
    if (vp->isDerived)
      continue;
    // means vp hs not related to EV yet
    if (vp->calleePar < -1) { //changed by Hui 04/18/16: from 0 to -1 as aligned to static analysis
      if(vp->eStatus == EXIT_VAR_RETURN) {
        vp->calleePar = -1; //changed by Hui 04/18/16: from 0 to -1 as aligned to static analysis
      }
      else if(vp->eStatus >= EXIT_VAR_PARAM) {//all blamed 'chpl_macro_tmp*' nodes
        vp->calleePar = vp->eStatus - EXIT_VAR_PARAM;
      } 
      
      else if (vp->nStatus[EXIT_VAR_FIELD]) { //changed from 'if' to 'else if' 12/19/16
        VertexProps * upPtr = vp->fieldUpPtr;
        std::set<VertexProps *> visited;
        //If vp isn't a param, then we check its upPtrs
        while (upPtr != NULL && visited.count(upPtr) == 0) {
          visited.insert(upPtr);
          /*
          if (upPtr->eStatus >= EXIT_VAR_RETURN)
          {
            vp->calleePar = upPtr->eStatus - EXIT_VAR_RETURN;
            break;
          }*/
          if(upPtr->eStatus == EXIT_VAR_RETURN) {
            vp->calleePar = -1;//changed by Hui 04/18/16: from 0 to -1 as aligned to static analysis
            break;
          }
          else if(upPtr->eStatus >= EXIT_VAR_PARAM){
            vp->calleePar = upPtr->eStatus - EXIT_VAR_PARAM;
            break;
          } 
 
          upPtr = upPtr->fieldUpPtr;
        }
      }
#ifdef DEBUG_CALCPARAM_INFO
      if (vp->calleePar >= -1)
        std::cout<<"CalcParamInfo: set "<<vp->calleePar<<" to "<<vp->name<<"'s calleePar"<<std::endl;
#endif 
    }
    
    // calculate the callerPar for the blamee(vp) in this frame
    if (callNode != NULL) {
      std::vector<FuncCall *>::iterator vec_fc_i;
      for (vec_fc_i = callNode->calls.begin(); vec_fc_i != callNode->calls.end(); vec_fc_i++) {
        FuncCall *fc = (*vec_fc_i);
        if (vp->params.count(fc->Node)) {
          vp->callerPars.insert(fc->paramNumber);
#ifdef DEBUG_CALCPARAM_INFO
          std::cout<<"CalcParamInfo: add "<<fc->paramNumber<<" to "<<vp->name<<"'s callerPars"<<std::endl;
#endif 
        }
      }
    }    
  }
}



void BlameFunction::clearTempFields(std::set<VertexProps *> & oldBlamees, BlameFunction * oldFunc)
{
  
  // recursion throws a big wrench into everything, so this accounts for that
  if (oldFunc == this)
    return;
  
  std::set<VertexProps *>::iterator set_vp_i;
  
  for (set_vp_i = oldBlamees.begin(); set_vp_i != oldBlamees.end(); set_vp_i++) {
    VertexProps * vp = (*set_vp_i);
    vp->tempFields.clear();
    vp->temptempFields.clear();
    //std::cout<<"Clearing from "<<vp<<std::endl;
    
    if (vp->isDerived) {
      //std::cout<<"Deleting "<<vp<<std::endl;
      delete vp;
    }
  }
  
}

void BlameFunction::outputFrameBlamees(std::set<VertexProps *> & blamees, std::set<VertexProps *> & localBlamees, 
                                       std::set<VertexProps *> & DQblamees, std::ostream &O)
{
  std::set<VertexProps *>::iterator set_vp_i;
  
  for (set_vp_i = blamees.begin(); set_vp_i != blamees.end(); set_vp_i++) {
    VertexProps * vp = (*set_vp_i);
#ifdef DEBUG_BLAMEES
    std::cout<<"Outputing blamee: "<<vp->name<<" estatus="<<vp->eStatus<<std::endl;
#endif
    //We don't add CE.XX as real blamee nodes to the final PARSE* files 07/31/17
    //if (endWithNumber(vp->name))
    if (anySubstrIsNumber(vp->name)) //more powerful version of endWithNumber
      continue;
    else if (vp->nStatus[EXIT_VAR_FIELD] || vp->nStatus[LOCAL_VAR_FIELD] || vp->isDerived) {  
      string fsn = getFullStructName(vp);
      if (anySubstrIsNumber(fsn)) //for field vp, it'll print out the full struct name on file
        continue;
    }

    if (vp->isDerived) {
      VertexProps *rootField = vp;
      std::set<VertexProps *> visited;
      
      while (rootField->fieldUpPtr != NULL && visited.count(rootField->fieldUpPtr) == 0) {
        visited.insert(rootField->fieldUpPtr);
        rootField = rootField->fieldUpPtr;
      }
      
      if  (rootField->eStatus >= EXIT_VAR_GLOBAL) {
        O<<"EDF";
        //std::cout<<"EDF ";
      }
      else {
        O<<"LDF";
        //std::cout<<"LDF ";
      }
    }
    else if (vp->eStatus == EXIT_PROG) {
      O<<"EP";
      //std::cout<<"EP ";
    }
    else if (vp->eStatus == EXIT_OUTP) {
      O<<"EO";
      //std::cout<<"EO "; 
    }
    ////added by Hui 01/08/16, to distinguish global vars from params/returns///////////
    else if (vp->eStatus == EXIT_VAR_GLOBAL) {
      O<<"EGV";
    }
    ///////////////////////////////////////////////////////////////////////////////////
    else if (vp->eStatus > EXIT_VAR_GLOBAL) {//'>=' --> '>'
      O<<"EV";
      //std::cout<<"EV ";
    }
    ///////added by Hui 01/26/16 Exit Var's aliases shall also treated as 'EV' (aliases of gv in the func)
    else if (vp->nStatus[EXIT_VAR] || vp->nStatus[EXIT_VAR_ALIAS]) {
      O<<"EV";
    }
    //second cond is added by Hui 01/26/16 local aliases of gv should be treated just as a 'EF'
    else if (vp->nStatus[EXIT_VAR_FIELD] || vp->nStatus[EXIT_VAR_FIELD_ALIAS]) {
      O<<"EF";
      //std::cout<<"EF ";
    }
    else if (vp->nStatus[LOCAL_VAR_FIELD]) {
      O<<"LF";
      //std::cout<<"LF ";
      
    }
    else {
      O<<"U";
      //std::cout<<"U ";
    }
    
    O<<vp->addedFromWhere<<" ";
    
    if (vp->nStatus[EXIT_VAR_FIELD] || vp->nStatus[LOCAL_VAR_FIELD] || vp->isDerived) {  
      O<<getFullStructName(vp)<<"   ";
      //O<<vp->name;
    }
    else {
      O<<vp->name<<"   ";
      //O<<vp->name;
    }
    
    // The generic type as given by LLVM (int, double, Struct*)
    O<<vp->genType<<" ";
    
    if (vp->sType != NULL) {
      O<<vp->sType->structName<<"   ";
    }
    else {
      O<<"NULL   ";
    }
    
    /*
     if (vp->fieldUpPtr != NULL)
     {
     O<<getFullStructName(vp->fieldUpPtr)<<" ";
     }
     else
     O<<"NULL ";
     */
    
    outputParamInfo(O,vp);
    //outputParamInfo(std::cout,vp);
    
    
    O<<std::endl;
    //std::cout<<std::endl;
    
  }//end of blamees
  
  
  for (set_vp_i = localBlamees.begin(); set_vp_i != localBlamees.end(); set_vp_i++)
  {
    VertexProps *vp = (*set_vp_i);
    
    if (vp->isDerived) 
    {
      O<<"VDF";
      //std::cout<<"VDF ";
    }
    if (vp->nStatus[LOCAL_VAR])
    {
      O<<"VL";
      //std::cout<<"VL ";
    }
    else if (vp->nStatus[LOCAL_VAR_FIELD])
    {
      O<<"VFL";
      //std::cout<<"VFL ";
    }
    else
    {
      O<<"VL";
      //std::cout<<"VL ";
    }
    
    O<<vp->addedFromWhere<<" ";
    
    if ( vp->nStatus[LOCAL_VAR_FIELD] )
    {  
      O<<getFullStructName(vp)<<" ";
      //O<<vp->name;
    }
    else
    {
      O<<vp->name<<" ";
      //O<<vp->name;
    }
    
    // The generic type as given by LLVM (int, double, Struct*)
    O<<vp->genType<<" ";
    
    if (vp->sType != NULL)
    {
      O<<vp->sType->structName<<" ";
    }
    else
    {
      O<<"NULL ";
    }
    
    /*
     if (vp->fieldUpPtr != NULL)
     {
     O<<getFullStructName(vp->fieldUpPtr)<<" ";
     }
     else
     O<<"NULL ";
     */
    
    //outputParamInfo(O,vp);
    //outputParamInfo(std::cout,vp);
    
    
    O<<std::endl;
    //std::cout<<std::endl;
    
  }//end of localBlamees
  
  
  if (blamees.size() == 0)
  {
    //std::cerr<<"No EV[EO, EP] found ["<<realName<<"]"<<std::endl;
    //std::cout<<"No EV[EO, EP] found ["<<realName<<"]"<<std::endl;
    O<<"***No EV[EO, EP] found*** ["<<realName<<"]"<<std::endl;
    
  }
  
}


/* SAVE TENO
//'vp' is the blamee from callee frame(oldBlamees), 'blamee' is the blamee for this frame(blamees)
//now we try to add blamee.field to blamees by looking at vp.field
void BlameFunction::addTempFieldBlameesRecursive(VertexProps *vp, VertexProps *blamee, 
        std::set<VertexProps *> &oldBlamees, std::set<VertexProps *> &blamees, std::set<VertexProps *> &visited)
{
  // The usual recursive check
  
  if (visited.count(vp))
    return;
  
  visited.insert(vp);
  
#ifdef DEBUG_ATFB
  std::cout<<"ATFB(2) "<<vp->name<<std::endl;
#endif

  std::set<VertexProps *>::iterator set_vp_i;
  for (set_vp_i = vp->fields.begin(); set_vp_i != vp->fields.end(); set_vp_i++) {
#ifdef DEBUG_ATFB
    std::cout<<"ATFB(3) "<<vp->name<<"'s field: "<<(*set_vp_i)->name<<std::endl;
#endif
    if (oldBlamees.count(*set_vp_i) > 0) {
      VertexProps *vp2 = *set_vp_i;
      bool found = true;
      //TOCHECK: why name of newVP == vp2 ?? the top-level struct parent name should
      //be "blamee" in blamees but not vp in oldBlamees, Change it!
      VertexProps *newVP = findOrCreateTempBlamees(blamees, getFullStructName(vp2), found);
      //findOrCreateTempBlamees will create/find blamee.field and insert into blamees 
      if (found == false) {
        //std::cout<<"Adding info for newly generated VP(2) "<<newVP->name<<std::endl;
        newVP->sType = vp2->sType;
        newVP->genType = vp2->genType;
        newVP->fieldUpPtr = blamee;
        newVP->sField = vp2->sField;
        newVP->calleePar = blamee->calleePar;
        blamee->tempFields.insert(newVP);
      }
      
      addTempFieldBlameesRecursive(vp2, newVP, oldBlamees, blamees, visited);
    }
  }
  
  for (set_vp_i = vp->tempFields.begin(); set_vp_i != vp->tempFields.end(); set_vp_i++) {
#ifdef DEBUG_ATFB
    std::cout<<"ATFB(4) "<<vp->name<<"'s tempfield: "<<(*set_vp_i)->name<<std::endl;
#endif
    
    if (oldBlamees.count(*set_vp_i) > 0) {
      VertexProps *vp2 = *set_vp_i;
      //std::cout<<getFullStructName(blamee)<<"("<<blamee<<")"<<" maybe connected to(3) "<<getFullStructName(vp2)<<" "<<vp2->name<<" "<<vp2<<std::endl;
      
      bool found = true;
      //TOCHECK: why name of newVP == vp2 ?? the top-level struct parent name should
      //be "blamee" in blamees but not vp in oldBlamees, Change it!
      VertexProps *newVP = findOrCreateTempBlamees(blamees, getFullStructName(vp2), found);
      
      if (found == false) {
        //std::cout<<"Adding info for newly generated VP(3) "<<newVP->name<<" "<<newVP<<std::endl;
        newVP->sType = vp2->sType;
        newVP->genType = vp2->genType;
        newVP->fieldUpPtr = blamee;
        newVP->sField = vp2->sField;
        newVP->calleePar = blamee->calleePar;
        blamee->tempFields.insert(newVP);
      }
      
      addTempFieldBlameesRecursive(vp2, newVP, oldBlamees, blamees, visited);
    }
  }
  
  // a is a temptempField of b if b=a->alias->fieldUpPtr or b=a->aliasUpPtr, came from addBlameToFieldParent
  for (set_vp_i = vp->temptempFields.begin(); set_vp_i != vp->temptempFields.end(); set_vp_i++) {
#ifdef DEBUG_ATFB
    std::cout<<"ATFB(5) "<<vp->name<<"'s temptempfield: "<<(*set_vp_i)->name<<std::endl;
#endif
    
    if (oldBlamees.count(*set_vp_i) > 0) {
      VertexProps *vp2 = *set_vp_i;
      bool found = true;
      //TOCHECK: why name of newVP == vp2 ?? the top-level struct parent name should
      //be "blamee" in blamees but not vp in oldBlamees, Change it!
      VertexProps *newVP = findOrCreateTempBlamees(blamees, getFullStructName(vp2), found);
      
      if (found == false) {
        //std::cout<<"Adding info for newly generated VP(3) "<<newVP->name<<" "<<newVP<<std::endl;
        newVP->sType = vp2->sType;
        newVP->genType = vp2->genType;
        newVP->fieldUpPtr = blamee;
        newVP->sField = vp2->sField;
        newVP->calleePar = blamee->calleePar;
        blamee->tempFields.insert(newVP);
      }
      
      addTempFieldBlameesRecursive(vp2, newVP, oldBlamees, blamees, visited);
    }
  }
 
}
*/



//'vp' is the blamee from callee frame(oldBlamees), 'blamee' is the blamee for this frame(blamees)
//now we try to add blamee.field to blamees by looking at vp.field
void BlameFunction::addTempFieldBlameesRecursive(VertexProps *vp, VertexProps *blamee, 
        std::set<VertexProps *> &oldBlamees, std::set<VertexProps *> &blamees, std::set<VertexProps *> &visited)
{
  // The usual recursive check
  
  if (visited.count(vp))
    return;
  
  visited.insert(vp);
  
#ifdef DEBUG_ATFB
  std::cout<<"ATFB(2) "<<vp->name<<std::endl;
#endif

  std::set<VertexProps *>::iterator set_vp_i;
  for (set_vp_i = vp->fields.begin(); set_vp_i != vp->fields.end(); set_vp_i++) {
#ifdef DEBUG_ATFB
    std::cout<<"ATFB(3) "<<vp->name<<"'s field: "<<(*set_vp_i)->name<<std::endl;
#endif
    if (oldBlamees.count(*set_vp_i) > 0) {
      VertexProps *vp2 = *set_vp_i;
      if (vp2->sField) {
        //newly added 03/31/17: match fields from caller to blamed fields in callee
        std::set<VertexProps *>::iterator svi;
        for (svi=blamee->fields.begin(); svi!=blamee->fields.end(); svi++) {
          VertexProps *vp3 = *svi;
          if (vp3->sField) {
            if (vp2->sField->fieldNum == vp3->sField->fieldNum) {
              blamees.insert(vp3);
              vp3->addedFromWhere = 80; //new tag 
              //recursively check vp2<->vp3
              addTempFieldBlameesRecursive(vp2, vp3, oldBlamees, blamees, visited);
            }
          }
        }   
        //---------------------^^^^^--------------------------------------------//
        //We still keep newVP in case vp3 doesn't exsits 03/31/17
        bool found = true;
        //for vp2 = a_addr.f1, newVP->name will also be a_addr.f1 but added to blamees;
        //however, when newVP is finally output, getFullStructName(newVP) will produce
        //"a.f1" because of the fields relation added for newVP with blamee (a)
        //Although, a.f1 should already be there from the above vp3 if sField  exists
        //We put this under "vp2->sField" because if not, it'll be a.0x666* but 0x666* 
        //should not be in blamees since it's a reg in oldBlamees
        VertexProps *newVP = findOrCreateTempBlamees(blamees, getFullStructName(vp2), found);
        if (found == false) {
#ifdef DEBUG_ATFB
          std::cout<<"Adding info for newly generated VP(2) "<<newVP->name<<std::endl;
#endif
          newVP->sType = vp2->sType;
          newVP->genType = vp2->genType;
          newVP->fieldUpPtr = blamee;
          newVP->sField = vp2->sField;
          newVP->calleePar = blamee->calleePar;
          blamee->tempFields.insert(newVP);
        }
    
        addTempFieldBlameesRecursive(vp2, newVP, oldBlamees, blamees, visited);
      } //if vp2->sField != NULL
      else {
#ifdef DEBUG_ATFB
        std::cout<<"vp2 from oldBlamee's fields has no sField, we ignore "<<vp2->name<<std::endl;
#endif
      }
    }
  }
  
  for (set_vp_i = vp->tempFields.begin(); set_vp_i != vp->tempFields.end(); set_vp_i++) {
#ifdef DEBUG_ATFB
    std::cout<<"ATFB(4) "<<vp->name<<"'s tempfield: "<<(*set_vp_i)->name<<std::endl;
#endif
    
    if (oldBlamees.count(*set_vp_i) > 0) {
      VertexProps *vp2 = *set_vp_i;
      if (vp2->sField) {
      
        bool found = true;
        //TOCHECK: why name of newVP == vp2 ?? the top-level struct parent name should
        //be "blamee" in blamees but not vp in oldBlamees, Change it!
        VertexProps *newVP = findOrCreateTempBlamees(blamees, getFullStructName(vp2), found);
      
        if (found == false) {
          //std::cout<<"Adding info for newly generated VP(3) "<<newVP->name<<" "<<newVP<<std::endl;
          newVP->sType = vp2->sType;
          newVP->genType = vp2->genType;
          newVP->fieldUpPtr = blamee;
          newVP->sField = vp2->sField;
          newVP->calleePar = blamee->calleePar;
          blamee->tempFields.insert(newVP);
        }
      
        addTempFieldBlameesRecursive(vp2, newVP, oldBlamees, blamees, visited);
      }
    }
  }
  
  // a is a temptempField of b if b=a->alias->fieldUpPtr or b=a->aliasUpPtr, came from addBlameToFieldParent
  for (set_vp_i = vp->temptempFields.begin(); set_vp_i != vp->temptempFields.end(); set_vp_i++) {
#ifdef DEBUG_ATFB
    std::cout<<"ATFB(5) "<<vp->name<<"'s temptempfield: "<<(*set_vp_i)->name<<std::endl;
#endif
    
    if (oldBlamees.count(*set_vp_i) > 0) {
      VertexProps *vp2 = *set_vp_i;
      if (vp2->sField) {
        bool found = true;
        //TOCHECK: why name of newVP == vp2 ?? the top-level struct parent name should
        //be "blamee" in blamees but not vp in oldBlamees, Change it!
        VertexProps *newVP = findOrCreateTempBlamees(blamees, getFullStructName(vp2), found);
      
        if (found == false) {
          //std::cout<<"Adding info for newly generated VP(3) "<<newVP->name<<" "<<newVP<<std::endl;
          newVP->sType = vp2->sType;
          newVP->genType = vp2->genType;
          newVP->fieldUpPtr = blamee;
          newVP->sField = vp2->sField;
          newVP->calleePar = blamee->calleePar;
          blamee->tempFields.insert(newVP);
        }
      
        addTempFieldBlameesRecursive(vp2, newVP, oldBlamees, blamees, visited);
      }
    }
  }
}


void BlameFunction::addTempFieldBlamees(std::set<VertexProps *> &blamees, std::set<VertexProps *> &oldBlamees)
{
  std::set<VertexProps *>::iterator set_vp_i, set_vp_i2;
  
#ifdef ADD_TEMP_FIELDBLAMEES
  std::cout<<"Entering addTempFieldBlamees for "<<this->realName<<endl;
#endif
  for (set_vp_i = blamees.begin(); set_vp_i != blamees.end(); set_vp_i++) {
    VertexProps *vp = (*set_vp_i);
    // This blamee is a struct and has some fields we can attach to it from prior frames
    if (vp->genType.find("Struct") != std::string::npos) {
      for (set_vp_i2 = oldBlamees.begin(); set_vp_i2 != oldBlamees.end(); set_vp_i2++) {
        VertexProps *vp2 = (*set_vp_i2);
        // We're looking at the root in the previous frame that matches up
        if (vp->callerPars.count(vp2->calleePar) && vp2->fieldUpPtr == NULL) {
          
        //The follow two 'continue' conditions only apply for "Struct", check why? 12/19/16
          //std::cout<<vp->sType<<" "<<vp2->sType<<std::endl;
          if (vp->sType == NULL || vp2->sType == NULL)
            continue;
          
          //std::cout<<vp->sType->structName<<" "<<vp2->sType->structName<<std::endl;
          if (vp->sType->structName != vp2->sType->structName)
            continue;

          std::set<VertexProps *> visited;
          addTempFieldBlameesRecursive(vp2, vp, oldBlamees, blamees, visited);
        }
      }
    } //end of vp->genType.find(Struct)

    //pid array (chpl__iterLF) has no sType but we still need to handle it
    else if (vp->genType.find("Array") != std::string::npos) {
      for (set_vp_i2 = oldBlamees.begin(); set_vp_i2 != oldBlamees.end(); set_vp_i2++) {
        VertexProps *vp2 = (*set_vp_i2);
        // We're looking at the root in the previous frame that matches up
        if (vp->callerPars.count(vp2->calleePar)  && vp2->fieldUpPtr == NULL) {
          
          if (vp->sType == NULL || vp2->sType == NULL)
            continue;
          
          if (vp->sType->structName != vp2->sType->structName)
            continue;
          
          if (vp->sType->structName.find("PidArray") == std::string::npos) //Only pidArray takes this special process
            continue;
          
          std::set<VertexProps *> visited;
          addTempFieldBlameesRecursive(vp2, vp, oldBlamees, blamees, visited);
        }
      }
    }

    else if (vp->genType.find("VOID") != std::string::npos) {
      for (set_vp_i2 = oldBlamees.begin(); set_vp_i2 != oldBlamees.end(); set_vp_i2++) {
        VertexProps * vp2 = (*set_vp_i2);
        // We're looking at the root in the previous frame that matches up
        if (vp->callerPars.count(vp2->calleePar)  && vp2->fieldUpPtr == NULL) {
          
          std::set<VertexProps *> visited;
          addTempFieldBlameesRecursive(vp2, vp, oldBlamees, blamees, visited);
        }
      }    
    }

    else if (vp->genType.find("Opaque") != std::string::npos) {
      for (set_vp_i2 = oldBlamees.begin(); set_vp_i2 != oldBlamees.end(); set_vp_i2++) {
        VertexProps * vp2 = (*set_vp_i2);
        // We're looking at the root in the previous frame that matches up
        if (vp->callerPars.count(vp2->calleePar)  && vp2->fieldUpPtr == NULL) {
          
          std::set<VertexProps *> visited;
          addTempFieldBlameesRecursive(vp2, vp, oldBlamees, blamees, visited);
          
        }
      }    
    }

    else if (vp->genType.find("Int") != std::string::npos) {
      for (set_vp_i2 = oldBlamees.begin(); set_vp_i2 != oldBlamees.end(); set_vp_i2++) {
        VertexProps *vp2 = (*set_vp_i2);
        // We're looking at the root in the previous frame that matches up
        if (vp->callerPars.count(vp2->calleePar)  && vp2->fieldUpPtr == NULL) {
          
          std::set<VertexProps *> visited;
          addTempFieldBlameesRecursive(vp2, vp, oldBlamees, blamees, visited);
          
        }
      }    
    }

    else {
      for (set_vp_i2 = oldBlamees.begin(); set_vp_i2 != oldBlamees.end(); set_vp_i2++) {
        VertexProps *vp2 = (*set_vp_i2);
        // We're looking at the root in the previous frame that matches up
        if (vp->callerPars.count(vp2->calleePar)  && vp2->fieldUpPtr == NULL) {
          
          if (vp2->sType == NULL)
            continue;
          
          std::set<VertexProps *> visited;
          addTempFieldBlameesRecursive(vp2, vp, oldBlamees, blamees, visited);
        }
      }
    }
  }
}


// Add all pidAliases of the one in blamees to blamees 03/31/17
void BlameFunction::addPidAliasesToBlamees(std::set<VertexProps *> &blamees, std::set<VertexProps *> &localBlamees)
{
  std::set<VertexProps *>::iterator svi, sve, svi2, sve2;
  std::set<VertexProps *> tempBlamees, tempLocalBlamees;
  for (svi=blamees.begin(), sve=blamees.end(); svi != sve; svi++) {
    VertexProps *vp = *svi;
    if (vp->isPid) {
      for (svi2=vp->pidAliases.begin(), sve2=vp->pidAliases.end(); svi2 != sve2; svi2++) {
        VertexProps *vp2 = *svi2;
        // we shouldn't change those that already existed in blamees
        if (vp2->eStatus>=EXIT_PROG || vp2->nStatus[EXIT_VAR] || vp2->nStatus[EXIT_VAR_ALIAS]
            || vp2->nStatus[EXIT_VAR_FIELD] || vp2->nStatus[EXIT_VAR_FIELD_ALIAS]) {
          if (localBlamees.count(vp2) == 0 && blamees.count(vp2) == 0) { //only add if it wasn't in both places
            tempBlamees.insert(vp2);
            vp2->addedFromWhere = 88; //new tag 07/18/17
          }
        }
        else { //LV
          if (localBlamees.count(vp2) == 0 && blamees.count(vp2) == 0) { //only add if it wasn't in both places
            tempLocalBlamees.insert(vp2);
            vp2->addedFromWhere = 98; //new tag 07/18/17
          }
        }
      }
    }
  }

  for (svi=localBlamees.begin(), sve=localBlamees.end(); svi != sve; svi++) {
    VertexProps *vp = *svi;
    if (vp->isPid) {
      for (svi2=vp->pidAliases.begin(), sve2=vp->pidAliases.end(); svi2 != sve2; svi2++) {
        VertexProps *vp2 = *svi2;
        // we shouldn't change those that already existed in blamees
        if (vp2->eStatus>=EXIT_PROG || vp2->nStatus[EXIT_VAR] || vp2->nStatus[EXIT_VAR_ALIAS]
            || vp2->nStatus[EXIT_VAR_FIELD] || vp2->nStatus[EXIT_VAR_FIELD_ALIAS]) {
          if (localBlamees.count(vp2) == 0 && blamees.count(vp2) == 0) { //only add if it wasn't in both places
            tempBlamees.insert(vp2);
            vp2->addedFromWhere = 62; //new tag 07/18/17
          }
        }
        else { //LV
          if (localBlamees.count(vp2) == 0 && blamees.count(vp2) == 0) { //only add if it wasn't in both places
            tempLocalBlamees.insert(vp2);
            vp2->addedFromWhere = 66; //new tag 07/18/17
          }
        }
      }
    }
  }
  //add all new nodes from tempBlamees to blamees
  blamees.insert(tempBlamees.begin(), tempBlamees.end());
  //add all new nodes from tempLocalBlamees to localBlamees
  localBlamees.insert(tempLocalBlamees.begin(), tempLocalBlamees.end());
}


std::string BlameFunction::getFullContextName(vector<StackFrame> & frames, ModuleHash & modules,
                                              vector<StackFrame>::iterator vec_SF_i)
{
  int x = 0;
  std::string fullName = getName();
    
  vector<StackFrame>::iterator plusOne = vec_SF_i + 1;
  for (;plusOne != frames.end(); plusOne++)
  {    
    ++x;
    if (modules.count((*plusOne).moduleName))
    {
      BlameModule * bmCheck = modules[(*plusOne).moduleName];

      if (bmCheck == NULL)
        return fullName;
      //BlameFunction * bfCheck = bmCheck->findLineRange((*plusOne).lineNumber);
      BlameFunction *bfCheck = bmCheck->getFunction((*plusOne).frameName);
      if (bfCheck == NULL)
        return fullName;
      fullName.insert(0, ".");
      fullName.insert(0, bfCheck->getName());
      
    }
    else
    {
//      fprintf(stderr, "%s %d\n", fullName.c_str(), x);
      return fullName;
    }
  }
  
//  fprintf(stderr, "%s %d\n", fullName.c_str(), x);
  return fullName;
  
}

void BlameFunction::resolveLineNum(vector<StackFrame> & frames, ModuleHash & modules,vector<StackFrame>::iterator vec_SF_i, std::set<VertexProps *> & blamees, short isBlamePoint, bool isBottomParsed, BlameFunction *oldFunc, std::ostream &O)
{
  clearPastData();
  
  std::set<VertexProps *> localBlamees;
  std::set<VertexProps *> DQblamees;
  
  O<<"FRAME# "<<(*vec_SF_i).frameNumber<<" ";
  O<<getFullContextName(frames, modules, vec_SF_i); //getFullContextName will call findLineRange
  O<<" "<<getModuleName()<<" "<<getModulePathName()<<" ";
  O<<(*vec_SF_i).lineNumber<<" "<<isBlamePoint<<" ";
  O<<beginLineNum<<" "<<endLineNum<<std::endl;
  
  // Dominator call information for aliases
  populateTempSideEffects((*vec_SF_i).lineNumber, blamees);
  
  // We're not a blame point and we're in the middle of the stack,
  //   we're going to have to apply a transfer function to get out of this
  if (isBlamePoint == NO_BLAME && isBottomParsed == false) {  
#ifdef DEBUG_RESOLVE_LN
   cout<<"I'm IN isBlamePoint == NO_BLAME && isBottomParsed == false"<<endl;
#endif
    std::vector<VertexProps *>::iterator vec_vp_i;
    
    VertexProps *callNode = NULL;
    
    std::vector<VertexProps *> matchingCalls;
    // We're only concerned with transfer functions so we look through the call nodes
    for (vec_vp_i = callNodes.begin(); vec_vp_i != callNodes.end(); vec_vp_i++) {
      VertexProps *vp = *vec_vp_i;
      if (vp->declaredLine == (*vec_SF_i).lineNumber) {
        matchingCalls.push_back(vp);
      }
    }

    if (matchingCalls.size() == 0) {
#ifdef DEBUG_RESOLVE_LN
      std::cerr<<"TRANSFER FUNC BF.cpp:3365 - No lineNums found for sample in "<<getName();
      std::cerr<<" Line Num "<<(*vec_SF_i).lineNumber<<" Frame Number  "<<(*vec_SF_i).frameNumber<<std::endl;
#endif
    }
    else if (matchingCalls.size() == 1) {
      // apply transfer function
      callNode = matchingCalls.front();
      handleTransferFunction(callNode, blamees); 
      std::set<VertexProps *>  oldBlamees = blamees;
      
      blamees.clear();
      determineBlameHolders(blamees, oldBlamees, callNode, (*vec_SF_i).lineNumber, isBlamePoint, localBlamees, DQblamees);
      calcParamInfo(blamees, callNode);
      
      // Go through and add temporary fields to the blamees from prior frames
      // Example
      // Frame #0
      // EF s1_addr.b
      // EV s1_addr
      // Frame #1
      // U  s1
      //  ... would be come
      // Frame #1
      //  U s1
      //  GF s1.b
      addTempFieldBlamees(blamees, oldBlamees);
      addPidAliasesToBlamees(blamees, localBlamees); //added 03/31/17 for blamed pidAliases
      clearTempFields(oldBlamees, oldFunc);
      
      outputFrameBlamees(blamees, localBlamees, DQblamees, O);      
      vec_SF_i++; 
      if (vec_SF_i == frames.end())
        return;
      // NOW check the upper/caller frame
      if ((*vec_SF_i).lineNumber > 0 && (*vec_SF_i).toRemove == false) {
#ifdef DEBUG_RESOLVE_LN
        cout<<"*****At Frame(3) "<<(*vec_SF_i).frameNumber<<" at line num "<<(*vec_SF_i).lineNumber;
        cout<<" in module "<<(*vec_SF_i).moduleName<<endl;
#endif
        // Get the module from the debugging information
        BlameModule *bm = modules[(*vec_SF_i).moduleName];
        if (bm != NULL) {
          //BlameFunction *nextbf = bm->findLineRange((*vec_SF_i).lineNumber);
          BlameFunction *nextbf = bm->getFunction((*vec_SF_i).frameName);
          //Two consecutive stackFrames cannot mapped to same func
          if (nextbf&&((this->getName()).compare(nextbf->getName())!=0)) {
#ifdef DEBUG_RESOLVE_LN
            cout<<"start recursion in matchingCalls==1\n";
#endif
            nextbf->resolveLineNum(frames, modules, vec_SF_i, blamees, 
                               nextbf->getBlamePoint(), false, this, O);
          }
          else {
#ifdef DEBUG_RESOLVE_LN
            std::cerr<<"Error: nextbf=NULL or upper frame has saame name"<<std::endl;
#endif
          }
        }
      }
    }
    else { // More than one matching call (two func calls on one line number in code) 
      callNode = NULL;
      // figure out which call is appropriate, then apply transfer function
      vector<StackFrame>::iterator minusOne = vec_SF_i - 1;
      BlameModule * bmCheck = modules[(*minusOne).moduleName];
      
      if (bmCheck == NULL) {
        std::cerr<<"BM null when differntiating calls"<<std::endl;
        return;
      }
      
      //BlameFunction * bfCheck = bmCheck->findLineRange((*minusOne).lineNumber);
      BlameFunction *bfCheck = bmCheck->getFunction((*minusOne).frameName);
      if (bfCheck == NULL) {
        std::cerr<<"BF null when differentiating calls"<<std::endl;
        return;
      }
      
      std::vector<VertexProps *>::iterator vec_vp_i2;
      for (vec_vp_i2 = matchingCalls.begin(); vec_vp_i2 != matchingCalls.end(); vec_vp_i2++) {
        VertexProps *vpCheck = *vec_vp_i2;
        // Look for subsets since vpCheck will have the line number concatenated
        if (vpCheck->name.find(bfCheck->getName()) != std::string::npos) {
          callNode = vpCheck;
          break;
        }
      }
      
      if (callNode == NULL) {
        for (vec_vp_i2 = matchingCalls.begin(); vec_vp_i2 != matchingCalls.end(); vec_vp_i2++) {
          VertexProps *vpCheck = *vec_vp_i2;
          //TODO: funcPtrs shouldn't be handled in this way 12/05/16
          // Look for function pointers resolved at runtime //TC: why tmp? Hui
          if (vpCheck->name.find("tmp") != std::string::npos) {
            callNode = vpCheck;
            break;
          }
        }
      }
      
      if (callNode == NULL) {
        std::cerr<<"No matching call nodes from multiple matches"<<std::endl;
        return;
      }
      else
        std::cout<<"Call node that matched is "<<callNode->name<<std::endl;

      handleTransferFunction(callNode, blamees); 
      std::set<VertexProps *>  oldBlamees = blamees;
      blamees.clear();
      determineBlameHolders(blamees, oldBlamees, callNode, (*vec_SF_i).lineNumber, isBlamePoint, localBlamees, DQblamees);
      calcParamInfo(blamees, callNode);
      
      
      // Go through and add temporary fields to the blamees from prior frames
      // Example
      // Frame #0
      // EF s1_addr.b
      // EV s1_addr
      // Frame #1
      // U  s1
      //  ... would be come
      // Frame #1
      //  U s1
      //  GF s1.b
      addTempFieldBlamees(blamees, oldBlamees);
      addPidAliasesToBlamees(blamees, localBlamees); //added 03/31/17
      clearTempFields(oldBlamees, oldFunc);
      
      outputFrameBlamees(blamees, localBlamees, DQblamees, O);      
      vec_SF_i++;
      if (vec_SF_i == frames.end())
        return;
      
      if ((*vec_SF_i).lineNumber>0 && (*vec_SF_i).toRemove==false) {
#ifdef DEBUG_RESOLVE_LN
        cout<<"*****At Frame(4) "<<(*vec_SF_i).frameNumber<<" at line num "<<(*vec_SF_i).lineNumber;
        cout<<" in module "<<(*vec_SF_i).moduleName<<endl;
#endif
        // Get the module from the debugging information
        BlameModule *bm = modules[(*vec_SF_i).moduleName];
        
        if (bm != NULL) {
          //BlameFunction *nextbf = bm->findLineRange((*vec_SF_i).lineNumber);
          BlameFunction *nextbf = bm->getFunction((*vec_SF_i).frameName);
          
          //Changed by Hui, two consecutive stackFrames cannot mapped to same func
          if (nextbf&&((this->getName()).compare(nextbf->getName())!=0)) {
            //cout<<"Checking(4) BF line range "<<bf->getName()<<std::endl;
#ifdef DEBUG_RESOLVE_LN
            cout<<"start recursion when more than one matching call found\n";
#endif
            nextbf->resolveLineNum(frames, modules, vec_SF_i, blamees, 
                               nextbf->getBlamePoint(), false, this, O);
          }
        }
        else {
#ifdef DEBUG_RESOLVE_LN
          std::cerr<<"BM(4) is NULL"<<std::endl;
#endif
        }
      }
    }
  } //End of isBottomParsed=false and isBlamePoint=false

  // We don't need to apply a transfer function as it's the bottom of the readable stack
  else if (isBottomParsed == true) {
#ifdef DEBUG_RESOLVE_LN
    cout<<"I'm IN isBottomParsed==true"<<endl;
#endif
    VertexProps * callNode = NULL;
    std::set<VertexProps *> oldBlamees = blamees;
    determineBlameHolders(blamees, oldBlamees, callNode,(*vec_SF_i).lineNumber, isBlamePoint, localBlamees, DQblamees);
    calcParamInfo(blamees, callNode);
    addPidAliasesToBlamees(blamees, localBlamees); //added 03/31/17
    
    outputFrameBlamees(blamees, localBlamees, DQblamees, O);

    vec_SF_i++; //check the next stack frame in this instance
    if (vec_SF_i == frames.end())
      return;

    if ((*vec_SF_i).lineNumber>0 && (*vec_SF_i).toRemove==false) {
      BlameModule * bm = modules[(*vec_SF_i).moduleName];
      
      if (bm != NULL) {
        //BlameFunction *nextbf = bm->findLineRange((*vec_SF_i).lineNumber);
        BlameFunction *nextbf = bm->getFunction((*vec_SF_i).frameName);
        //Changed by Hui, two consecutive stackFrames cannot mapped to same func
        if (nextbf&&((this->getName()).compare(nextbf->getName())!=0)) {
#ifdef DEBUG_RESOLVE_LN
            cout<<"Gonna start recursion when isBottomParsed=false"<<endl;
#endif
          nextbf->resolveLineNum(frames, modules, vec_SF_i, blamees, nextbf->getBlamePoint(), false, this, O);
        }
      }
    }    
  } // End of isBottomParsed=true

  else if (isBlamePoint > NO_BLAME) {//NO_BLAME=0
#ifdef DEBUG_RESOLVE_LN
    cout<<"I'm IN (isBlamePoint > NO_BLAME): isBlamePoint="<<isBlamePoint<<endl;
#endif
    std::vector<VertexProps *>::iterator vec_vp_i;
    VertexProps *callNode = NULL;
    
    std::vector<VertexProps *> matchingCalls;
    //We're only concerned with transfer functions so we look through the call nodes
    for (vec_vp_i = callNodes.begin(); vec_vp_i != callNodes.end(); vec_vp_i++) {
      VertexProps *vp = *vec_vp_i;
#ifdef DEBUG_RESOLVE_LN
      std::cout<<"bf has callNode: "<<vp->name<<" in "<<getName()<<\
          " vp's declaredLine = "<<vp->declaredLine<<std::endl;
#endif
      if (vp->lineNumbers.count((*vec_SF_i).lineNumber)) {
#ifdef DEBUG_RESOLVE_LN
        std::cout<<"TRANSFER FUNC -- Line Number found in VP "<<vp->name<<std::endl;
#endif
        matchingCalls.push_back(vp);
      }
      else if (vp->declaredLine == (*vec_SF_i).lineNumber) {
#ifdef DEBUG_RESOLVE_LN
        std::cout<<"TRANSFER FUNC -- Line number(2) found in VP "<<vp->name<<std::endl;
#endif
        matchingCalls.push_back(vp);
      }
    }
    if (matchingCalls.size() == 0) {
#ifdef DEBUG_RESOLVE_LN
      std::cerr<<"TRANSFER FUNC BF.cpp:3632 - No lineNums found for sample in "<<getName();
      std::cerr<<" Line Num "<<(*vec_SF_i).lineNumber<<" Frame Number  "<<(*vec_SF_i).frameNumber<<std::endl;
#endif
    }
    else if (matchingCalls.size() == 1) {
      // apply transfer function
      callNode = matchingCalls.front();
      handleTransferFunction(callNode, blamees); 
      // In previous senarios, we need to output blamees 
      //and prepare for the next frame, now no need since it's blame point
    }

    else { // More than one matching call (two func calls on one line number in code)
      callNode = NULL;
      // figure out which call is appropriate, then apply transfer function
      vector<StackFrame>::iterator minusOne = vec_SF_i - 1;
      BlameModule * bmCheck = modules[(*minusOne).moduleName];
      if (bmCheck == NULL) {
        std::cerr<<"BM null when differntiating calls"<<std::endl;
        return;
      }
      
      //BlameFunction * bfCheck = bmCheck->findLineRange((*minusOne).lineNumber);
      BlameFunction *bfCheck = bmCheck->getFunction((*minusOne).frameName);
      if (bfCheck == NULL) {
        std::cerr<<"BF null when differentiating calls"<<std::endl;
        return;
      }
      
      std::vector<VertexProps *>::iterator vec_vp_i2;
      for (vec_vp_i2 = matchingCalls.begin(); vec_vp_i2 != matchingCalls.end(); vec_vp_i2++) {
        VertexProps * vpCheck = *vec_vp_i2;
        // Look for subsets since vpCheck will have the line number concatenated
        if (vpCheck->name.find(bfCheck->getName()) != std::string::npos)
          callNode = vpCheck;
      }
      
      if (callNode == NULL) {
        for (vec_vp_i2 = matchingCalls.begin(); vec_vp_i2 != matchingCalls.end(); vec_vp_i2++) {
          VertexProps * vpCheck = *vec_vp_i2;
          // Look for function pointers resolved at runtime
          if (vpCheck->name.find("tmp") != std::string::npos)
            callNode = vpCheck;
        }
      }      
      
      if (callNode == NULL) {
        std::cerr<<"No matching call nodes from multiple matches"<<std::endl;
        return;
      }
      
      //std::cout<<"Call node that matched is "<<callNode->name<<std::endl;
      handleTransferFunction(callNode, blamees); 
    }
    
    std::set<VertexProps *>  oldBlamees = blamees;
    blamees.clear();
    determineBlameHolders(blamees, oldBlamees, callNode,(*vec_SF_i).lineNumber, isBlamePoint, localBlamees, DQblamees);
    calcParamInfo(blamees, callNode);
    
    // Go through and add temporary fields to the blamees from prior frames
    // Example
    // Frame #0
    // EF s1_addr.b
    // EV s1_addr
    // Frame #1
    // U  s1
    //  ... would become
    // Frame #1
    //  U s1
    //  GF s1.b
    addTempFieldBlamees(blamees, oldBlamees);
    addPidAliasesToBlamees(blamees, localBlamees); //added 03/31/17
    clearTempFields(oldBlamees, oldFunc);
    outputFrameBlamees(blamees, localBlamees, DQblamees, O);    
    
    vec_SF_i++;
    if (vec_SF_i == frames.end())
      return;
    
    // We still need to check the upper frame if it's a implicit blamepoint, why?
    if ((*vec_SF_i).lineNumber > 0 && (*vec_SF_i).toRemove == false) {//test the next stack frame 
#ifdef DEBUG_RESOLVE_LN
      cout<<"*****At Frame(2) "<<(*vec_SF_i).frameNumber<<" at line num "<<(*vec_SF_i).lineNumber;
      cout<<" in module "<<(*vec_SF_i).moduleName<<endl;
#endif
      BlameModule * bm = modules[(*vec_SF_i).moduleName];
      
      if (bm != NULL) {
#ifdef DEBUG_RESOLVE_LN
        cout<<"bm != NULL in :3748!"<<endl;
#endif
        //BlameFunction *nextbf = bm->findLineRange((*vec_SF_i).lineNumber);
        BlameFunction *nextbf = bm->getFunction((*vec_SF_i).frameName);
        //Changed by Hui, two consecutive stackFrames cannot mapped to same func
        if (nextbf&&((this->getName()).compare(nextbf->getName())!=0)) {
#ifdef DEBUG_RESOLVE_LN
          cout<<"start recursion if nextbf.isBlamePoint==IMPLICIT\n";
#endif
          if (isBlamePoint == IMPLICIT) {
            nextbf->resolveLineNum(frames, modules, vec_SF_i, blamees, \
                               nextbf->getBlamePoint(), false, this, O);
          }
        }
      }
    }
  } //End of isBlamePoint > 0
  
  // TODO:: Cases where blame point can pass up params
  // TODO:: Automatic detection of V param, V return (explicit blame points)
}


// Utility Function: tell whether the string is end with dot&numbers(".XX")
bool BlameFunction::endWithNumber(const string &str)
{
  int pos = str.find_last_of('.');
  if (pos == string::npos)
    return false;
  else {
    pos++;
    string subStr = str.substr(pos);
    bool check = !subStr.empty() && subStr.find_first_not_of("0123456789") == std::string::npos;
    return check;
  }
}

// Utility Function: tell whether any substr of this string is number
bool BlameFunction::anySubstrIsNumber(const string &str)
{
  int pos = str.find_last_of('.');
  if (pos == string::npos)
    return false;
    
  string leftStr = string(str); //left part of '.' of the updated string
  bool check = false; //keep the return value
  while (pos != string::npos && !check) {
    pos++;
    string rightStr = leftStr.substr(pos); //substr start from pos, to the end of leftStr
    check |= !rightStr.empty() && rightStr.find_first_not_of("0123456789") == std::string::npos;
    // update str and pos for the next iteration
    leftStr = leftStr.substr(0, pos-1); //substr(start position, length of substr)
    pos = leftStr.find_last_of('.');
  }

  return check;
}
