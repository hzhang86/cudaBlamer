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

//#include "FunctionBFC.h"
#include "ModuleBFC.h"

#include <iostream>
#include <fstream>

using namespace std;

// to be used in pidArrayResolve
extern ofstream struct_file;

/*
string getStringFromMD(Value * v)
{
	string fail("---");
	
	if (v == NULL)
		return fail;
	
	if (isa<ConstantExpr>(v))
	{
		ConstantExpr * ce = cast<ConstantExpr>(v);
				
		User::op_iterator op_i = ce->op_begin();
		for (; op_i != ce->op_end(); op_i++)
		{
			Value * stringArr = *op_i;			
						
			if (isa<GlobalValue>(stringArr))
	    {
	      GlobalValue * gv = cast<GlobalValue>(stringArr);		
	      User::op_iterator op_i2 = gv->op_begin();
	      for (; op_i2 != gv->op_end(); op_i2++)
				{
					Value * stringArr2 = *op_i2;
					
					if (isa<ConstantArray>(stringArr2))
					{
						ConstantArray * ca = cast<ConstantArray>(stringArr2);
						if (ca->isString())
						{
							string rawRet = ca->getAsString();
							
							// Need to get rid of the trailing NULL
							return rawRet.substr(0, rawRet.length()-1);
						}
					}		
				}
	    }
		}
	}	
	return fail;
}


void getTypeFromBasic(Value * v, string & typeName)
{
	
	if (v->getName().find("derivedtype.type") != string::npos)
	{
		//cout<<"Leaving getTypeFromDerivedType (1)"<<endl;
		return;
	}
	
	if (!isa<GlobalVariable>(v))
	{
		//cout<<"Leaving getTypeFromDerivedType (2)"<<endl;
		return;
	}
	
	GlobalVariable * gv = cast<GlobalVariable>(v);
	
	User::op_iterator op_i2 = gv->op_begin();
	for (; op_i2 != gv->op_end(); op_i2++)
	{
		Value * structOp = *op_i2;			
		//cout<<"StructOp Name(getTypeFromDerivedType) - "<<structOp->getName()<<endl;
		
		if (isa<ConstantStruct>(structOp))
		{
			ConstantStruct * csOp = cast<ConstantStruct>(structOp);
			
			if (csOp->getNumOperands() < 9)
			{
				//cout<<"Leaving getTypeFromDerivedType (3)  "<<csOp->getNumOperands()<<endl;
				return;
			}
			
			
			string maybeName = getStringFromMD(csOp->getOperand(2));
			//cout<<"Maybe name (Basic) is "<<maybeName<<endl;
			typeName += maybeName;
			
		}
	}
	
	return;
}



void getTypeFromComposite(Value * v, string & typeName)
{
	
	return;
}


void getTypeFromDerived(Value* v, string & typeName)
{
	if (v->getName().find("derivedtype.type") != string::npos)
	{
		//cout<<"Leaving getTypeFromDerivedType (1)"<<endl;
		return;
	}
	
	if (!isa<GlobalVariable>(v))
	{
		//cout<<"Leaving getTypeFromDerivedType (2)"<<endl;
		return;
	}
	
	GlobalVariable * gv = cast<GlobalVariable>(v);
	
	User::op_iterator op_i2 = gv->op_begin();
	for (; op_i2 != gv->op_end(); op_i2++)
	{
		Value * structOp = *op_i2;			
		//cout<<"StructOp Name(getTypeFromDerivedType) - "<<structOp->getName()<<endl;
		
		if (isa<ConstantStruct>(structOp))
		{
			ConstantStruct * csOp = cast<ConstantStruct>(structOp);
			
			if (csOp->getNumOperands() < 9)
			{
				//cout<<"Leaving getTypeFromDerivedType (3)  "<<csOp->getNumOperands()<<endl;
				return;
			}
			
			
			string maybeName = getStringFromMD(csOp->getOperand(2));
			//cout<<"Maybe name(Derived) is "<<maybeName<<endl;
			
			
			// We found a legit type name
			if (maybeName.find("---") == string::npos)
			{
				typeName += maybeName;
				return;
			}
			
			// We aren't at the end of the rabbit hole quite yet, have
			//  to get through some pointers first
			typeName.insert(0, "*");
			
			
			Value * v2 = csOp->getOperand(9);
			
			if (!isa<ConstantExpr>(v2))
			{
				//cout<<"Leaving getTypeFromDerived (4)"<<endl;
				string voidStr("VOID");
				typeName += voidStr;
				return;
			}
			
			ConstantExpr * ce = cast<ConstantExpr>(v2);
			User::op_iterator op_i = ce->op_begin();
			
			for (; op_i != ce->op_end(); op_i++)
			{
				
				Value * bitCastOp = *op_i;
				if (bitCastOp == NULL)
					return;
				
				//cout<<endl<<"bitCastOp (getTypeFromMD)- "<<bitCastOp->getName()<<endl;
				
				
				if (bitCastOp->getName().find("derived") != string::npos)
					getTypeFromDerived(bitCastOp, typeName);
				else if (bitCastOp->getName().find("basic") != string::npos)
					getTypeFromBasic(bitCastOp, typeName);
				else if (bitCastOp->getName().find("composite") != string::npos)
					getTypeFromComposite(bitCastOp, typeName);			
				
			}
		}
	}
	return;
}



string getTypeFromMD(Value *v)
{
	string noIdea("NO_IDEA");
	
	if (v == NULL)
		return noIdea;
	
	
	//cout<<"(getTypeFromMD) v Name -- "<<v->getName()<<endl;
	
	if (!isa<ConstantExpr>(v))
	{
		//cout<<"Leaving getTypeFromMD (2)"<<endl;
		return noIdea;
	}
	
  ConstantExpr * ce = cast<ConstantExpr>(v);
  User::op_iterator op_i = ce->op_begin();
	
	for (; op_i != ce->op_end(); op_i++)
	{
		
		Value * bitCastOp = *op_i;
		if (bitCastOp == NULL)
			return noIdea;
		
		//cout<<endl<<"bitCastOp (getTypeFromMD)- "<<bitCastOp->getName()<<endl;
		
		string typeName;
		
	  if (bitCastOp->getName().find("derived") != string::npos)
			getTypeFromDerived(bitCastOp, typeName);
		else if (bitCastOp->getName().find("basic") != string::npos)
			getTypeFromBasic(bitCastOp, typeName);
		else if (bitCastOp->getName().find("composite") != string::npos)
			getTypeFromComposite(bitCastOp, typeName);
		
		return typeName;
		
		//if (bitCastOp->getName().find("composite") == string::npos)
		//	return NULL;
	}
	
	return noIdea;
}




void StructBFC::setModuleName(string rawName)
{
	moduleName = rawName.substr(0, rawName.length() - 1);
	
}

void StructBFC::setModulePathName(string rawName)
{
	modulePathName = rawName.substr(0, rawName.length() - 1);
}
*/


