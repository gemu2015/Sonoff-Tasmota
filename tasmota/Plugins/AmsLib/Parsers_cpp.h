
time_t decodeCosemDateTime(CosemDateTime timestamp) {
SETREGS
    tmElements_t tm;
    uint16_t year = _ntohs(timestamp.year);
    if(year < 1970) return 0;
    tm.Year = year - 1970;
    tm.Month = timestamp.month;
    tm.Day = timestamp.dayOfMonth;
    tm.Hour = timestamp.hour;
    tm.Minute = timestamp.minute;
    tm.Second = timestamp.second;

    //Serial.printf("\nY: %d, M: %d, D: %d, h: %d, m: %d, s: %d, deviation: 0x%2X, status: 0x%1X\n", tm.Year, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second, timestamp.deviation, timestamp.status);

    time_t time = makeTime(tm);
    int16_t deviation = _ntohs(timestamp.deviation);
    if (deviation >= -720 && deviation <= 720) {
        time -= deviation * 60;
    }
    return time;
}

// ======================================================

const int32_t i32_co[3] PROGMEM = {UINT16_MAX,0x8408,0xa001}; 


uint16_t AMS_crc16_x25(const uint8_t* p, int len) {
SETREGS
    volatile const int32_t *ipc = (const int32_t *) ((uint8_t *)i32_co+EXEC_OFFSET);
	uint16_t crc = ipc[0];

	while (len--)
		for (uint16_t i = 0, d = 0xff & *p++; i < 8; i++, d >>= 1)
			crc = ((crc & 1) ^ (d & 1)) ? (crc >> 1) ^ ipc[1] : (crc >> 1);

	return (~crc << 8) | (~crc >> 8 & 0xff);
}

uint16_t AMS_crc16 (const uint8_t *p, int len) {
SETREGS
    volatile const int32_t *ipc = (const int32_t *) ((uint8_t *)i32_co+EXEC_OFFSET);
    uint16_t crc = 0;

    while (len--) {
		int i;
		crc ^= *p++;
		for (i = 0 ; i < 8 ; ++i) {
			if (crc & 1)
				crc = (crc >> 1) ^ ipc[2];
			else
				crc = (crc >> 1);
		}
    }
    return crc;
}

// ======================================================

int8_t DLMSParser_parse(Han_Parser *hp, uint8_t *buf, DataParserContext &ctx) {
SETREGS
    if(ctx.length < 6) return DATA_PARSE_INCOMPLETE;

    uint8_t* ptr = buf+1;
    ptr += 4; // Skip invoke ID and priority

    CosemData* item = (CosemData*) ptr;
    if (item->base.type == CosemTypeOctetString) {
        if(item->base.length == 0x0C) {
            CosemDateTime* dateTime = (CosemDateTime*) (ptr+1);
            ctx.timestamp = decodeCosemDateTime(*dateTime);
        }
        uint8_t len = 5 + 14;
        ctx.length -= len;
        return len;
    } else if(item->base.type == CosemTypeNull) {
        ctx.timestamp = 0;
        uint8_t len = 5 + 1;
        ctx.length -= len;
        return len;
    } else if(item->base.type == CosemTypeDateTime) {
        CosemDateTime* dateTime = (CosemDateTime*) (ptr);
        ctx.timestamp = decodeCosemDateTime(*dateTime);
        uint8_t len = 5 + 13;
        ctx.length -= len;
        return len;
    } else if(item->base.type == 0x0C) { // Kamstrup bug...
        CosemDateTime* dateTime = (CosemDateTime*) (ptr);
        ctx.timestamp = decodeCosemDateTime(*dateTime);
        uint8_t len = 5 + 13;
        ctx.length -= len;
        return len;
    }
    return DATA_PARSE_UNKNOWN_DATA;
}

// ======================================================

