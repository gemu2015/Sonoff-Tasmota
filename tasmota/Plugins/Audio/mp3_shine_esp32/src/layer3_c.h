/* layer3.c */

#ifdef ESP32


// counter[] moved to MODULE_MEMORY as shine_counter[]
#define counter mem->shine_counter

const int32_t granules_per_frame[4] PROGMEM = {
    1,  /* MPEG 2.5 */
   -1,  /* Reserved */
    1,  /* MPEG II */
    2  /* MPEG I */
};

/* Set default values for important vars */
MODULE_PART void p_shine_set_config_mpeg_defaults(shine_mpeg_t *mpeg) {
  mpeg->bitr = 128;
  mpeg->emph = NONE;
  mpeg->copyright = 0;
  mpeg->original  = 1;
}

MODULE_PART int32_t p_shine_mpeg_version(int32_t samplerate_index) {
  /* Pick mpeg version according to samplerate index. */
  if (samplerate_index < 3) {
    /* First 3 samplerates are for MPEG-I */
    return MPEG_I;
  } else if (samplerate_index < 6) {
    /* Then it's MPEG-II */
    return MPEG_II;
  } else {
    /* Finally, MPEG-2.5 */
    return MPEG_25;
  }
}

MODULE_PART int32_t p_shine_find_samplerate_index(int32_t freq) {
SETMEMREGS
  int32_t i;

  for(i=0;i<9;i++) {
    if(freq==GTAB_I32(samplerates)[i]) return i;
  }
  return -1; /* error - not a valid samplerate for encoder */
}

MODULE_PART int32_t p_shine_find_bitrate_index(int32_t bitr, int32_t mpeg_version) {
SETMEMREGS
  int32_t i;

  for(i=0;i<16;i++) {
    if(bitr==GTAB_I32((const int32_t*)bitrates)[i*4+mpeg_version]) return i;
  }
  return -1; /* error - not a valid samplerate for encoder */
}

MODULE_PART int32_t p_shine_check_config(int32_t freq, int32_t bitr) {
  int32_t samplerate_index, bitrate_index, mpeg_version;

  samplerate_index = p_shine_find_samplerate_index(freq);
  if (samplerate_index < 0) {
    return -1;
  }
  mpeg_version = p_shine_mpeg_version(samplerate_index);

  bitrate_index = p_shine_find_bitrate_index(bitr, mpeg_version);
  if (bitrate_index < 0) {
    return -1;
  }
  return mpeg_version;
}

MODULE_PART int32_t p_shine_samples_per_pass(shine_t s) {
SETMEMREGS
  return s->mpeg.granules_per_frame * (int32_t)INTC(8);
}

