#ifndef _SVS_Device_State_h

#define _SVS_Device_State_h



#ifndef DEV_STATUS_LINKED

#define DEV_STATUS_ACTIVE             0  //设备默认为激活状态

#define DEV_STATUS_LINKED             1 //设备已注册上来

#define DEV_STATUS_UNLINKED           2 //设备未注册上来

//#define DEV_STATUS_DEL                 3 //设备删除

#define DEV_STATUS_PAUSE               3 //设备停用

#define DEV_STATUS_FIRST_REG           9 //设备首次记载，但未注册上来

#endif

#endif //_SVS_Device_State_h