int8_t DSMRParser_parse(Han_Parser *hp, uint8_t *buf, DataParserContext &ctx, bool verified) {
SETREGS
    uint16_t crcPos = 0;
    bool reachedEnd = verified;
    uint8_t lastByte = 0x00;
    for (int pos = 0; pos < ctx.length; pos++) {
        uint8_t b = *(buf+pos);
        if (pos == 0 && b != '/') return DATA_PARSE_BOUNDRY_FLAG_MISSING;
        if (pos > 0 && b == '!' && lastByte == '\n') crcPos = pos+1;
        if (crcPos > 0 && b == '\n') reachedEnd = true;
        lastByte = b;
    }
    if (!reachedEnd) return DATA_PARSE_INCOMPLETE;
    buf[ctx.length+1] = '\0';
    if (crcPos > 0) {
	    uint16_t crc_calc = AMS_crc16(buf, crcPos);
        uint16_t crc = 0x0000;
        AMS_fromHex((uint8_t*) &crc, (char*) buf+crcPos, 2);
        crc = _ntohs(crc);

        if (crc != crc_calc)
            return DATA_PARSE_FOOTER_CHECKSUM_ERROR;
    }
    return DATA_PARSE_OK;
}

// ======================================================


int8_t GBTParser_parse(Han_Parser *hp, uint8_t *d, DataParserContext &ctx) {
SETREGS
    GBTHeader* h = (GBTHeader*) (d);
    uint16_t sequence = _ntohs(h->sequence);

    if (h->flag != GBT_TAG) return DATA_PARSE_BOUNDRY_FLAG_MISSING;

    if (sequence == 1) {
        if (hp->gbt.buf == NULL) hp->gbt.buf = (uint8_t *)calloc((size_t)1024, 1); // TODO find out from first package ?
        hp->gbt.pos = 0;
    } else if(hp->gbt.lastSequenceNumber != sequence-1) {
        return DATA_PARSE_FAIL;
    }

    if (hp->gbt.buf == NULL) return DATA_PARSE_FAIL;

    uint8_t* ptr = (uint8_t*) &h[1];
    memmove(hp->gbt.buf + hp->gbt.pos, ptr, h->size);
    hp->gbt.pos += h->size;
    hp->gbt.lastSequenceNumber = sequence;

    if ((h->control & 0x80) == 0x00) {
        return DATA_PARSE_INTERMEDIATE_SEGMENT;
    } else {
        memmove((uint8_t *) d, hp->gbt.buf, hp->gbt.pos);
    }
    ctx.length = hp->gbt.pos;
    return DATA_PARSE_OK;

}

// ======================================================

void New_GCMParser(Han_Parser *hp, uint8_t *encryption_key, uint8_t *authentication_key) {
SETREGS

    memmove(hp->gcm.encryption_key, encryption_key, 16);
    memmove(hp->gcm.authentication_key, authentication_key, 16);
    hp->gcm.use_auth = 0;
    for (uint16_t cnt = 0; cnt < 16; cnt++) {
      if (hp->gcm.authentication_key[cnt]) {
        hp->gcm.use_auth |= 1;
      }
    }
}

