#ifndef _SVS_ACM_STRUCT_H_
#define _SVS_ACM_STRUCT_H_

#include "vms/vms.h"
#include <vector>
#include <string>

namespace SVS_ACM
{

enum DEV_CTRL_TYPE
{
    DEV_CTRL_TYPE_STOP,             //��ֹ̨ͣ���޲�������Ȧ�;۽�ʹ��DEV_CTRL_TYPE_STOP_FIֹͣ
    DEV_CTRL_TYPE_ZOOM_IN,          //��̨�Ŵ�ʹ��REQUEST_DEV_CTRL.nCtrlParam1������ȡֵ0-0x0F
    DEV_CTRL_TYPE_ZOOM_OUT,         //��̨��С��ʹ��REQUEST_DEV_CTRL.nCtrlParam1������ȡֵ0-0x0F
    DEV_CTRL_TYPE_UP,               //��̨���ϣ�ʹ��REQUEST_DEV_CTRL.nCtrlParam1����
    DEV_CTRL_TYPE_DOWN,             //��̨���£�ʹ��REQUEST_DEV_CTRL.nCtrlParam1����
    DEV_CTRL_TYPE_LEFT,             //��̨����ʹ��REQUEST_DEV_CTRL.nCtrlParam1����
    DEV_CTRL_TYPE_RIGHT,            //��̨���ң�ʹ��REQUEST_DEV_CTRL.nCtrlParam1����
    DEV_CTRL_TYPE_LEFT_UP,          //��̨�����ϣ�ʹ��REQUEST_DEV_CTRL.nCtrlParam1����
    DEV_CTRL_TYPE_LEFT_DOWN,        //��̨�����£�ʹ��REQUEST_DEV_CTRL.nCtrlParam1����
    DEV_CTRL_TYPE_RIGHT_UP,         //��̨�����ϣ�ʹ��REQUEST_DEV_CTRL.nCtrlParam1����
    DEV_CTRL_TYPE_RIGHT_DOWN,       //��̨�����£�ʹ��REQUEST_DEV_CTRL.nCtrlParam1����
    DEV_CTRL_TYPE_APERTURE_OPEN,    //��Ȧ�Ŵ�ʹ��REQUEST_DEV_CTRL.nCtrlParam1����
    DEV_CTRL_TYPE_APERTURE_CLOSE,   //��Ȧ��С��ʹ��REQUEST_DEV_CTRL.nCtrlParam1����
    DEV_CTRL_TYPE_FOCAL_NEAR,       //�۽�����ʹ��REQUEST_DEV_CTRL.nCtrlParam1����
    DEV_CTRL_TYPE_FOCAL_FAR,        //�۽�Զ��ʹ��REQUEST_DEV_CTRL.nCtrlParam1����
    DEV_CTRL_TYPE_STOP_FI,          //ֹͣ��Ȧ��۽����޲���
    DEV_CTRL_TYPE_PREFAB_BIT_SET,   //Ԥ��λ���ã�ʹ��REQUEST_DEV_CTRL.nCtrlParam1����������Ϊ0
    DEV_CTRL_TYPE_PREFAB_BIT_DEL,   //Ԥ��λɾ����ʹ��REQUEST_DEV_CTRL.nCtrlParam1����������Ϊ0
    DEV_CTRL_TYPE_PREFAB_BIT_RUN,   //Ԥ��λ���У�ʹ��REQUEST_DEV_CTRL.nCtrlParam1����������Ϊ0
    DEV_CTRL_TYPE_MAX
};

//��ͷ��Ϣ
struct LENS_INFO
{
    SVS_DEV_TYPE    eLensType;
    SVS_DEV_STATUS  eLensStatus;
    char szLensID[SVS_DEVICEID_LEN + 1];    //��ͷID
    char szDeviceID[SVS_DEVICEID_LEN + 1];  //���豸ID
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

//�豸��Ϣ
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
