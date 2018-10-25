/**
* @file    SvsRetCode.h
* @brief   系统返回码定义
*
* Copyright (c) 2010 AllCam Tech.Co.,Ltd
*
* @date      2010-12-10
*/

#ifndef _SVSRETCODE_H__
#define _SVSRETCODE_H__

// SVS系统返回码，采用int类型
typedef enum _enSvsRetCode
{
    RET_OK                  = 0x00000000,                          /// 操作成功返回码
    RET_FAIL                = 0x10000000,                          /// 通用失败返回码

    RET_ERR_PARAM           = 0x10001001,                          /// 参数错误
    RET_ERR_UNREGISTED      = 0x10001002,                          /// 未注册
    RET_ERR_DISCONNECT      = 0x10001003,                          /// 连接已断开
    RET_ERR_TIMEOUT         = 0x10001004,                          /// 操作超时
    RET_ERR_LIST_EMPTY      = 0x10001005,                          /// 链表为空
    RET_ERR_STATUS_ABNORMAL = 0x10001006,                          /// 状态异常
    RET_ERR_PARSE_MSG       = 0x10001007,                          /// 解析消息出错
    RET_ERR_QUEUE_FULL      = 0x10001008,                          /// 队列满
    RET_ERR_SEND_FAIL       = 0x10001009,                          /// 发送出错
    RET_ERR_MEDIA_DATA      = 0x1000100A,                          /// 媒体数据错误
    RET_ERR_CONFIG_ITEM_NOT_FOUND = 0x1000100B,                    /// 配置项不存在

    RET_ERR_SYS             = 0x10002001,                          /// 通用系统错误
    RET_ERR_SYS_NEW         = 0x10002002                           /// 系统调用new失败
} SVS_RET_CODE;

#endif /* _SVSRETCODE_H__ */