int8_t GCMParser_parse(Han_Parser *hp, uint8_t *d, DataParserContext &ctx) {
SETREGS
    if (ctx.length < 12) return DATA_PARSE_INCOMPLETE;

    uint8_t* ptr = (uint8_t*) d;
    if (*ptr != GCM_TAG) return DATA_PARSE_BOUNDRY_FLAG_MISSING;
    ptr++;
    // Encrypted APDU
    // http://www.weigu.lu/tutorials/sensors2bus/04_encryption/index.html

    uint8_t systemTitleLength = *ptr;
    ptr++;

    uint8_t initialization_vector[12];
    memmove(ctx.system_title, ptr, systemTitleLength);
    memmove(initialization_vector, ctx.system_title, systemTitleLength);

    int len = 0;
    int headersize = 2 + systemTitleLength;
    ptr += systemTitleLength;

    if (ctx.flags & 1) {
      len = *ptr;
        ptr++;
        headersize++;
    } else {
      if(((*ptr) & 0xFF) == 0x81) {
        ptr++;
        len = *ptr;
        // 1-byte payload length
        ptr++;
        headersize += 2;
      } else if(((*ptr) & 0xFF) == 0x82) {
        GCMSizeDef* h = (GCMSizeDef*) ptr;

        // 2-byte payload length
        len = (_ntohs(h->format) & 0xFFFF);

        ptr += 3;
        headersize += 3;
      } else  if(((*ptr) & 0xFF) == 0x4f) {
        // ???????? single frame did only decode with this compare
        ptr++;
        headersize++;
      } else  if(((*ptr) & 0xFF) == 0x5e) {
        // ???????? single frame did only decode with this compare
        ptr++;
        headersize++;
      } else {
        len = *ptr;
        ptr++;
        headersize++;
      }
    }
    if(len + headersize > ctx.length)
        return DATA_PARSE_INCOMPLETE;

    uint8_t additional_authenticated_data[17];
    memmove(additional_authenticated_data, ptr, 1);

    // Security tag
    uint8_t sec = *ptr;
    ptr++;
    headersize++;

    // Frame counter
    memmove(initialization_vector + 8, ptr, 4);
    ptr += 4;
    headersize += 4;

    int footersize = 0;

    // Authentication enabled
    uint8_t authentication_tag[12];
    uint8_t authkeylen = 0, aadlen = 0;
    if ((sec & 0x10) == 0x10) {
        authkeylen = 12;
        aadlen = 17;
        footersize += authkeylen;
        memmove(additional_authenticated_data + 1, hp->gcm.authentication_key, 16);
        memmove(authentication_tag, ptr + len - footersize - 5, authkeylen);
    }

    br_gcm_context gcm_ctx;
  	br_aes_small_ctr_keys ctr_ctx;
  	br_aes_small_ctr_init(&ctr_ctx, hp->gcm.encryption_key, 16);
  	br_gcm_init(&gcm_ctx, &ctr_ctx.vtable, &br_ghash_ctmul32);
    br_gcm_reset(&gcm_ctx, initialization_vector, 12);
    if (hp->gcm.use_auth && authkeylen > 0) {
      br_gcm_aad_inject(&gcm_ctx, additional_authenticated_data, aadlen);
    }
    br_gcm_flip(&gcm_ctx);
  	br_gcm_run(&gcm_ctx, 0, ptr , ctx.length - headersize);
    if (hp->gcm.use_auth && authkeylen > 0 && br_gcm_check_tag_trunc(&gcm_ctx, authentication_tag, authkeylen) != 1) {
      return GCM_AUTH_FAILED;
    }

    ctx.length -= footersize + headersize;
    return ptr - d;
}

// ======================================================

int8_t HDLCParser_parse(Han_Parser *hp, uint8_t *d, DataParserContext &ctx) {
SETREGS
    int len;

    uint8_t* ptr;
    if (ctx.length < 3)
        return DATA_PARSE_INCOMPLETE;

    HDLCHeader* h = (HDLCHeader*) d;
    ptr = (uint8_t*) &h[1];

    // Frame format type 3
    if ((h->format & 0xF0) == 0xA0) {
        // Length field (11 lsb of format)
        len = (_ntohs(h->format) & 0x7FF) + 2;
        if(len > ctx.length)
            return DATA_PARSE_INCOMPLETE;

        HDLCFooter* f = (HDLCFooter*) (d + len - sizeof *f);

        // First and last byte should be HDLC_FLAG
        if (h->flag != HDLC_FLAG || f->flag != HDLC_FLAG)
            return DATA_PARSE_BOUNDRY_FLAG_MISSING;

        // Verify FCS
        if (_ntohs(f->fcs) != AMS_crc16_x25(d + 1, len - sizeof *f - 1))
            return DATA_PARSE_FOOTER_CHECKSUM_ERROR;

        // Skip destination address, LSB marks last byte
        while (((*ptr) & 0x01) == 0x00) {
            ptr++;
        }
        ptr++;

        // Skip source address, LSB marks last byte
        while (((*ptr) & 0x01) == 0x00) {
            ptr++;
        }
        ptr++;

        HDLC3CtrlHcs* t3 = (HDLC3CtrlHcs*) (ptr);

        // Verify HCS
        if (_ntohs(t3->hcs) != AMS_crc16_x25(d + 1, ptr-d))
            return DATA_PARSE_HEADER_CHECKSUM_ERROR;
        ptr += 3;

        // Exclude all of header and 3 byte footer
        ctx.length -= ptr - d + 3;
        return ptr - d;
    }
    return DATA_PARSE_UNKNOWN_DATA;
}

// ======================================================


void AMS_fromHex(uint8_t *out, char *in, uint16_t size) {
SETREGS
    char hbuff[3];
    hbuff[2] = 0;
	for(int i = 0; i < size*2; i += 2) {
		//out[i/2] = strtol(in.substring(i, i+2).c_str(), 0, 16);
        hbuff[0] = *in++;
        hbuff[1] = *in++;
        *out++ = strtol(hbuff, 0, 16);
	}
}

