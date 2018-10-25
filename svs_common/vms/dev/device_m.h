#ifndef __SVS_DEVICE_MSG_H__
#define __SVS_DEVICE_MSG_H__
#include <vms/vms_mt.h>
#include <vms/vms_adt.h>
#include <vms/vms_df.h>
#include <vms/vms_cm.h>

#pragma pack(push, 1)
typedef struct _SVS_MSG_LEN_REG_INFO
{
    uint8_t                     LensID[SVS_DEVICEID_LEN];
    uint8_t                     Name[DEVICE_NAME_LEN];
    uint8_t                     Manufacturer[DEV_FAC_LEN];
}SVS_MSG_LEN_REG_INFO;

typedef struct _SVS_MSG_DEV_REG_REQ
{
    SVS_MSG_HEADER               MsgHeader;
    uint8_t                      DevID[DEVICE_ID_LEN];
    uint32_t                     LensCount;
    SVS_MSG_LEN_REG_INFO         ListList[1];
}SVS_MSG_DEV_REG_REQ;

typedef struct _SVS_MSG_DEV_REG_RESP
{
    SVS_MSG_HEADER               MsgHeader;
    uint8_t                      DevID[DEVICE_ID_LEN];
    uint32_t                     RespCode;
}SVS_MSG_DEV_REG_RESP;

#pragma pack(pop)
#endif //__SVS_DEVICE_MSG_H__

