

































#ifndef PICOKPR_H_
#define PICOKPR_H_

#include "picoos.h"
#include "picoknow.h"





 
 
 
 

pico_status_t picokpr_specializePreprocKnowledgeBase(picoknow_KnowledgeBase thiz,
                                                     picoos_Common common);


 
 
 

 
#define NPPMaxStrArrLen       100000000
#define NPPMaxLexCatArrLen    65536
#define NPPMaxAttrValLen      65536
#define NPPMaxOutItemArrLen   65536
#define NPPMaxTokArrLen       65536
#define NPPMaxProdArrLen      65536
#define NPPMaxCtxArrLen       65536

 
typedef picoos_uint32 picokpr_StrArrOffset;
typedef picoos_uint16 picokpr_LexCatArrOffset;
typedef picoos_uint16 picokpr_AttrValArrOffset;
typedef picoos_uint16 picokpr_OutItemArrOffset;
typedef picoos_uint16 picokpr_TokArrOffset;
typedef picoos_uint16 picokpr_ProdArrOffset;
typedef picoos_uint16 picokpr_CtxArrOffset;

typedef picoos_uchar * picokpr_VarStrPtr;

typedef picoos_int16 picokpr_LexCat;
typedef picoos_uint32 picokpr_TokSetNP;
typedef picoos_uint32 picokpr_TokSetWP;

 
typedef struct picokpr_preproc * picokpr_Preproc;

 
picokpr_Preproc picokpr_getPreproc(picoknow_KnowledgeBase thiz);


 
 
 

 
extern picokpr_VarStrPtr picokpr_getVarStrPtr(picokpr_Preproc preproc, picokpr_StrArrOffset ofs);
extern picoos_bool picokpr_isEqual (picokpr_Preproc preproc, picoos_uchar str[], picoos_int32 len__9, picokpr_StrArrOffset str2);
extern picoos_bool picokpr_isEqualHead (picokpr_Preproc preproc, picoos_uchar str[], picoos_int32 len__10, picokpr_StrArrOffset head);
extern picoos_bool picokpr_isEqualMid (picokpr_Preproc preproc, picoos_uchar str[], picoos_int32 len__11, picokpr_StrArrOffset mid);
extern picoos_bool picokpr_isEqualTail (picokpr_Preproc preproc, picoos_uchar str[], picoos_int32 len__12, picokpr_StrArrOffset tail);

 
extern picokpr_LexCat picokpr_getLexCat(picokpr_Preproc preproc, picokpr_LexCatArrOffset ofs);

 
extern picoos_int32 picokpr_getAttrValArrInt32(picokpr_Preproc preproc, picokpr_AttrValArrOffset ofs);

 
extern picokpr_StrArrOffset picokpr_getOutItemStrOfs(picokpr_Preproc preproc, picokpr_OutItemArrOffset ofs);
extern picokpr_VarStrPtr picokpr_getOutItemStr(picokpr_Preproc preproc, picokpr_OutItemArrOffset ofs);
extern picoos_int32 picokpr_getOutItemType(picokpr_Preproc preproc, picokpr_OutItemArrOffset ofs);
extern picoos_int32 picokpr_getOutItemVal(picokpr_Preproc preproc, picokpr_OutItemArrOffset ofs);
extern picokpr_OutItemArrOffset picokpr_getOutItemArgOfs(picokpr_Preproc preproc, picokpr_OutItemArrOffset ofs);
extern picokpr_OutItemArrOffset picokpr_getOutItemNextOfs(picokpr_Preproc preproc, picokpr_OutItemArrOffset ofs);

 
extern picokpr_TokSetNP picokpr_getTokSetNP(picokpr_Preproc preproc, picokpr_TokArrOffset ofs);
extern picokpr_TokSetWP picokpr_getTokSetWP(picokpr_Preproc preproc, picokpr_TokArrOffset ofs);
extern picokpr_TokArrOffset picokpr_getTokNextOfs(picokpr_Preproc preproc, picokpr_TokArrOffset ofs);
extern picokpr_TokArrOffset picokpr_getTokAltLOfs(picokpr_Preproc preproc, picokpr_TokArrOffset ofs);
extern picokpr_TokArrOffset picokpr_getTokAltROfs(picokpr_Preproc preproc, picokpr_TokArrOffset ofs);
extern picokpr_AttrValArrOffset picokpr_getTokAttribOfs(picokpr_Preproc preproc, picokpr_TokArrOffset ofs);

 
extern picoos_int32 picokpr_getProdArrLen(picokpr_Preproc preproc);
extern picoos_int32 picokpr_getProdPrefCost(picokpr_Preproc preproc, picokpr_ProdArrOffset ofs);
extern picokpr_StrArrOffset picokpr_getProdNameOfs(picokpr_Preproc preproc, picokpr_ProdArrOffset ofs);
extern picokpr_TokArrOffset picokpr_getProdATokOfs(picokpr_Preproc preproc, picokpr_ProdArrOffset ofs);
extern picokpr_TokArrOffset picokpr_getProdETokOfs(picokpr_Preproc preproc, picokpr_ProdArrOffset ofs);

 
extern picoos_int32 picokpr_getCtxArrLen(picokpr_Preproc preproc);
extern picokpr_StrArrOffset picokpr_getCtxCtxNameOfs(picokpr_Preproc preproc, picokpr_CtxArrOffset ofs);
extern picokpr_StrArrOffset picokpr_getCtxNetNameOfs(picokpr_Preproc preproc, picokpr_CtxArrOffset ofs);
extern picokpr_StrArrOffset picokpr_getCtxProdNameOfs(picokpr_Preproc preproc, picokpr_CtxArrOffset ofs);

 
extern picokpr_VarStrPtr picokpr_getPreprocNetName(picokpr_Preproc preproc);




#endif  