// ======================================================

int8_t LLCParser_parse(Han_Parser *hp, uint8_t *buf, DataParserContext &ctx) {
    ctx.length -= 3;
    return 3;
}

// ======================================================

int8_t MBUSParser_parse(Han_Parser *hp, uint8_t *d, DataParserContext &ctx) {
SETREGS
    int len;
    int headersize = 3;
    int footersize = 1;

    uint8_t* ptr;

    // https://m-bus.com/documentation-wired/06-application-layer
    if (ctx.length < 4)
        return DATA_PARSE_INCOMPLETE;

    MbusHeader* mh = (MbusHeader*) d;
    if (mh->flag1 != MBUS_START || mh->flag2 != MBUS_START)
        return DATA_PARSE_BOUNDRY_FLAG_MISSING;

    // First two bytes is 1-byte length value repeated. Only used for last segment
    if (mh->len1 != mh->len2)
        return MBUS_FRAME_LENGTH_NOT_EQUAL;
    len = mh->len1;
    ptr = (uint8_t*) &mh[1];
    headersize = 4;
    footersize = 2;

    if (len == 0x00)
        len = ctx.length - headersize - footersize;
    // Payload can max be 255 bytes, so I think the following case is only valid for austrian meters
    if (len < headersize)
        len += 256;

    if ((headersize + footersize + len) > ctx.length)
        return DATA_PARSE_INCOMPLETE;

    MbusFooter* mf = (MbusFooter*) (d + len + headersize);
    if (mf->flag != MBUS_END)
        return DATA_PARSE_BOUNDRY_FLAG_MISSING;
    if (MBUSParser_checksum(d + headersize, len) != mf->fcs)
        return DATA_PARSE_FOOTER_CHECKSUM_ERROR;

    ptr += 2; len -= 2;

    // Control information field
    uint8_t ci = *ptr;

    // Skip CI, STSAP and DTSAP
    ptr += 3; len -= 3;

    // Bits 7 6 5 4         3 2 1 0
    //      0 0 0 Finished  Sequence number
    uint8_t sequenceNumber = (ci & 0x0F);
    if ((ci & 0x10) == 0x00) { // Not finished yet
        if(sequenceNumber == 0) {
            if (hp->mbus.buf == NULL) hp->mbus.buf = (uint8_t *)calloc((size_t)1024, 1); // TODO find out from first package ?
            hp->mbus.pos = 0;
        } else if(hp->mbus.buf == NULL || hp->mbus.pos + len > 1024 || sequenceNumber != (hp->mbus.lastSequenceNumber + 1)) {
            return DATA_PARSE_FAIL;
        }
        memmove(hp->mbus.buf+hp->mbus.pos, ptr, len);
        hp->mbus.pos += len;
        hp->mbus.lastSequenceNumber = sequenceNumber;
        return DATA_PARSE_INTERMEDIATE_SEGMENT;
    } else if(sequenceNumber > 0) { // This is the last frame of multiple, assembly needed
        if(hp->mbus.buf == NULL || hp->mbus.pos + len > 1024 || sequenceNumber != (hp->mbus.lastSequenceNumber + 1)) {
            return DATA_PARSE_FAIL;
        }
        memmove(hp->mbus.buf+hp->mbus.pos, ptr, len);
        hp->mbus.pos += len;
        return DATA_PARSE_FINAL_SEGMENT;
    }
    return ptr-d;
}

uint16_t MBUSParser_write(Han_Parser *hp, const uint8_t* d, DataParserContext &ctx) {
SETREGS
    if (hp->mbus.buf != NULL) {
        memmove((uint8_t *) d, hp->mbus.buf, hp->mbus.pos);
        ctx.length = hp->mbus.pos;
    }
    return 0;
}

uint8_t MBUSParser_checksum(const uint8_t* p, int len) {
    uint8_t ret = 0;
    while(len--)
        ret += *p++;
    return ret;
}

// ======================================================

