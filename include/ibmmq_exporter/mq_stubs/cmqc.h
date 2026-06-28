/* Stub IBM MQ C header for building without IBM MQ client installed.
   Provides type definitions and constants needed for compilation. */
#ifndef CMQC_H_STUB
#define CMQC_H_STUB

#include <cstdint>

/* Basic MQ types */
typedef int32_t  MQLONG;
typedef uint32_t MQULONG;
typedef int64_t  MQINT64;
typedef char     MQCHAR;
typedef MQCHAR   MQCHAR4[4];
typedef MQCHAR   MQCHAR8[8];
typedef MQCHAR   MQCHAR12[12];
typedef MQCHAR   MQCHAR20[20];
typedef MQCHAR   MQCHAR28[28];
typedef MQCHAR   MQCHAR32[32];
typedef MQCHAR   MQCHAR48[48];
typedef MQCHAR   MQCHAR64[64];
typedef MQCHAR   MQCHAR128[128];
typedef MQCHAR   MQCHAR256[256];
typedef MQCHAR   MQCHAR264[264];
typedef MQLONG   MQHCONN;
typedef MQLONG   MQHOBJ;
typedef MQLONG   MQHSUB;  /* Convenience alias; real MQ uses MQHOBJ for subscriptions */
typedef uint8_t  MQBYTE;
typedef MQBYTE   MQBYTE24[24];
typedef MQBYTE   MQBYTE32[32];
typedef void*    PMQVOID;
typedef MQLONG*  PMQLONG;
typedef MQCHAR*  PMQCHAR;

/* Completion codes */
#define MQCC_OK       0
#define MQCC_WARNING  1
#define MQCC_FAILED   2

/* Reason codes */
#define MQRC_NONE              0
#define MQRC_NO_MSG_AVAILABLE  2033
#define MQRC_Q_MGR_QUIESCING   2161
#define MQRC_CONNECTION_BROKEN 2009
#define MQRC_SUB_ALREADY_EXISTS 2432
#define MQRC_UNKNOWN_OBJECT_NAME 2067

/* Object types */
#define MQOT_Q        1
#define MQOT_Q_MGR    5
#define MQOT_TOPIC    19

/* Open options */
#define MQOO_INPUT_AS_Q_DEF    0x00000001
#define MQOO_INPUT_SHARED      0x00000002
#define MQOO_INPUT_EXCLUSIVE   0x00000004
#define MQOO_OUTPUT            0x00000010
#define MQOO_INQUIRE           0x00000020
#define MQOO_FAIL_IF_QUIESCING 0x00002000

/* Get message options */
#define MQGMO_NONE             0x00000000
#define MQGMO_WAIT             0x00000001
#define MQGMO_NO_WAIT          0x00000000
#define MQGMO_CONVERT          0x00004000
#define MQGMO_FAIL_IF_QUIESCING 0x00002000
#define MQGMO_VERSION_1        1

/* Put message options */
#define MQPMO_NONE             0x00000000
#define MQPMO_VERSION_1        1

/* Message types */
#define MQMT_REQUEST   1
#define MQMT_REPLY     2
#define MQMT_DATAGRAM  8
#define MQMT_REPORT    4

/* Format */
#define MQFMT_ADMIN    "MQADMIN "
#define MQFMT_NONE     "        "
#define MQFMT_PCF      "MQPCF   "

/* Connection options */
#define MQCNO_VERSION_1        1
#define MQCNO_VERSION_2        2
#define MQCNO_VERSION_5        5
#define MQCNO_CLIENT_BINDING   0x00000002
#define MQCNO_LOCAL_BINDING    0x00000004
#define MQCNO_STANDARD_BINDING 0x00000000

/* CSP auth types */
#define MQCSP_AUTH_NONE            0
#define MQCSP_AUTH_USER_ID_AND_PWD 1

/* Inquiry selectors */
#define MQIA_CURRENT_Q_DEPTH   3
#define MQIA_MAX_Q_DEPTH       15
#define MQIA_OPEN_INPUT_COUNT  17
#define MQIA_OPEN_OUTPUT_COUNT 18
#define MQIA_PLATFORM          32
#define MQIA_HIGH_Q_DEPTH      36
#define MQIA_MSG_ENQ_COUNT     37
#define MQIA_MSG_DEQ_COUNT     38

/* Platform values */
#define MQPL_OS400       3
#define MQPL_WINDOWS_NT  11
#define MQPL_UNIX        13
#define MQPL_ZOS         18
#define MQPL_NSK         27
#define MQPL_APPLIANCE   28

/* Close options */
#define MQCO_IMMEDIATE     0x00000000
#define MQCO_NONE          0x00000000
#define MQCO_DELETE        0x00000001
#define MQCO_DELETE_PURGE  0x00000002
#define MQCO_KEEP_SUB      0x00000004
#define MQCO_REMOVE_SUB    0x00000008
#define MQCO_QUIESCE       0x00000020

