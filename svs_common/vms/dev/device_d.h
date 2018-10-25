#ifndef __SVS_DEVICE_H__
#define __SVS_DEVICE_H__

enum SVS_DEV_TYPE
{
    SVS_DEV_TYPE_GB28181    = 1,
    SVS_DEV_TYPE_VMS        = 2,
    SVS_DEV_TYPE_EHOME      = 3, /* HIK EHOME */
    SVS_DEV_TYPE_MAX
};

enum SVS_DEV_STATUS
{
    SVS_DEV_STATUS_OFFLINE  = 0,
    SVS_DEV_STATUS_ONLINE   = 1,

    SVS_DEV_STATUS_MAX
};

#endif //__SVS_DEVICE_H__

