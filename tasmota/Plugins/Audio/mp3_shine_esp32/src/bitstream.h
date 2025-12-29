#ifndef BITSTREAM_H
#define BITSTREAM_H

typedef struct  bit_stream_struc {
    uint8_t     *data;        /* Processed data */
    int32_t     data_size;      /* Total data size */
    int32_t     data_position;  /* Data position */
    uint32_t    cache;			/* bit stream cache */
    int32_t     cache_bits;     /* free bits in cache */
} bitstream_t;

/* "bit_stream.h" Definitions */

#define         MINIMUM         4    /* Minimum size of the buffer in bytes */
#define         MAX_LENGTH      32   /* Maximum length of word written or
                                        read from bit stream */

#define         BUFFER_SIZE     4096

//#define         MIN(A, B)       ((A) < (B) ? (A) : (B))
//#define         MAX(A, B)       ((A) > (B) ? (A) : (B))

MODULE_PART void p_shine_open_bit_stream(bitstream_t *bs, const int32_t size);
MODULE_PART void  p_shine_close_bit_stream(bitstream_t *bs);
MODULE_PART void  p_shine_putbits(bitstream_t *bs, uint32_t val, uint32_t N);
MODULE_PART int32_t   p_shine_get_bits_count(bitstream_t *bs);

#endif
