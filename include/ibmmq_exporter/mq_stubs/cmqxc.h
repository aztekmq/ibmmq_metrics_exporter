/* Stub IBM MQ extended header - channel definitions */
#ifndef CMQXC_H_STUB
#define CMQXC_H_STUB
#include "cmqc.h"

/* MQCD - Channel Definition (simplified stub) */
typedef struct tagMQCD {
    MQCHAR20  ChannelName;
    MQCHAR264 ConnectionName;
    MQCHAR32  SSLCipherSpec;
} MQCD;
#define MQCD_CLIENT_CONN_DEFAULT {""}, {""}, {""}

#endif /* CMQXC_H_STUB */
