/*
dnl $Id$
ifelse(TYPECODE,`CHAR',`define(`TYPE',`CHAR')')
ifelse(TYPECODE,`I2',`define(`TYPE',`INT2')')
ifelse(TYPECODE,`I4',`define(`TYPE',`INT4')')
ifelse(TYPECODE,`I8',`define(`TYPE',`INT8')')
ifelse(TYPECODE,`U2',`define(`TYPE',`UINT2')')
ifelse(TYPECODE,`U4',`define(`TYPE',`UINT4')')
ifelse(TYPECODE,`U8',`define(`TYPE',`UINT8')')
ifelse(TYPECODE,`S',`define(`TYPE',`REAL4')')
ifelse(TYPECODE,`D',`define(`TYPE',`REAL8')')
ifelse(TYPECODE,`C',`define(`TYPE',`COMPLEX8')')
ifelse(TYPECODE,`Z',`define(`TYPE',`COMPLEX16')')
define(`SEQTYPE',`format(`%sSequence',TYPE)')
define(`VSEQTYPE',`format(`%sVectorSequence',TYPE)')
define(`CREATESEQFUN',`format(`LAL%sCreateSequence',TYPECODE)')
define(`DESTROYSEQFUN',`format(`LAL%sDestroySequence',TYPECODE)')
define(`CREATEVSEQFUN',`format(`LAL%sCreateVectorSequence',TYPECODE)')
define(`DESTROYVSEQFUN',`format(`LAL%sDestroyVectorSequence',TYPECODE)')
*/

void CREATESEQFUN ( LALStatus *status,
          SEQTYPE   **sequence,
          UINT4);
          
void DESTROYSEQFUN ( LALStatus  *status,
          SEQTYPE   **sequence);
          
void CREATEVSEQFUN ( LALStatus *status,
          VSEQTYPE **vectorSequence,
          CreateVectorSequenceIn *vSeqParams);
          
void DESTROYVSEQFUN ( LALStatus *status,
          VSEQTYPE **vectorSequence);
          
