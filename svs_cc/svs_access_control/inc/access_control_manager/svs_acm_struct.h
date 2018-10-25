#ifndef _SVS_ACM_STRUCT_H_
#define _SVS_ACM_STRUCT_H_

#include "vms/vms.h"
#include <vector>
#include <string>

namespace SVS_ACM
{

enum DEV_CTRL_TYPE
{
    DEV_CTRL_TYPE_STOP,             //云台停止，无参数。光圈和聚焦使用DEV_CTRL_TYPE_STOP_FI停止
    DEV_CTRL_TYPE_ZOOM_IN,          //云台放大，使用REQUEST_DEV_CTRL.nCtrlParam1参数，取值0-0x0F
    DEV_CTRL_TYPE_ZOOM_OUT,         //云台缩小，使用REQUEST_DEV_CTRL.nCtrlParam1参数，取值0-0x0F
    DEV_CTRL_TYPE_UP,               //云台向上，使用REQUEST_DEV_CTRL.nCtrlParam1参数
    DEV_CTRL_TYPE_DOWN,             //云台向下，使用REQUEST_DEV_CTRL.nCtrlParam1参数
    DEV_CTRL_TYPE_LEFT,             //云台向左，使用REQUEST_DEV_CTRL.nCtrlParam1参数
    DEV_CTRL_TYPE_RIGHT,            //云台向右，使用REQUEST_DEV_CTRL.nCtrlParam1参数
    DEV_CTRL_TYPE_LEFT_UP,          //云台向左上，使用REQUEST_DEV_CTRL.nCtrlParam1参数
    DEV_CTRL_TYPE_LEFT_DOWN,        //云台向左下，使用REQUEST_DEV_CTRL.nCtrlParam1参数
    DEV_CTRL_TYPE_RIGHT_UP,         //云台向右上，使用REQUEST_DEV_CTRL.nCtrlParam1参数
    DEV_CTRL_TYPE_RIGHT_DOWN,       //云台向右下，使用REQUEST_DEV_CTRL.nCtrlParam1参数
    DEV_CTRL_TYPE_APERTURE_OPEN,    //光圈放大，使用REQUEST_DEV_CTRL.nCtrlParam1参数
    DEV_CTRL_TYPE_APERTURE_CLOSE,   //光圈缩小，使用REQUEST_DEV_CTRL.nCtrlParam1参数
    DEV_CTRL_TYPE_FOCAL_NEAR,       //聚焦近，使用REQUEST_DEV_CTRL.nCtrlParam1参数
    DEV_CTRL_TYPE_FOCAL_FAR,        //聚焦远，使用REQUEST_DEV_CTRL.nCtrlParam1参数
    DEV_CTRL_TYPE_STOP_FI,          //停止光圈或聚焦，无参数
    DEV_CTRL_TYPE_PREFAB_BIT_SET,   //预置位设置，使用REQUEST_DEV_CTRL.nCtrlParam1参数，不能为0
    DEV_CTRL_TYPE_PREFAB_BIT_DEL,   //预置位删除，使用REQUEST_DEV_CTRL.nCtrlParam1参数，不能为0
    DEV_CTRL_TYPE_PREFAB_BIT_RUN,   //预置位运行，使用REQUEST_DEV_CTRL.nCtrlParam1参数，不能为0
    DEV_CTRL_TYPE_MAX
};

//镜头信息
struct LENS_INFO
{
    SVS_DEV_TYPE    eLensType;
    SVS_DEV_STATUS  eLensStatus;
    char szLensID[SVS_DEVICEID_LEN + 1];    //镜头ID
    char szDeviceID[SVS_DEVICEID_LEN + 1];  //主设备ID
    char szLensName[DEVICE_NAME_LEN + 1];
    char szManufacturer[DEV_FAC_LEN + 1];

    LENS_INFO()
    {
        eLensType           = SVS_DEV_TYPE_MAX;
        eLensStatus         = SVS_DEV_STATUS_MAX;
        memset(szLensID, 0, sizeof(szLensID));
        memset(szDeviceID, 0, sizeof(szDeviceID));
        memset(szLensName, 0, sizeof(szLensName));
        memset(szManufacturer, 0, sizeof(szManufacturer));
    }
};

//设备信息
struct DEVICE_INFO
{
    SVS_DEV_TYPE    eDeviceType;
    SVS_DEV_STATUS  eDeviceStatus;
    char szDeviceID[SVS_DEVICEID_LEN + 1];
    char szHost[SVS_IP_LEN + 1];
    char szPort[SVS_PORT_LEN + 1];
    std::vector<LENS_INFO> vecLensInfo;

    DEVICE_INFO()
    {
        eDeviceType     = SVS_DEV_TYPE_MAX;
        eDeviceStatus   = SVS_DEV_STATUS_MAX;
        memset(szDeviceID, 0, sizeof(szDeviceID));
        memset(szHost, 0, sizeof(szHost));
        memset(szPort, 0, sizeof(szPort));
    }
};

struct DEV_ALARM_INFO
{
    char szDeviceID[SVS_DEVICEID_LEN + 1];
    std::string strAlarmPriority;
    std::string strAlarmMethod;
    std::string strAlarmTime;
    std::string strAlarmDescription;
    std::string strLongitude;
    std::string strLatitude;

    DEV_ALARM_INFO()
    {
        memset(szDeviceID, 0, sizeof(szDeviceID));
    }
};


}   //namespace SVS_ACM

#endif
