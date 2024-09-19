// AMS lib defines


MODULE_PART uint16_t _ntohs(uint16_t v);
MODULE_PART uint32_t _ntohl(uint32_t v);
MODULE_PART uint64_t _ntohll(uint64_t v);


// COSEM
// Blue book, Table 2
enum CosemType {
    CosemTypeNull = 0x00,
    CosemTypeArray = 0x01,
    CosemTypeStructure = 0x02,
    CosemTypeOctetString = 0x09,
    CosemTypeString = 0x0A,
    CosemTypeDLongSigned = 0x05,
    CosemTypeDLongUnsigned = 0x06,
    CosemTypeLongSigned = 0x10,
    CosemTypeLongUnsigned = 0x12,
    CosemTypeLong64Signed = 0x14,
    CosemTypeLong64Unsigned = 0x15,
    CosemTypeDateTime = 0x19
};

struct CosemBasic {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct CosemString {
    uint8_t type;
    uint8_t length;
    uint8_t data[];
} __attribute__((packed));

struct CosemLongSigned {
    uint8_t type;
	int16_t data;
} __attribute__((packed));

struct CosemLongUnsigned {
    uint8_t type;
    uint16_t data;
} __attribute__((packed));

struct CosemDLongSigned {
    uint8_t type;
    int32_t data;
} __attribute__((packed));

struct CosemDLongUnsigned {
    uint8_t type;
    uint32_t data;
} __attribute__((packed));

struct CosemLong64Signed {
    uint8_t type;
    int64_t data;
} __attribute__((packed));

struct CosemLong64Unsigned {
    uint8_t type;
    uint64_t data;
} __attribute__((packed));

struct CosemDateTime {
    uint8_t type;
    uint16_t year;
    uint8_t month;
    uint8_t dayOfMonth;
    uint8_t dayOfWeek;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t hundredths;
    int16_t deviation;
    uint8_t status;
} __attribute__((packed));

typedef union {
    struct CosemBasic base;
	struct CosemString str;
	struct CosemString oct;
    struct CosemLongSigned ls;
	struct CosemLongUnsigned lu;
    struct CosemDLongSigned dls;
    struct CosemDLongUnsigned dlu;
    struct CosemLong64Signed l64s;
    struct CosemLong64Unsigned l64u;
    struct CosemDateTime dt;
} CosemData;


MODULE_PART time_t decodeCosemDateTime(CosemDateTime timestamp);

// CRC
MODULE_PART uint16_t AMS_crc16(const uint8_t* p, int len);
MODULE_PART uint16_t AMS_crc16_x25(const uint8_t* p, int len);

// DATAPARSER
#define DATA_TAG_NONE 0x00
#define DATA_TAG_AUTO 0x01
#define DATA_TAG_HDLC 0x7E
#define DATA_TAG_LLC 0xE6
#define DATA_TAG_DLMS 0x0F
#define DATA_TAG_DSMR 0x2F
#define DATA_TAG_MBUS 0x68
#define DATA_TAG_GBT 0xE0
#define DATA_TAG_GCM 0xDB

#define DATA_PARSE_OK 0
#define DATA_PARSE_FAIL -1
#define DATA_PARSE_INCOMPLETE -2
#define DATA_PARSE_BOUNDRY_FLAG_MISSING -3
#define DATA_PARSE_HEADER_CHECKSUM_ERROR -4
#define DATA_PARSE_FOOTER_CHECKSUM_ERROR -5
#define DATA_PARSE_INTERMEDIATE_SEGMENT -6
#define DATA_PARSE_FINAL_SEGMENT -7
#define DATA_PARSE_UNKNOWN_DATA -9

struct DataParserContext {
    uint8_t type;
    uint8_t flags;
    uint16_t length;
    time_t timestamp;
    uint8_t system_title[8];
};

// DIMS
MODULE_PART int8_t DLMSParser_parse(uint8_t *buf, DataParserContext &ctx);

// DSMR
MODULE_PART int8_t DSMRParser_parse(uint8_t *buf, DataParserContext &ctx, bool verified);

// GBT
#define GBT_TAG 0xE0

typedef struct GBTHeader {
	uint8_t flag;
	uint8_t control;
	uint16_t sequence;
	uint16_t sequenceAck;
    uint8_t size;
} __attribute__((packed)) GBTHeader;

MODULE_PART int8_t GBTParser_parse(Han_Parser *hp, uint8_t *buf, DataParserContext &ctx);

typedef struct {
uint8_t lastSequenceNumber;
uint16_t pos;
uint8_t *buf;
} GBT_VARS;


// GCM
#define GCM_TAG 0xDB
#define GCM_AUTH_FAILED -51
#define GCM_DECRYPT_FAILED -52
#define GCM_ENCRYPTION_KEY_FAILED -53

typedef struct GCMSizeDef {
	uint8_t  flag;
	uint16_t format;
} __attribute__((packed)) GCMSizeDef;


//class GCMParser {
//public:
    //GCMParser(uint8_t *encryption_key, uint8_t *authentication_key);
    
MODULE_PART int8_t GCMParser_parse(Han_Parser *hp, uint8_t *buf, DataParserContext &ctx);

typedef struct {
uint8_t encryption_key[16];
uint8_t authentication_key[16];
uint8_t use_auth;
} GCM_VARS;

// HDLC
#define HDLC_FLAG 0x7E

typedef struct HDLCHeader {
	uint8_t  flag;
	uint16_t format;
} __attribute__((packed)) HDLCHeader;

typedef struct HDLCFooter {
	uint16_t fcs;
	uint8_t flag;
} __attribute__((packed)) HDLCFooter;

typedef struct HDLC3CtrlHcs {
    uint8_t control;
    uint16_t hcs;
} __attribute__((packed)) HDLC3CtrlHcs;

MODULE_PART int8_t HDLCParser_parse(uint8_t *buf, DataParserContext &ctx);

// HEXUTILS
MODULE_PART String AMS_toHex(uint8_t* in);
MODULE_PART String AMS_toHex(uint8_t* in, uint16_t size);
MODULE_PART void AMS_fromHex(uint8_t *out, String in, uint16_t size);

// LLC
typedef struct LLCHeader {
    uint8_t dst;
    uint8_t src;
    uint8_t control;
} __attribute__((packed)) LLCHeader;

MODULE_PART int8_t LLCParser_parse(uint8_t *buf, DataParserContext &ctx);


// MBUS

#define MBUS_START 0x68
#define MBUS_END 0x16
#define MBUS_FRAME_LENGTH_NOT_EQUAL -41

typedef struct MbusHeader {
	uint8_t flag1;
	uint8_t len1;
	uint8_t len2;
	uint8_t flag2;
} __attribute__((packed)) MbusHeader;

typedef struct MbusFooter {
	uint8_t fcs;
	uint8_t flag;
} __attribute__((packed)) MbusFooter;


MODULE_PART int8_t MBUSParser_parse(Han_Parser *hp, uint8_t *buf, DataParserContext &ctx);
MODULE_PART uint16_t MBUSParser_write(Han_Parser *hp, const uint8_t* d, DataParserContext &ctx);
MODULE_PART uint8_t checksum(const uint8_t* p, int len);

typedef struct {
uint8_t lastSequenceNumber = 0;
uint16_t pos = 0;
uint8_t *buf  = NULL;
} MBUS_VARS;


// NTOHL
uint64_t ntohll(uint64_t x);


// HAN
#define BUF_SIZE_HAN (1280)

int16_t serial_available(void);
uint8_t serial_read(void);


//Han_Parser(uint16_t (*)(uint8_t, uint8_t), uint8_t, uint8_t *, uint8_t *);
MODULE_PART Han_Parser *New_Han_Parser(uint16_t (dp)(uint8_t, uint8_t), uint8_t m, uint8_t *key, uint8_t *auth);
MODULE_PART bool Han_Parser_readHanPort(Han_Parser *hp, uint8_t **out, uint16_t *size, uint8_t flags);
MODULE_PART int16_t Han_Parser_unwrapData(Han_Parser *hp, uint8_t *buf, DataParserContext &context);
MODULE_PART void Han_Parser_printHanReadError(Han_Parser *hp, int16_t pos);
MODULE_PART int Han_Parser_serial_available(Han_Parser *hp);
MODULE_PART int Han_Parser_serial_read(Han_Parser *hp);
MODULE_PART int16_t Han_Parser_serial_readBytes(Han_Parser *hp, uint8_t *, uint16_t);

typedef struct {
uint8_t encryptionKey[16];
uint8_t authenticationKey[16];
uint8_t hanBuffer[BUF_SIZE_HAN];
int len;
uint16_t (*dispatch)(uint8_t, uint8_t);

//HDLCParser *hdlcParser = NULL;
//MBUSParser *mbusParser = NULL;
//GBTParser *gbtParser = NULL;
//GCMParser *gcmParser = NULL;
//LLCParser *llcParser = NULL;
//DLMSParser *dlmsParser = NULL;
//DSMRParser *dsmrParser = NULL;

uint8_t encryption_key[16];
uint8_t authentication_key[16];
uint8_t meter;
bool serialInit = true;
bool Debug = true;

GBT_VARS gbt;
GCM_VARS gcm;
MBUS_VARS mbus;

} Han_Parser;


