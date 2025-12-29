#define HUFFBITS uint16_t
#define HTN     34
#define MXOFF   250

struct p_huffcodetab {
  uint32_t xlen;         /*max. x-index+                         */
  uint32_t ylen;         /*max. y-index+                         */
  uint32_t linbits;      /*number of linbits                     */
  uint32_t linmax;       /*max number to be stored in linbits    */
  const HUFFBITS *table;     /*pointer to array[xlen][ylen]          */
  const uint8_t *hlen; /*pointer to array[xlen][ylen]          */
};

extern const struct p_huffcodetab shine_huffman_table[HTN];/* global memory block                */
                                                         /* array of all huffcodtable headers    */
                                                         /* 0..31 Huffman code table 0..31       */
                                                         /* 32,33 count1-tables                  */