/* Subscription options */
#define MQSO_CREATE              0x00000002
#define MQSO_NON_DURABLE         0x00000000
#define MQSO_MANAGED             0x00000020
#define MQSO_FAIL_IF_QUIESCING   0x00002000

/* ----- MQCHARV - Variable length string ----- */
typedef struct tagMQCHARV {
    void*   VSPtr;
    MQLONG  VSOffset;
    MQLONG  VSBufSize;
    MQLONG  VSLength;
    MQLONG  VSCCSID;
} MQCHARV;
#define MQCHARV_DEFAULT nullptr, 0, 0, 0, 0

/* ----- Structure definitions ----- */

/* MQOD - Object Descriptor */
#define MQOD_VERSION_1 1
#define MQOD_VERSION_4 4
typedef struct tagMQOD {
    MQCHAR4   StrucId;
    MQLONG    Version;
    MQLONG    ObjectType;
    MQCHAR48  ObjectName;
    MQCHAR48  ObjectQMgrName;
    MQCHAR48  DynamicQName;
    MQCHAR12  AlternateUserId;
} MQOD;
#define MQOD_DEFAULT {'O','D',' ',' '}, MQOD_VERSION_1, MQOT_Q, {""}, {""}, {"AMQ.*"}, {""}

/* MQMD - Message Descriptor */
#define MQMD_VERSION_1 1
#define MQMD_VERSION_2 2
typedef struct tagMQMD {
    MQCHAR4   StrucId;
    MQLONG    Version;
    MQLONG    Report;
    MQLONG    MsgType;
    MQLONG    Expiry;
    MQLONG    Feedback;
    MQLONG    Encoding;
    MQLONG    CodedCharSetId;
    MQCHAR8   Format;
    MQLONG    Priority;
    MQLONG    Persistence;
    MQBYTE24  MsgId;
    MQBYTE24  CorrelId;
    MQLONG    BackoutCount;
    MQCHAR48  ReplyToQ;
    MQCHAR48  ReplyToQMgr;
    MQCHAR12  UserIdentifier;
    MQBYTE32  AccountingToken;
    MQCHAR32  ApplIdentityData;
    MQLONG    PutApplType;
    MQCHAR28  PutApplName;
    MQCHAR8   PutDate;
    MQCHAR8   PutTime;
    MQCHAR4   ApplOriginData;
} MQMD;
#define MQMD_DEFAULT {'M','D',' ',' '}, MQMD_VERSION_1, 0, MQMT_DATAGRAM, -1, 0, 0, 0, {""}, 0, 0, {0}, {0}, 0, {""}, {""}, {""}, {0}, {""}, 0, {""}, {""}, {""}, {""}

/* MQGMO - Get Message Options */
typedef struct tagMQGMO {
    MQCHAR4   StrucId;
    MQLONG    Version;
    MQLONG    Options;
    MQLONG    WaitInterval;
    MQLONG    Signal1;
    MQLONG    Signal2;
    MQCHAR48  ResolvedQName;
} MQGMO;
#define MQGMO_DEFAULT {'G','M','O',' '}, MQGMO_VERSION_1, MQGMO_NO_WAIT, 0, 0, 0, {""}

/* MQPMO - Put Message Options */
typedef struct tagMQPMO {
    MQCHAR4   StrucId;
    MQLONG    Version;
    MQLONG    Options;
    MQLONG    Timeout;
    MQLONG    Context;
    MQLONG    KnownDestCount;
    MQLONG    UnknownDestCount;
    MQLONG    InvalidDestCount;
    MQCHAR48  ResolvedQName;
    MQCHAR48  ResolvedQMgrName;
} MQPMO;
#define MQPMO_DEFAULT {'P','M','O',' '}, MQPMO_VERSION_1, MQPMO_NONE, -1, 0, 0, 0, 0, {""}, {""}

/* MQCNO - Connect Options */
typedef struct tagMQCNO {
    MQCHAR4   StrucId;
    MQLONG    Version;
    MQLONG    Options;
    MQLONG    ClientConnOffset;
    void*     ClientConnPtr;
    void*     SecurityParmsPtr;
} MQCNO;
#define MQCNO_DEFAULT {'C','N','O',' '}, MQCNO_VERSION_2, MQCNO_CLIENT_BINDING, 0, nullptr, nullptr

/* MQCSP - Security Parameters */
typedef struct tagMQCSP {
    MQCHAR4   StrucId;
    MQLONG    Version;
    MQLONG    AuthenticationType;
    void*     CSPUserIdPtr;
    MQLONG    CSPUserIdOffset;
    MQLONG    CSPUserIdLength;
    void*     CSPPasswordPtr;
    MQLONG    CSPPasswordOffset;
    MQLONG    CSPPasswordLength;
} MQCSP;
#define MQCSP_DEFAULT {'C','S','P',' '}, 1, MQCSP_AUTH_NONE, nullptr, 0, 0, nullptr, 0, 0