MODULE_PART int HAN_print(const char *format, ...) {
SETREGS

	char loc_buf[64];
	char* temp = loc_buf;
	int len;
	va_list arg;
	va_list copy;
	va_start(arg, format);
	va_copy(copy, arg);
	len = vsnprintf_P(NULL, 0, format, arg);
	va_end(copy);
	if (len >= sizeof(loc_buf)) {
		temp = (char*)special_malloc(len + 1);
		if (temp == NULL) {
	  	return 0;
	  }
	}
	vsnprintf_P(temp, len + 1, format, arg);
	AddLog(LOG_LEVEL_DEBUG, PSTR("SML: %s"),temp);
	va_end(arg);
	if (len >= sizeof(loc_buf)) {
		free(temp);
	}
	return len;
}


#define han_debug HAN_print


Han_Parser *New_Han_Parser(uint16_t (dp)(uint8_t, uint8_t), uint8_t m, uint8_t *key, uint8_t *auth, uint16 *size) {
SETREGS
    // allocate all memory for parsers
    Han_Parser *hp = (Han_Parser*)calloc(sizeof(Han_Parser), 1);
    hp->dispatch = dp;
    hp->meter = m;
    memmove(hp->encryptionKey, key, 16);
    if (auth) {
      memmove(hp->authenticationKey, auth, 16);
    } else {
      memset(hp->authenticationKey, 0, 16);
    }

    New_GCMParser(hp, key, hp->authenticationKey);

    *size = sizeof(Han_Parser);

    hp->Debug = true;

    return hp;
}

void Delete_Han_Parser(Han_Parser *hp) {
SETREGS

    if (hp->mbus.buf) free(hp->mbus.buf);
    if (hp->gbt.buf) free(hp->gbt.buf);

    free(hp);
}

int Han_Parser_serial_available(Han_Parser *hp) {
SETREGS
  return hp->dispatch(hp->meter, 0);
}

int Han_Parser_serial_read(Han_Parser *hp) {
SETREGS
  return hp->dispatch(hp->meter, 1);
}

int16_t Han_Parser_serial_readBytes(Han_Parser *hp, uint8_t *buf, uint16_t size) {
SETREGS
  if (size > Han_Parser_serial_available(hp)) {
    size = Han_Parser_serial_available(hp);
  }
  for (uint16_t cnt = 0; cnt < size; cnt++) {
    buf[cnt] = Han_Parser_serial_read(hp);
  }
  return size;
}

bool Han_Parser_readHanPort(Han_Parser *hp, uint8_t **out, uint16_t *size, uint8_t flags) {
SETREGS
	if (!Han_Parser_serial_available(hp)) return false;

	// Before reading, empty serial buffer to increase chance of getting first byte of a data transfer
	if (!hp->serialInit) {
		Han_Parser_serial_readBytes(hp, hp->hanBuffer, BUF_SIZE_HAN);
		hp->serialInit = true;
		return false;
	}

	DataParserContext ctx;
    memset(&ctx, 0, sizeof(DataParserContext));

	ctx.flags = flags;
	int pos = DATA_PARSE_INCOMPLETE;
	// For each byte received, check if we have a complete frame we can handle
	while (Han_Parser_serial_available(hp) && pos == DATA_PARSE_INCOMPLETE) {
        yield();
    // If buffer was overflowed, reset
		if (hp->len >= BUF_SIZE_HAN) {
			Han_Parser_serial_readBytes(hp, hp->hanBuffer, BUF_SIZE_HAN);
			hp->len = 0;
			han_debug(PSTR("Buffer overflow, resetting"));
			return false;
		}
		hp->hanBuffer[hp->len++] = Han_Parser_serial_read(hp);
		ctx.length = hp->len;
		pos = Han_Parser_unwrapData(hp, (uint8_t *) hp->hanBuffer, ctx);
		if (ctx.type > 0 && pos >= 0) {
			if(ctx.type == DATA_TAG_DLMS) {
				han_debug(PSTR("Received valid DLMS at %d"), pos);
			} else if(ctx.type == DATA_TAG_DSMR) {
				han_debug(PSTR("Received valid DSMR at %d"), pos);
			} else {
				// TODO: Move this so that payload is sent to MQTT
				han_debug(PSTR("Unknown tag %02X at pos %d"), ctx.type, pos);
				hp->len = 0;
				return false;
			}
		}
	}
	if (pos == DATA_PARSE_INCOMPLETE) {
		return false;
	} else if (pos == DATA_PARSE_UNKNOWN_DATA) {
		han_debug(PSTR("Unknown data payload:"));
		hp->len = hp->len + Han_Parser_serial_readBytes(hp, hp->hanBuffer + hp->len, BUF_SIZE_HAN - hp->len);
		//debugPrint(hp->hanBuffer, 0, hp->len);
		hp->len = 0;
		return false;
	}

	if (pos == DATA_PARSE_INTERMEDIATE_SEGMENT) {
		hp->len = 0;
		return false;
	} else if (pos < 0) {
		Han_Parser_printHanReadError(hp, pos);
		hp->len += Han_Parser_serial_readBytes(hp, hp->hanBuffer + hp->len, BUF_SIZE_HAN - hp->len);
		while (Han_Parser_serial_available(hp)) Han_Parser_serial_read(hp); // Make sure it is all empty, in case we overflowed buffer above
		hp->len = 0;
		return false;
	}

	// Data is valid, clear the rest of the buffer to avoid tainted parsing
	for (int i = pos + ctx.length; i < BUF_SIZE_HAN; i++) {
		hp->hanBuffer[i] = 0x00;
	}

	//AmsData data;
	char* payload = ((char *) (hp->hanBuffer)) + pos;
	if (ctx.type == DATA_TAG_DLMS) {
		han_debug(PSTR("Using application data:"));

		//if (Debug.isActive(RemoteDebug::VERBOSE)) debugPrint((byte*) payload, 0, ctx.length);

		// Rudimentary detector for L&G proprietary format
		if (payload[0] == CosemTypeStructure && payload[2] == CosemTypeArray && payload[1] == payload[3]) {
			//data = LNG(payload, meterState.getMeterType(), &meterConfig, ctx, &Debug);
		} else {
			// TODO: Split IEC6205675 into DataParserKaifa and DataParserObis. This way we can add other means of parsing, for those other proprietary formats
			//data = IEC6205675(payload, meterState.getMeterType(), &meterConfig, ctx);
		}
	} else if(ctx.type == DATA_TAG_DSMR) {
		//data = IEC6205621(payload);
	}

	*out = hp->hanBuffer + pos;
	*size = ctx.length;
	hp->len = 0;
	return true;
}