/* Compute default encoding values. */
MODULE_PART shine_global_config *p_shine_initialise(shine_config_t *pub_config) {
SETMEMREGS
  SHINE_DOUBLE avg_slots_per_frame;
  shine_global_config *config;
  int32_t x, y;
  if (p_shine_check_config(pub_config->wave.samplerate, pub_config->mpeg.bitr) < 0) {
    return NULL;
  }

  config = (shine_global_config*)malloc(ICONST(sizeof(shine_global_config)));
  if (config == NULL) {
    return config;
  }

  memset(config, 0, ICONST(sizeof(shine_global_config)));

#ifdef  SHINE_DEBUG
  printf("l3_enc & mdct_freq each: %d\n", sizeof(int32_t)*GRANULE_SIZE*MAX_GRANULES*MAX_CHANNELS);
#endif

  for (x = 0; x < MAX_CHANNELS; x++) {
      for (y = 0; y < MAX_GRANULES; y++) {
        // 2 * 2 * 576 each
        config->l3_enc[x][y] = (int32_t*)malloc((int32_t)INTC(13));
        if (!config->l3_enc[x][y]) {
          // error should never occur because of spiram size
        }
        config->mdct_freq[x][y] = (int32_t*)malloc((int32_t)INTC(13));
        if (!config->mdct_freq[x][y]) {
          // error
        }
      }
  }
#ifdef  SHINE_DEBUG
  printf("l3loop struct: %d\n", sizeof(l3loop_t));
#endif
  config->l3loop = (l3loop_t*)malloc(ICONST(sizeof(l3loop_t)));
#ifdef  SHINE_DEBUG
  printf("xrsq & xrabs each: %d\n", sizeof(int32_t)*GRANULE_SIZE);
#endif
  config->l3loop->xrsq = (int32_t*)malloc((int32_t)INTC(13));
  if (!config->l3loop->xrsq) {
    // error
  }

  config->l3loop->xrabs = (int32_t*)malloc((int32_t)INTC(13));
  if (!config->l3loop->xrabs) {
    // error
  }

/*typedef struct {
  int32_t *xr;
  int32_t *xrsq[GRANULE_SIZE];
  int32_t *xrabs[GRANULE_SIZE];
  int32_t xrmax;
  int32_t en_tot[MAX_GRANULES];
  int32_t en[MAX_GRANULES][21];
  int32_t xm[MAX_GRANULES][21];
  int32_t xrmaxl[MAX_GRANULES];
  SHINE_DOUBLE steptab[128];
  int32_t steptabi[128];
  int16_t int2idx[10000];
} l3loop_t;*/

  p_shine_subband_initialise(config);
  p_shine_mdct_initialise(config);
  p_shine_loop_initialise(config);

  /* Copy public config. */
  config->wave.channels   = pub_config->wave.channels;
  config->wave.samplerate = pub_config->wave.samplerate;
  config->mpeg.mode       = pub_config->mpeg.mode;
  config->mpeg.bitr       = pub_config->mpeg.bitr;
  config->mpeg.emph       = pub_config->mpeg.emph;
  config->mpeg.copyright  = pub_config->mpeg.copyright;
  config->mpeg.original   = pub_config->mpeg.original;

  /* Set default values. */
  config->ResvMax             = 0;
  config->ResvSize            = 0;
  config->mpeg.layer          = LAYER_III;
  config->mpeg.crc            = 0;
  config->mpeg.ext            = 0;
  config->mpeg.mode_ext       = 0;
  config->mpeg.bits_per_slot  = 8;

  config->mpeg.samplerate_index   = p_shine_find_samplerate_index(config->wave.samplerate);
  config->mpeg.version            = p_shine_mpeg_version(config->mpeg.samplerate_index);
  config->mpeg.bitrate_index      = p_shine_find_bitrate_index(config->mpeg.bitr, config->mpeg.version);
  config->mpeg.granules_per_frame = GTAB_I32(granules_per_frame)[config->mpeg.version];

  /* Figure average number of 'slots' per frame. */
  avg_slots_per_frame = fmul(fdiv(fmul(float_i32(config->mpeg.granules_per_frame), float_i32((int32_t)INTC(8))),
                        float_i32(config->wave.samplerate)),
                        fdiv(fmul(float_i32(1000), float_i32(config->mpeg.bitr)),
                         float_i32(config->mpeg.bits_per_slot)));

  config->mpeg.whole_slots_per_frame  = fixsfti(avg_slots_per_frame);

  config->mpeg.frac_slots_per_frame  = fdiff(avg_slots_per_frame, float_i32(config->mpeg.whole_slots_per_frame));
  config->mpeg.slot_lag              = fneg(config->mpeg.frac_slots_per_frame);

  if(jeqsf2(config->mpeg.frac_slots_per_frame, float_i32(0))) {
    config->mpeg.padding = 0;
  }

  p_shine_open_bit_stream(&config->bs, (int32_t)INTC(12));

  memset((char *)&config->side_info,0,ICONST(sizeof(shine_side_info_t)));

  /* determine the mean bitrate for main data */
  if (config->mpeg.granules_per_frame == 2) { /* MPEG 1 */
    config->sideinfo_len = 8 * ((config->wave.channels==1) ? 4 + 17 : 4 + 32);
  } else {               /* MPEG 2 */
    config->sideinfo_len = 8 * ((config->wave.channels==1) ? 4 + 9 : 4 + 17);
  }
  return config;
}