/* MQSD - Subscription Descriptor */
#define MQSD_VERSION_1 1
typedef struct tagMQSD {
    MQCHAR4   StrucId;
    MQLONG    Version;
    MQLONG    Options;
    MQCHARV   ObjectString;
    MQCHARV   SubName;
    MQLONG    SubExpiry;
    MQCHAR48  ObjectName;
    MQLONG    SubLevel;
} MQSD;
#define MQSD_DEFAULT {'S','D',' ',' '}, MQSD_VERSION_1, 0, {MQCHARV_DEFAULT}, {MQCHARV_DEFAULT}, -1, {""}, 0

/* ----- Stub API functions ----- */
#ifdef IBMMQ_STUB_MODE

#include <cstring>

inline void MQCONNX(const char*, MQCNO*, MQHCONN* hconn, MQLONG* cc, MQLONG* rc) {
    *hconn = 1; *cc = MQCC_OK; *rc = MQRC_NONE;
}
inline void MQDISC(MQHCONN*, MQLONG* cc, MQLONG* rc) {
    *cc = MQCC_OK; *rc = MQRC_NONE;
}
inline void MQOPEN(MQHCONN, MQOD* od, MQLONG, MQHOBJ* hobj, MQLONG* cc, MQLONG* rc) {
    *hobj = 1; *cc = MQCC_OK; *rc = MQRC_NONE;
    /* Copy DynamicQName to ObjectName to simulate dynamic queue creation */
    if (od->DynamicQName[0] != '\0') {
        std::strncpy(od->ObjectName, "AMQ.STUB.REPLY.Q", sizeof(od->ObjectName) - 1);
    }
}
inline void MQCLOSE(MQHCONN, MQHOBJ*, MQLONG, MQLONG* cc, MQLONG* rc) {
    *cc = MQCC_OK; *rc = MQRC_NONE;
}
inline void MQGET(MQHCONN, MQHOBJ, MQMD*, MQGMO*, MQLONG, void*, MQLONG* datalen, MQLONG* cc, MQLONG* rc) {
    *datalen = 0; *cc = MQCC_FAILED; *rc = MQRC_NO_MSG_AVAILABLE;
}
inline void MQPUT(MQHCONN, MQHOBJ, MQMD*, MQPMO*, MQLONG, void*, MQLONG* cc, MQLONG* rc) {
    *cc = MQCC_OK; *rc = MQRC_NONE;
}
inline void MQINQ(MQHCONN, MQHOBJ, MQLONG /*selectorCount*/, MQLONG* selectors,
                  MQLONG intAttrCount, MQLONG* intAttrs,
                  MQLONG charAttrLength, char* charAttrs,
                  MQLONG* cc, MQLONG* rc) {
    (void)selectors; (void)charAttrLength; (void)charAttrs;
    for (MQLONG i = 0; i < intAttrCount; ++i) intAttrs[i] = 0;
    *cc = MQCC_OK; *rc = MQRC_NONE;
}
inline void MQSUB(MQHCONN, MQSD*, MQHOBJ* hobj, MQHOBJ* hsub, MQLONG* cc, MQLONG* rc) {
    *hobj = 2; *hsub = 1; *cc = MQCC_OK; *rc = MQRC_NONE;
}

#else
/* Real IBM MQ - link against mqm/mqic library */
#ifdef __cplusplus
extern "C" {
#endif
void MQCONNX(const char*, MQCNO*, MQHCONN*, MQLONG*, MQLONG*);
void MQDISC(MQHCONN*, MQLONG*, MQLONG*);
void MQOPEN(MQHCONN, MQOD*, MQLONG, MQHOBJ*, MQLONG*, MQLONG*);
void MQCLOSE(MQHCONN, MQHOBJ*, MQLONG, MQLONG*, MQLONG*);
void MQGET(MQHCONN, MQHOBJ, MQMD*, MQGMO*, MQLONG, void*, MQLONG*, MQLONG*, MQLONG*);
void MQPUT(MQHCONN, MQHOBJ, MQMD*, MQPMO*, MQLONG, void*, MQLONG*, MQLONG*);
void MQINQ(MQHCONN, MQHOBJ, MQLONG, MQLONG*, MQLONG, MQLONG*, MQLONG, char*, MQLONG*, MQLONG*);
void MQSUB(MQHCONN, MQSD*, MQHOBJ*, MQHOBJ*, MQLONG*, MQLONG*);
#ifdef __cplusplus
}
#endif
#endif /* IBMMQ_STUB_MODE */

#endif /* CMQC_H_STUB */