int16_t Han_Parser_unwrapData(Han_Parser *hp, uint8_t *buf, DataParserContext &context) {
SETREGS

	int16_t ret = 0;
	bool doRet = false;
	uint16_t end = BUF_SIZE_HAN;
	uint8_t tag = (*buf);
	uint8_t lastTag = DATA_TAG_NONE;
	while (tag != DATA_TAG_NONE) {
		int16_t curLen = context.length;
		int8_t res = 0;
#if 1
		switch(tag) {
			case DATA_TAG_HDLC:
				res = HDLCParser_parse(hp, buf, context);
				break;
			case DATA_TAG_MBUS:
				res = MBUSParser_parse(hp, buf, context);
				break;
			case DATA_TAG_GBT:
				res = GBTParser_parse(hp, buf, context);
				break;
			case DATA_TAG_GCM:
				res = GCMParser_parse(hp, buf, context);
				break;
			case DATA_TAG_LLC:
				res = LLCParser_parse(hp, buf, context);
				break;
			case DATA_TAG_DLMS:
				res = DLMSParser_parse(hp, buf, context);
				if (res >= 0) doRet = true;
				break;
			case DATA_TAG_DSMR:
				res = DSMRParser_parse(hp, buf, context, lastTag != DATA_TAG_NONE);
				if (res >= 0) doRet = true;
				break;
			default:
				han_debug(PSTR("Ended up in default case while unwrapping...(tag %02X)"), tag);
				return DATA_PARSE_UNKNOWN_DATA;
		}
#else
		if (DATA_TAG_HDLC == tag) {
			res = HDLCParser_parse(hp, buf, context);
        } else if (DATA_TAG_MBUS == tag) {
			res = MBUSParser_parse(hp, buf, context);
        } else if (DATA_TAG_GBT == tag) {
			res = GBTParser_parse(hp, buf, context);
        } else if (DATA_TAG_GCM == tag) {
			res = GCMParser_parse(hp, buf, context);
        } else if (DATA_TAG_LLC == tag) {
			res = LLCParser_parse(hp, buf, context);
        } else if (DATA_TAG_DLMS == tag) {
			res = DLMSParser_parse(hp, buf, context);
			if (res >= 0) doRet = true;
        } else if (DATA_TAG_DSMR == tag) {
			res = DSMRParser_parse(hp, buf, context, lastTag != DATA_TAG_NONE);
			if (res >= 0) doRet = true;
        } else {
			han_debug(PSTR("Ended up in default case while unwrapping...(tag %02X)"), tag);
			return DATA_PARSE_UNKNOWN_DATA;
		}
#endif
		lastTag = tag;
		if (res == DATA_PARSE_INCOMPLETE) {
			return res;
		}
		if (context.length > end) return false;
		if (hp->Debug) {
#if 1
			switch(tag) {
				case DATA_TAG_HDLC:
					han_debug(PSTR("HDLC frame:"));
					break;
				case DATA_TAG_MBUS:
					han_debug(PSTR("MBUS frame:"));
					break;
				case DATA_TAG_GBT:
					han_debug(PSTR("GBT frame:"));
					break;
				case DATA_TAG_GCM:
					han_debug(PSTR("GCM frame:"));
					break;
				case DATA_TAG_LLC:
					han_debug(PSTR("LLC frame:"));
					break;
				case DATA_TAG_DLMS:
					han_debug(PSTR("DLMS frame:"));
					break;
				case DATA_TAG_DSMR:
					han_debug(PSTR("DSMR frame:"));
					break;
			}
#else
			if (DATA_TAG_HDLC == tag) {
				han_debug(PSTR("HDLC frame:"));
            }
			if (DATA_TAG_MBUS == tag) {
				han_debug(PSTR("MBUS frame:"));
            }
			if (DATA_TAG_GBT == tag) {
				han_debug(PSTR("GBT frame:"));
            }
			if (DATA_TAG_GCM == tag) {
				han_debug(PSTR("GCM frame:"));
            }
			if (DATA_TAG_LLC == tag) {
				han_debug(PSTR("LLC frame:"));
            }
			if (DATA_TAG_DLMS == tag) {
				han_debug(PSTR("DLMS frame:"));
            }
			if (DATA_TAG_DSMR == tag) {
				han_debug(PSTR("DSMR frame:"));
				break;
			}
#endif
		}
		if (res == DATA_PARSE_FINAL_SEGMENT) {
			if (tag == DATA_TAG_MBUS) {
				res = MBUSParser_write(hp, buf, context);
			}
		}

		if (res < 0) {
			return res;
		}
		buf += res;
		end -= res;
		ret += res;

		// If we are ready to return, do that
		if (doRet) {
			context.type = tag;
			return ret;
		}

		// Use start byte of new buffer position as tag for next round in loop
		tag = (*buf);
	}
	han_debug(PSTR("Got to end of unwrap method..."));
	return DATA_PARSE_UNKNOWN_DATA;
}