void ModuleBFC::exportOneStruct(ostream &O, StructBFC *sb)
{
    O<<"BEGIN STRUCT"<<endl;
    
    O<<"BEGIN S_NAME "<<endl;
    O<<sb->structName<<endl;
    O<<"END S_NAME "<<endl;
    
    O<<"BEGIN M_PATH"<<endl;
    O<<sb->modulePathName<<endl;
    O<<"END M_PATH"<<endl;
    
    O<<"BEGIN M_NAME"<<endl;
    O<<sb->moduleName<<endl;
    O<<"END M_NAME"<<endl;
    
    O<<"BEGIN LINENUM"<<endl;
    O<<sb->lineNum<<endl;
    O<<"END LINENUM"<<endl;
    
    O<<"BEGIN FIELDS"<<endl;
    vector<StructField *>::iterator vec_sf_i;
    for (vec_sf_i = sb->fields.begin(); vec_sf_i != sb->fields.end(); vec_sf_i++) {
        O<<"BEGIN FIELD"<<endl;
        StructField * sf = (*vec_sf_i);
        if (sf == NULL) {
            O<<"END FIELD"<<endl;
            continue;
        }
        O<<"BEGIN F_NUM"<<endl;
        O<<sf->fieldNum<<endl;
        O<<"END F_NUM"<<endl;
        
        O<<"BEGIN F_NAME"<<endl;
        O<<sf->fieldName<<endl;
        O<<"END F_NAME"<<endl;
        
        O<<"BEGIN F_TYPE"<<endl;
        O<<sf->typeName<<endl;
        O<<"END F_TYPE"<<endl;
        
        O<<"END FIELD"<<endl;
    }
    
    O<<"END FIELDS"<<endl;
    
    //end of this new added struct
    O<<"END STRUCT"<<endl;
}


