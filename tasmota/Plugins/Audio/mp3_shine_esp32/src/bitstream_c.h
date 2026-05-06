/*
 *  bit_stream.c package
 *  Author:  Jean-Georges Fritsch, C-Cube Microsystems
 *
 * This package provides functions to write information to the bit stream.
 *
 * Removed unused functions. Feb 2001 P.Everett
 */


/* open the device to write the bit stream into it */
MODULE_PART  void p_shine_open_bit_stream(bitstream_t *bs, int32_t size) {
SETMEMREGS
  bs->data = (uint8_t *)malloc(size * sizeof(uint8_t));
  bs->data_size = size;
  bs->data_position = 0;
  bs->cache = 0;
  bs->cache_bits = 32;
}

/*close the device containing the bit stream */
MODULE_PART  void p_shine_close_bit_stream(bitstream_t *bs) {
SETMEMREGS
  if (bs->data)
    free(bs->data);
}

/*
 * shine_putbits:
 * --------
 * write N bits into the bit stream.
 * bs = bit stream structure
 * val = value to write into the buffer
 * N = number of bits of val
 */
MODULE_PART void p_shine_putbits(bitstream_t *bs, uint32_t val, uint32_t N) {
SETMEMREGS
#ifdef SHINE_DEBUG
	if (N > 32) {
		printf("Cannot write more than 32 bits at a time.\n");
  }
	if (N < 32 && (val >> N) != 0) {
		printf("Upper bits (higher than %d) are not all zeros.\n", N);
  }
#endif

	if (bs->cache_bits > N) {
		bs->cache_bits -= N;
		bs->cache |= val << bs->cache_bits;
	} else {
		if (bs->data_position + sizeof(uint32_t) >= bs->data_size) {
			bs->data = (uint8_t *)realloc(bs->data, bs->data_size + (bs->data_size / 2));
			bs->data_size += (bs->data_size / 2);
		}

		N -= bs->cache_bits;
		bs->cache |= val >> N;
#ifdef SHINE_BIG_ENDIAN
		*(uint32_t*)(bs->data + bs->data_position) = bs->cache;
#else
		// PIC-CROSS-ARCH FIX: was `SWAB32(bs->cache)` which expands to
		// `__builtin_bswap32(...)`. At the optimization level this MODULE_PART
		// is built with, GCC emits a CALL to libgcc's `__bswapsi2` helper
		// instead of inlining the swap. The literal pool stores the absolute
		// firmware address of `__bswapsi2`, which the PIC loader cannot
		// relocate — on a different chip the indirect call lands at junk PC.
		// Spelling the swap out explicitly keeps it inline (4 shifts + 4 ors)
		// with zero external function references.
		{
		  uint32_t _v = bs->cache;
		  uint32_t _swabbed = ((_v >> 24) & 0x000000FF) |
		                      ((_v >>  8) & 0x0000FF00) |
		                      ((_v <<  8) & 0x00FF0000) |
		                      ((_v << 24) & 0xFF000000);
		  *(unsigned int*)(bs->data + bs->data_position) = _swabbed;
		}
#endif
		bs->data_position += sizeof(uint32_t);
		bs->cache_bits = 32 - N;
		if (N != 0)
			bs->cache = val << bs->cache_bits;
		else
			bs->cache = 0;
	}
}

MODULE_PART int32_t p_shine_get_bits_count(bitstream_t *bs) {
	return bs->data_position * 8 + 32 - bs->cache_bits;
}