MODULE_PART uint32_t *p_shine_get_counters() {
SETMEMREGS
  return counter;
}

/* Counter results
Counters 1550541561 : 1550541629 : 1553135798 : 1555116724 : 1555309952
68
core 1 will do:
2594169
core 0 will do:
1980926
193228


Counters 2664123380 : 2664123448 : 2666717886 : 2668665908 : 2668859025
*/


MODULE_PART uint8_t *p_shine_encode_buffer_internal(shine_global_config *config, int32_t *written, int32_t stride) {
SETMEMREGS
  counter[0] = 0;      // TASMOTA more portable solution
  // counter[0] = xthal_get_ccount();
  if(!jeqsf2(config->mpeg.frac_slots_per_frame, float_i32(0))) {
    { SHINE_DOUBLE _cmp_val = fdiff(config->mpeg.frac_slots_per_frame, FLTC(15));
      config->mpeg.padding   = (jltsf2(config->mpeg.slot_lag, _cmp_val) | jeqsf2(config->mpeg.slot_lag, _cmp_val)); }
    config->mpeg.slot_lag = fadd(config->mpeg.slot_lag, fdiff(float_i32(config->mpeg.padding), config->mpeg.frac_slots_per_frame));
  }

  config->mpeg.bits_per_frame = 8*(config->mpeg.whole_slots_per_frame + config->mpeg.padding);
  config->mean_bits = (config->mpeg.bits_per_frame - config->sideinfo_len)/config->mpeg.granules_per_frame;
  counter[1] = 0;      // TASMOTA more portable solution
  // counter[1] = xthal_get_ccount();
  /* apply mdct to the polyphase output */
  // put on core 1
  p_shine_mdct_sub(config, stride);
  counter[2] = 0;      // TASMOTA more portable solution
  // counter[2] = xthal_get_ccount();
  /* bit and noise allocation */
  //put on core 0
  p_shine_iteration_loop(config);
  counter[3] = 0;      // TASMOTA more portable solution
  // counter[3] = xthal_get_ccount();
  /* write the frame to the bitstream */
  p_shine_format_bitstream(config);
  counter[4] = 0;      // TASMOTA more portable solution
  // counter[4] = xthal_get_ccount();
  /* Return data. */
  *written = config->bs.data_position;
  config->bs.data_position = 0;

  return config->bs.data;
}

MODULE_PART uint8_t *p_shine_encode_buffer(shine_global_config *config, int16_t **data, int32_t *written) {
  config->buffer[0] = data[0];
  if (config->wave.channels == 2) {
    config->buffer[1] = data[1];
  }
  return p_shine_encode_buffer_internal(config, written, 1);
}

MODULE_PART uint8_t *p_shine_encode_buffer_interleaved(shine_global_config *config, int16_t *data, int32_t *written) {
  config->buffer[0] = data;
  if (config->wave.channels == 2) {
    config->buffer[1] = data + 1;
  }
  return p_shine_encode_buffer_internal(config, written, config->wave.channels);
}

MODULE_PART uint8_t *p_shine_flush(shine_global_config *config, int32_t *written) {
  *written = config->bs.data_position;
  config->bs.data_position = 0;
  return config->bs.data;
}

MODULE_PART void p_shine_close(shine_global_config *config) {
SETMEMREGS
  p_shine_close_bit_stream(&config->bs);

  for (uint16_t x = 0; x < MAX_CHANNELS; x++) {
      for (uint16_t y = 0; y < MAX_GRANULES; y++) {
        if (config->l3_enc[x][y]) {
          free(config->l3_enc[x][y]);
        }
        if (config->mdct_freq[x][y]) {
          free(config->mdct_freq[x][y]);
        }
      }
  }

  if (config->l3loop) {
    if (config->l3loop->xrsq) free(config->l3loop->xrsq);
    if (config->l3loop->xrabs) free(config->l3loop->xrabs);
    free(config->l3loop);
  }

  free(config);
}

#endif // ESP32