void ModuleBFC::exportStructs(ostream &O)
{
	vector<StructBFC *>::iterator vec_sb_i;
	
	for (vec_sb_i = structs.begin(); vec_sb_i != structs.end(); vec_sb_i++) {
		StructBFC * sb = (*vec_sb_i);
		O<<"BEGIN STRUCT"<<endl;
		
		O<<"BEGIN S_NAME "<<endl;
		O<<sb->structName<<endl;
		O<<"END S_NAME "<<endl;
		
		O<<"BEGIN M_PATH"<<endl;
		O<<sb->modulePathName<<endl;
		O<<"END M_PATH"<<endl;
		
		O<<"BEGIN M_NAME"<<endl;
		O<<sb->moduleName<<endl;
		O<<"END M_NAME"<<endl;
		
		O<<"BEGIN LINENUM"<<endl;
		O<<sb->lineNum<<endl;
		O<<"END LINENUM"<<endl;
		
		O<<"BEGIN FIELDS"<<endl;
		vector<StructField *>::iterator vec_sf_i;
		for (vec_sf_i = sb->fields.begin(); vec_sf_i != sb->fields.end(); vec_sf_i++) {
			O<<"BEGIN FIELD"<<endl;
			StructField * sf = (*vec_sf_i);
			if (sf == NULL) {
				O<<"END FIELD"<<endl;
				continue;
			}
			O<<"BEGIN F_NUM"<<endl;
			O<<sf->fieldNum<<endl;
			O<<"END F_NUM"<<endl;
			
			O<<"BEGIN F_NAME"<<endl;
			O<<sf->fieldName<<endl;
			O<<"END F_NAME"<<endl;
			
			O<<"BEGIN F_TYPE"<<endl;
			O<<sf->typeName<<endl;
			O<<"END F_TYPE"<<endl;
			
			O<<"END FIELD"<<endl;
		}
		
		O<<"END FIELDS"<<endl;
		
		O<<"END STRUCT"<<endl;
	}
	
    //we move the following MARK to the end of runOnModule since we may addin more
    //when firstPassing each user functions (pidArrays)
	//O<<"END STRUCTS"<<endl;
}


string returnTypeName(const llvm::Type * t, string prefix)
{
	if (t == NULL)
		return prefix += string("NULL");
	
    unsigned typeVal = t->getTypeID();
    if (typeVal == Type::VoidTyID)
        return prefix += string("Void");
    else if (typeVal == Type::FloatTyID)
        return prefix += string("Float");
    else if (typeVal == Type::DoubleTyID)
        return prefix += string("Double");
    else if (typeVal == Type::X86_FP80TyID)
        return prefix += string("80 bit FP");
    else if (typeVal == Type::FP128TyID)
        return prefix += string("128 bit FP");
    else if (typeVal == Type::PPC_FP128TyID)
        return prefix += string("2-64 bit FP");
    else if (typeVal == Type::LabelTyID)
        return prefix += string("Label");
    else if (typeVal == Type::IntegerTyID)
        return prefix += string("Int");
    else if (typeVal == Type::FunctionTyID)
        return prefix += string("Function");
    else if (typeVal == Type::StructTyID)
        return prefix += string("Struct");
    else if (typeVal == Type::ArrayTyID)
        return prefix += string("Array");
    else if (typeVal == Type::PointerTyID)
        return prefix += returnTypeName(cast<PointerType>(t)->getElementType(),
			string("*"));
    else if (typeVal == Type::MetadataTyID)
        return prefix += string("Metadata");
    else if (typeVal == Type::VectorTyID)
        return prefix += string("Vector");
    else
        return prefix += string("UNKNOWN");
}