void Han_Parser_printHanReadError(Han_Parser *hp, int16_t pos) {
SETREGS
		switch(pos) {
			case DATA_PARSE_BOUNDRY_FLAG_MISSING:
				han_debug(PSTR("Boundry flag missing"));
				break;
			case DATA_PARSE_HEADER_CHECKSUM_ERROR:
				han_debug(PSTR("Header checksum error"));
				break;
			case DATA_PARSE_FOOTER_CHECKSUM_ERROR:
				han_debug(PSTR("Frame checksum error"));
				break;
			case DATA_PARSE_INCOMPLETE:
				han_debug(PSTR("Received frame is incomplete"));
				break;
			case GCM_AUTH_FAILED:
				han_debug(PSTR("Decrypt authentication failed"));
				break;
			case GCM_ENCRYPTION_KEY_FAILED:
				han_debug(PSTR("Setting decryption key failed"));
				break;
			case GCM_DECRYPT_FAILED:
				han_debug(PSTR("Decryption failed"));
				break;
			case MBUS_FRAME_LENGTH_NOT_EQUAL:
				han_debug(PSTR("Frame length mismatch"));
				break;
			case DATA_PARSE_INTERMEDIATE_SEGMENT:
				han_debug(PSTR("Intermediate segment received"));
				break;
			case DATA_PARSE_UNKNOWN_DATA:
				han_debug(PSTR("Unknown data format %02X"), hp->hanBuffer[0]);
				break;
			default:
				han_debug(PSTR("Unspecified error while reading data: %d"), pos);
				break;
		}
}



