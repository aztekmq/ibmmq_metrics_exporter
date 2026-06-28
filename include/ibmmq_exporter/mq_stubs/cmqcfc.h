/* Stub IBM MQ PCF header */
#ifndef CMQCFC_H_STUB
#define CMQCFC_H_STUB
#include "cmqc.h"

/* PCF structure types */
#define MQCFT_NONE               0
#define MQCFT_COMMAND            1
#define MQCFT_RESPONSE           2
#define MQCFT_INTEGER            3
#define MQCFT_STRING             4
#define MQCFT_INTEGER_LIST       5
#define MQCFT_STRING_LIST        6
#define MQCFT_GROUP              20
#define MQCFT_STATISTICS         21
#define MQCFT_ACCOUNTING         22
#define MQCFT_INTEGER64          23
#define MQCFT_INTEGER64_LIST     25
#define MQCFT_COMMAND_XR         16

/* PCF commands (values from real IBM MQ 9.x headers) */
#define MQCMD_INQUIRE_Q                 13
#define MQCMD_RESET_Q_STATS             17
#define MQCMD_INQUIRE_Q_STATUS          41
#define MQCMD_INQUIRE_CHANNEL_STATUS    42
#define MQCMD_INQUIRE_CLUSTER_Q_MGR     70
#define MQCMD_INQUIRE_USAGE             126
#define MQCMD_INQUIRE_Q_MGR_STATUS      161
#define MQCMD_STATISTICS_MQI            164
#define MQCMD_STATISTICS_Q              165
#define MQCMD_STATISTICS_CHANNEL        166
#define MQCMD_ACCOUNTING_MQI            167
#define MQCMD_ACCOUNTING_Q              168
#define MQCMD_INQUIRE_SUB_STATUS        182
#define MQCMD_INQUIRE_TOPIC_STATUS      183

/* PCF control */
#define MQCFC_LAST               1
#define MQCFC_NOT_LAST           0

/* Channel status parameter IDs (values from real IBM MQ 9.x headers) */
#define MQCACH_CHANNEL_NAME         3501
#define MQCACH_CONNECTION_NAME      3506
#define MQCACH_MCA_NAME             3507
#define MQCACH_SSL_CIPHER_SPEC      3544
#define MQCACH_MCA_JOB_NAME         3530
#define MQIACH_CHANNEL_STATUS       1527
#define MQIACH_CHANNEL_TYPE         1511
#define MQIACH_MSGS                 1534
#define MQIACH_BYTES_SENT           1535
#define MQIACH_BYTES_RECEIVED       1536
#define MQIACH_BATCHES              1537
#define MQIACH_CHANNEL_SUBSTATE     1609
#define MQIACH_CHANNEL_INSTANCE_TYPE 1523

/* Channel type values */
#define MQCHT_SENDER     1
#define MQCHT_SERVER     2
#define MQCHT_RECEIVER   3
#define MQCHT_REQUESTER  4
#define MQCHT_SVRCONN    7
#define MQCHT_CLNTCONN   6
#define MQCHT_CLUSSDR    8
#define MQCHT_CLUSRCVR   9
#define MQCHT_AMQP       14
#define MQCHT_MQTT       15

/* Channel status values */
#define MQCHS_INACTIVE   0
#define MQCHS_BINDING    1
#define MQCHS_STARTING   2
#define MQCHS_RUNNING    3
#define MQCHS_STOPPING   4
#define MQCHS_RETRYING   5
#define MQCHS_STOPPED    6
#define MQCHS_REQUESTING 7
#define MQCHS_PAUSED     8
#define MQCHS_DISCONNECTED 9
#define MQCHS_INITIALIZING 13
#define MQCHS_SWITCHING  26

/* Topic status parameter IDs */
#define MQCA_TOPIC_STRING       2094
#define MQCA_TOPIC_NAME         2092
#define MQIA_TOPIC_TYPE         208
#define MQIA_PUB_COUNT          215
#define MQIA_SUB_COUNT          204
#define MQIACF_TOPIC_STATUS_TYPE 1185

/* Topic status type values */
#define MQIACF_TOPIC_SUB        1
#define MQIACF_TOPIC_PUB        2
#define MQIACF_TOPIC_STATUS     0

/* Subscription status parameter IDs */
#define MQCACF_SUB_NAME         3152
#define MQBACF_SUB_ID           7016
#define MQIACF_SUB_TYPE         1289
#define MQIA_DURABLE_SUB        175
#define MQIACF_SUB_STATUS_TYPE  1186
#define MQCACF_DESTINATION      3154

/* QM status parameter IDs */
#define MQCA_Q_MGR_NAME         2015
#define MQCA_Q_MGR_DESC         2014
#define MQIACF_Q_MGR_STATUS     1149
#define MQIACF_CHINIT_STATUS    1232
#define MQIACF_CONNECTION_COUNT 1229
#define MQIACF_CMD_SERVER_STATUS 1233
#define MQCACF_Q_MGR_START_DATE 3175
#define MQCACF_Q_MGR_START_TIME 3176

/* Cluster QM parameter IDs */
#define MQCA_CLUSTER_NAME       2029
#define MQIA_QM_TYPE            125
#define MQIACF_CLUSTER_Q_MGR_STATUS 1127

/* z/OS usage parameter IDs */
#define MQIACF_USAGE_TYPE            1157
#define MQIACF_USAGE_BP              1
#define MQIACF_USAGE_PS              2
#define MQIACF_BUFFER_POOL_ID        1158
#define MQIA_PAGESET_ID              62
#define MQIACF_USAGE_TOTAL_PAGES     1159
#define MQIACF_USAGE_UNUSED_PAGES    1160
#define MQIACF_USAGE_PERSIST_PAGES   1161
#define MQIACF_USAGE_NONPERSIST_PAGES 1162
#define MQIACF_USAGE_RESTART_EXTENTS 1163
#define MQIACF_USAGE_EXPAND_COUNT    1164
#define MQIACF_USAGE_BUFFER_POOL     1170
#define MQIACF_USAGE_FREE_BUFF       1330
#define MQIACF_USAGE_TOTAL_BUFFERS   1166

/* Queue attributes for INQUIRE_Q */
#define MQCA_Q_NAME             2016
#define MQCA_Q_DESC             2017
#define MQIA_Q_TYPE             20
#define MQIA_CURRENT_Q_DEPTH    3
#define MQIA_MAX_Q_DEPTH        15

/* PCF Header - MQCFH */
typedef struct tagMQCFH {
    MQLONG Type;
    MQLONG StrucLength;
    MQLONG Version;
    MQLONG Command;
    MQLONG MsgSeqNumber;
    MQLONG Control;
    MQLONG CompCode;
    MQLONG Reason;
    MQLONG ParameterCount;
} MQCFH;

/* PCF String Parameter - MQCFST */
typedef struct tagMQCFST {
    MQLONG Type;
    MQLONG StrucLength;
    MQLONG Parameter;
    MQLONG CodedCharSetId;
    MQLONG StringLength;
    MQCHAR String[1]; /* variable length */
} MQCFST;

/* PCF Integer Parameter - MQCFIN */
typedef struct tagMQCFIN {
    MQLONG Type;
    MQLONG StrucLength;
    MQLONG Parameter;
    MQLONG Value;
} MQCFIN;

/* PCF Integer64 Parameter */
typedef struct tagMQCFIN64 {
    MQLONG  Type;
    MQLONG  StrucLength;
    MQLONG  Parameter;
    MQLONG  Reserved;
    MQINT64 Value;
} MQCFIN64;

#endif /* CMQCFC_H_STUB */