void ModuleBFC::printStructs()
{
	vector<StructBFC *>::iterator vec_sb_i;
	
	for (vec_sb_i = structs.begin(); vec_sb_i != structs.end(); vec_sb_i++)	{
		cout<<endl<<endl;
		StructBFC * sb = (*vec_sb_i);
		cout<<"Struct "<<sb->structName<<endl;
		cout<<"Module Path "<<sb->modulePathName<<endl;
		cout<<"Module Name "<<sb->moduleName<<endl;
		cout<<"Line Number "<<sb->lineNum<<endl;
		
		vector<StructField *>::iterator vec_sf_i;
		for (vec_sf_i = sb->fields.begin(); vec_sf_i != sb->fields.end(); vec_sf_i++) {
			StructField * sf = (*vec_sf_i);
			if (sf == NULL)
				continue;
			cout<<"   Field # "<<sf->fieldNum<<" ";
			cout<<", Name "<<sf->fieldName<<" ";
			cout<<" Type "<<sf->typeName<<" ";
			//cout<<", Type "<<returnTypeName(sf->llvmType, string(" "));
			cout<<endl;
		}
	}
}


StructBFC * ModuleBFC::structLookUp(string &sName)
{
  vector<StructBFC *>::iterator vec_sb_i;
  for (vec_sb_i = structs.begin(); vec_sb_i != structs.end(); vec_sb_i++) {
	StructBFC * sb = (*vec_sb_i);
		
	if (sb->structName == sName)
	  return sb;
  }

  return NULL;
}


void ModuleBFC::addStructBFC(StructBFC * sb)
{
	//cout<<"Entering addStructBFC"<<endl;
	vector<StructBFC *>::iterator vec_sb_i;
#ifdef DEBUG_P
    cout<<sb->structName<<": moduleName = "<<sb->moduleName<< \
        " modulePathName = "<<sb->modulePathName<<endl;
#endif

	for (vec_sb_i = structs.begin(); vec_sb_i != structs.end(); vec_sb_i++) {
	  StructBFC *sbv = (*vec_sb_i);
	  if (sbv->structName == sb->structName) {
		if ((sbv->moduleName==sb->moduleName && sbv->modulePathName==sb->modulePathName) || 
            (sbv->moduleName.empty() && sb->moduleName.empty() && sbv->modulePathName.empty() && sb->modulePathName.empty())) {
			
          // Already have this exact struct, free the space allocated for this sb
          vector<StructField*>::iterator sf_i, sf_e;
          for (sf_i=sb->fields.begin(), sf_e=sb->fields.end(); sf_i!=sf_e; sf_i++) {
            StructField *sf = *sf_i;
            delete sf;
          }
		  delete sb;
		  
          return;
		}
		
        else {
#ifdef DEBUG_ERROR			
		  cerr<<"Two structs with same name and different declaration sites"<<endl;
#endif
		}
	  }
	}
	
	structs.push_back(sb);
    exportOneStruct(struct_file, sb); //added 03/31/17
}


StructBFC* ModuleBFC::findOrCreatePidArray(string pidArrayName, int numElems, const llvm::Type *sbPointT)
{
    StructBFC *retSB = NULL;
    retSB = structLookUp(pidArrayName);
    if (retSB != NULL)
      return retSB;
    
    else {
      retSB = new StructBFC();
      retSB->structName = pidArrayName;
      //we leave context information blank since we dont know & we dont need them
      //That includes: lineNum, moduleName, and modulePathName
      // This should be true
      if (sbPointT->isArrayTy()) {
        const llvm::Type *pidType = cast<ArrayType>(sbPointT)->getElementType();
        for (int i=0; i<numElems; i++) {
          StructField *sf = new StructField(i); //fieldNum
          char tempBuf[20];
          sprintf(tempBuf, "pid_x%d", i);

          sf->fieldName = string(tempBuf); //fieldName
          sf->llvmType = pidType; //field->llvmType
          sf->typeName = returnTypeName(sf->llvmType, ""); // field->typeName
          sf->parentStruct = retSB;
          // add this field to the struct
          retSB->fields.push_back(sf);
        }
      }
      else {
        cerr<<"Error, non pid array apears in findOrCreatePidArray:"<<
            pidArrayName<<endl;
        delete retSB; 
        return NULL;
      }
    }
    
    addStructBFC(retSB);
    return retSB;
}

    


void StructBFC::setModuleNameAndPath(llvm::DIScope *contextInfo)
{
	if (contextInfo == NULL)
		return;

	this->moduleName = contextInfo->getFilename().str();
    this->modulePathName = contextInfo->getDirectory().str();
}


