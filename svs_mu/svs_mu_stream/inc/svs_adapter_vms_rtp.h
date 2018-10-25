#ifndef __SVS_RTP_H__
#define __SVS_RTP_H__

#include <bitset>

#pragma pack(push, 1)

#define PROFILE_LEN 2
#define AUTHORIZATION_LEN 16
/*
 *  RTP扩展类型及扩展数据定义类型
 */
enum SVS_RTP_EXTEN_DATA_TYPE
{
    RTP_EXTEN_DATA_TYPE_NORMAL   = 0x0000,  // 0x0000  正常媒体流
    RTP_EXTEN_DATA_TYPE_EOS      = 0x0001,  // 0x0001  EOS
    SVS_RTP_EXTEN_TYPE_NAT_REQ   = 0x0101,  // 0x0101  NAT穿越请求消息
    SVS_RTP_EXTEN_TYPE_NAT_RESP  = 0x0102   // 0x0102  NAT穿越响应消息
};


/*
 * 消息名称: rtp extension
 * 消息类型:
 * 消息说明:
 */
typedef struct _SVS_RTP_HEADER
{
    //考虑是按位处理还是按整型统一处理
    //uint16_t BitSeq;
    std::bitset<2> V;                     // V标识位
    std::bitset<1> P;                     // P标识位
    std::bitset<1> X;                     // X标识位
    std::bitset<4> CC;                    // CC标识位
    std::bitset<1> M;                     // M标识位
    std::bitset<7> PT;                    // PT标识位
    uint16_t  SequenceNumber;     // sequence number
    uint32_t   TimesTamp;          // timestamp
}SVS_RTP_HEADER;

/*
 * 消息名称: header extension
 * 消息类型:
 * 消息说明:
 */
typedef struct _SVS_RTP_EXTEN_HEADER
{
    char          Profile[PROFILE_LEN]; // defined by peofile
    uint16_t  Length;               // length
    uint16_t  ExVersion;      // 扩展版本，当前值为1
    uint16_t  ExType;         // 扩展类型,参见SVS_RTP_EXTEN_DATA_TYPE
    uint32_t   StreamID;       // 流ID
}SVS_RTP_EXTEN_HEADER;


/*
 * 消息名称: 0x0000  正常媒体流
 * 消息类型:
 * 消息说明:
 */
typedef struct _SVS_RTP_EXTEN_STREAM_NORMAL
{
    SVS_RTP_HEADER   RtpHeader;
    SVS_RTP_EXTEN_HEADER RtpExtenHeader;
    uint32_t    ulFrameID;          // 帧ID
    uint8_t    ucPackCount;        // 该帧中包的个数
    uint8_t    ucPackID;           // 该帧中包的ID，从0开始
    uint8_t    ucIsKeyFrane;       // 是否为关键帧
    uint8_t    ucReserved;         // 保留字段。用于保证32位对齐。
}SVS_RTP_EXTEN_STREAM_NORMAL;


/*
 * 消息名称: 0x0001  EOS
 * 消息类型:
 * 消息说明: 消息体为空
 */
typedef struct _SVS_RTP_EXTEN_STREAM_EOS
{
    SVS_RTP_HEADER RtpHeader;
    SVS_RTP_EXTEN_HEADER RtpExtenHeader;
}SVS_RTP_EXTEN_STREAM_EOS;

/*
 * 消息名称: 0x0101  NAT穿越请求消息
 * 消息类型:
 * 消息说明:
 */
typedef struct _SVS_RTP_EXTEN_NAT_REQ
{
    SVS_RTP_HEADER  RtpHeader;
    SVS_RTP_EXTEN_HEADER RtpExtenHeader;
    uint32_t   ulMsgReqSeq;         // 请求消息序列号
    uint8_t   Authorization[AUTHORIZATION_LEN];   // 认证信息,16字节
    uint32_t   ulLocalIP;           // 媒体请求方NAT穿越前的本地IP地址
    uint16_t  usLocalPort;         // 媒体请求方NAT穿越前的本地端口
    uint16_t  Resvered;            // 保留字段。用于保证32位对齐

}SVS_RTP_EXTEN_NAT_REQ;

/*
 * 消息名称: 0x0102  NAT穿越响应消息
 * 消息类型:
 * 消息说明:
 */
typedef struct _SVS_RTP_EXTEN_NAT_RESP
{
    SVS_RTP_HEADER  RtpHeader;
    SVS_RTP_EXTEN_HEADER RtpExtenHeader;
    uint32_t   ulMsgReqSeq;    // 请求消息序列号
    uint32_t   ulLocalIP;      // 媒体请求方NAT穿越前的本地IP地址
    uint16_t  usLocalPort;    // 媒体请求方NAT穿越前的本地端口
    uint32_t   ulSrcIP;        // 媒体请求方NAT穿越后的IP地址
    uint16_t  usSrcPort;      // 媒体请求方NAT穿越后的端口
}SVS_RTP_EXTEN_NAT_RESP;

#pragma pack(pop)

#endif /* __SVS_RTP_H__ */

