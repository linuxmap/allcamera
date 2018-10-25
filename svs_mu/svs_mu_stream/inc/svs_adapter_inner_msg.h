/*
 * StreamInnerMsg.h
 *
 *  Created on: 2016-1-12
 *      Author:
 */

#ifndef STREAMINNERMSG_H_
#define STREAMINNERMSG_H_
/*lint --e{528}*/
enum _enINNER_MSG_TYPE
{
    INNER_MSG_SVS,            /// SVS
    INNER_MSG_RTP,            /// RTP
    INNER_MSG_RTSP,           /// RTSP
    INNER_MSG_RTCP,            /// RTCP
    INNER_MSG_RTPDUMMY,        /// RTPDUMMY
    INNER_MSG_RTCPDUMMY,        /// RTCPDUMMY
    INNER_MSG_RELEASE_SESSION  ///
};


typedef struct _stSTREAM_INNER_MSG
{
    uint64_t         ullStreamID;
    void*            pRecvHandle;
    uint32_t         unRemoteIp;
    uint16_t         usRemotePort;
    uint16_t         usMsgType;
    uint32_t         unBodyOffset;


} STREAM_INNER_MSG;


static inline void fillStreamInnerMsg(char *pData,
        const uint64_t ullStreamID,
        const void* pRecvHandle,
        const uint32_t remoteIp,
        const uint16_t remotePort,
        const uint16_t usMsgType,
        const uint32_t unBodyOffset)
{
    STREAM_INNER_MSG* pInnerMsg = (STREAM_INNER_MSG*)(void*)pData;
    pInnerMsg->ullStreamID   = ullStreamID;
    pInnerMsg->pRecvHandle   = (void*)pRecvHandle;
    pInnerMsg->unRemoteIp    = remoteIp;
    pInnerMsg->usRemotePort  = remotePort;
    pInnerMsg->usMsgType     = usMsgType;
    pInnerMsg->unBodyOffset  = unBodyOffset;
}

/*lint -restore*/
#endif /* STREAMINNERMSG_H_ */