void ModuleBFC::parseDITypes(DIType* dt) //deal with derived type and composite type
{
#ifdef DEBUG_P    //stands for debug_print
    cout<<"Entering parseDITypes for "<<dt->getName().str()<<endl;
#endif	
    StructBFC *sb;
    string dtName = dt->getName().str();
    if (dtName.compare("object") && dtName.compare("super")) {
        if (dtName.find("chpl_") == 0) //not deal types that "start"(because pos==0) with "chpl_"
            return;

        if (dt->isCompositeType()) { //DICompositeType:public DIDerivedType
#ifdef DEBUG_P
            cout<<"Composite Type primary for: "<<dt->getName().str()<<endl;
#endif		
            sb = new StructBFC();
            if(parseCompositeType(dt, sb, true))
                addStructBFC(sb);
            else
                delete sb;
        }	
        
        else if ((dt->isDerivedType()) && (dt->getTag()!=dwarf::DW_TAG_member)) {
#ifdef DEBUG_P
            cout<<"Derived Type: "<<dt->getName().str()<<endl;
#endif		
            sb = new StructBFC();
            if(parseDerivedType(dt, sb, NULL, false))
                addStructBFC(sb);
            else
                delete sb;
            
        }	
    }
	//cout<<"Leaving parseStruct for "<<gv->getNameStart()<<endl;
}



/*
!6 = metadata !{
   i32,      ;; Tag (see below)
   metadata, ;; Reference to context
   metadata, ;; Name (may be "" for anonymous types)
   metadata, ;; Reference to file where defined (may be NULL)
   i32,      ;; Line number where defined (may be 0)
   i64,      ;; Size in bits
   i64,      ;; Alignment in bits
   i64,      ;; Offset in bits
   i32,      ;; Flags
   metadata, ;; Reference to type derived from
   metadata, ;; Reference to array of member descriptors
   i32       ;; Runtime languages
}*/
bool ModuleBFC::parseCompositeType(DIType *dt, StructBFC *sb, bool isPrimary)
{
    if (sb == NULL)
		return false;
	
	if (dt == NULL) 
		return false;
    else if (!dt->isCompositeType()) {//verify that dt is a composite type
#ifdef DEBUG_P
        cout<<"parseCompositeType "<<dt->getName().str()<<" failed in Cond1"<<endl;
#endif
        return false; }
    else if (!(getDICompositeType(*dt).Verify())) {
#ifdef DEBUG_P
        cout<<"parseCompositeType "<<dt->getName().str()<<" failed in Cond2"<<endl;
#endif
        return false; }
    else {
        // There are no other typedefs that would alias to this
		if (isPrimary) {
		    sb->lineNum = dt->getLineNumber();
			sb->structName = dt->getName().str();//dt->getStringField(2).str();
            sb->setModuleNameAndPath(dt);
		}
        //DICompositeType dct = dyn_cast<DICompositeType>(dt);
        DICompositeType dct = DICompositeType(*dt);
        DIArray fields = dct.getTypeArray(); // !3 = !{!1,!2}
                                                  //return the mdnode for members
        if (fields) { 
//            if(isa<MDNode>(fields)) {
//                MDNode *MDFields = cast<MDNode>(fields); //!3=!{!1,!2}
                if(fields.getNumElements()==0) {
#ifdef DEBUG_P
                    cout<<"parseCompositeType "<<dt->getName().str()<<" failed in Cond3"<<endl;
#endif
                    return false; }
                //else omitted
                int numFields = fields.getNumElements();
                int fieldNum = 0;

                for (int i=0; i<numFields; i++) {
                    DIDescriptor field = fields.getElement(i);
                    Value *fieldVal = field.operator->();

                    if (isa<MDNode>(fieldVal)) {
                        MDNode *MDFieldVal = cast<MDNode>(fieldVal); //!1
                        if (MDFieldVal->getNumOperands()==0) {
#ifdef DEBUG_P
                            cout<<"parseCompositeType "<<dt->getName().str()<<" failed in Cond4"<<endl;
#endif
                            return false; }

                        else { 
                            // ?? fieldContents == tag ??
                            DIDescriptor *ddField = new DIDescriptor(MDFieldVal);
                            StructField * sf = new StructField(fieldNum);
                            if (!ddField->isType()) {
#ifdef DEBUG_P
                                cout<<"parseCompositeType "<<dt->getName().str()<<" failed in Cond5"<<endl;
#endif
                                return false; 
                            }
                             
                            DIType *dtField = (DIType *)ddField;

                            bool success;
                            success = parseDerivedType(dtField, NULL, sf, true);
                            if (success) {
                                sb->fields.push_back(sf);
                                sf->parentStruct = sb;
                                fieldNum++;
                            }
                            else {
#ifdef DEBUG_P
                                cout<<"parseCompositeType "<<dt->getName().str()<<" failed in Cond6"<<endl;
#endif
                                delete sf;
                                return false;
                            }
                        }
                    }
                }
#ifdef DEBUG_P
                cout<<"parseCompositeType SUCCEED @ "<<dt->getName().str()<<endl;
#endif
                return true;
                            
//            }
        }
	}
#ifdef DEBUG_P
    cout<<"parseCompositeType "<<dt->getName().str()<<" failed in Cond7"<<endl;
#endif
	//cout<<"Leaving parseCompositeType(3)"<<endl;
	return false;
}





/*
!5 = metadata !{
  i32,      ;; Tag (see below)
  metadata, ;; Reference to context
  metadata, ;; Name (may be "" for anonymous types) //
  metadata, ;; Reference to file where defined (may be NULL)
  i32,      ;; Line number where defined (may be 0)
  i64,      ;; Size in bits
  i64,      ;; Alignment in bits
  i64,      ;; Offset in bits
  i32,      ;; Flags to encode attributes, e.g. private
  metadata, ;; Reference to type derived from
  metadata, ;; (optional) Name of the Objective C property associated with
            ;; Objective-C an ivar, or the type of which this
            ;; pointer-to-member is pointing to members of.
  metadata, ;; (optional) Name of the Objective C property getter selector.
  metadata, ;; (optional) Name of the Objective C property setter selector.
  i32       ;; (optional) Objective C property attributes.
}
 */
//void ModuleBFC::parseDerivedType(GlobalVariable *gv)
bool ModuleBFC::parseDerivedType(DIType *dt, StructBFC *sb, StructField *sf, bool isField)
{    
	if (dt == NULL) // both sf and sb can be NULL here
		return false;
    else if (!dt->isDerivedType()) {//verify that dt is a Derived type
#ifdef DEBUG_P
        cout<<"parseDerivedType "<<dt->getName().str()<<" failed in Cond1"<<endl;
#endif
        return false; }
    else if (!(dt->operator->()->getNumOperands() >= 10 && dt->operator->()->getNumOperands() <= 14)) {
#ifdef DEBUG_P
        cout<<"parseDerivedType "<<dt->getName().str()<<" failed in Cond2"<<endl;
#endif
        return false; }
    else {
        DIDerivedType *ddt = (DIDerivedType *)dt;
		DIType derivedFromType = ddt->getTypeDerivedFrom(); 
		
        if (isField == false) {
            if (derivedFromType.isCompositeType()) { //only care about 
                                                    //direct derived composite
                DICompositeType *dfct = (DICompositeType *) &derivedFromType; //derivedFromCompositeType -> dfct
                //TODO:replace the below func with easier method//
                //DIScope dfct_scope = dfct->getContext();
                //sb->setModuleNameAndPath(&dfct_scope);
                return parseCompositeType(dfct, sb, true);//not see a case for 
            }                                      //isPrimary=false yet
        }
        else {
            sf->fieldName = dt->getName().str();
            //DIType *dft = dyn_cast_or_null<DIType>(&derivedFromType);
            sf->typeName = derivedFromType.getName().str();//getStringField(2).str();
#ifdef DEBUG_P
            cout<<"parseDerivedType (as field) SUCCEED @name: "<<sf->fieldName \
                <<" fieldType: "<<sf->typeName<<endl;
#endif
            return true;
        }
    }
#ifdef DEBUG_P
    cout<<"parseDerivedType "<<dt->getName().str()<<" failed in Cond3"<<endl;
#endif
	//cout<<"Leaving parseDerivedType (4)"<<endl;
	return false;
}



