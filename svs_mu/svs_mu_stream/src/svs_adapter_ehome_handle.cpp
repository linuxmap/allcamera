#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_time.h"
#include <vms/vms.h>
#include "svs_adapter_ehome_handle.h"

#include "svs_adapter_net_conn_manager.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_adapter_session.h"
#include "svs_adapter_media_processor.h"
#include "svs_adapter_media_exchange.h"
#include "svs_adapter_session_factory.h"
#include "svs_utility.h"

struct ehome_ps_start_code
{
    unsigned char start_code[3];
    unsigned char stream_id[1];
};


CEhomeStreamHandle::CEhomeStreamHandle()
{
    m_ullStreamId = 0;
    m_lSessionId = -1;
    m_lLinkHandle = -1;
}
CEhomeStreamHandle::~CEhomeStreamHandle()
{
}
void    CEhomeStreamHandle::setStreamID(uint64_t ullSessionID)
{
    m_ullStreamId = ullSessionID;
}

void    CEhomeStreamHandle::setSessionID(int32_t lSessionID)
{
    m_lSessionId = lSessionID;
}

int32_t CEhomeStreamHandle::getSessionID()
{
    return m_lSessionId;
}
int32_t CEhomeStreamHandle::startLinkHandle(int32_t lLinkHandle)
{
    m_lLinkHandle = lLinkHandle;
    NET_EHOME_PREVIEW_DATA_CB_PARAM  StruCBParam;
    memset(&StruCBParam,0,sizeof(NET_EHOME_PREVIEW_DATA_CB_PARAM));

    StruCBParam.fnPreviewDataCB = preview_data_cb;
    StruCBParam.pUserData = this;
    if(!NET_ESTREAM_SetPreviewDataCB(m_lLinkHandle,&StruCBParam))
    {
        return RET_FAIL;
    }
    return RET_OK;
}
void CEhomeStreamHandle::stopLinkHandle()
{
    if(-1 < m_lLinkHandle)
    {
        (void)NET_ESTREAM_StopPreview(m_lLinkHandle);
    }
    return ;
}
void CEhomeStreamHandle::handle_preview_data(LONG  iPreviewHandle,
                                    NET_EHOME_PREVIEW_CB_MSG *pPreviewCBMsg)
{
    if(NET_EHOME_STREAMDATA != pPreviewCBMsg->byDataType)
    {
        return;
    }

    ehome_ps_start_code* phead = (ehome_ps_start_code*)pPreviewCBMsg->pRecvdata;
    if ((phead->start_code[0] != 0x00)
        ||(phead->start_code[1] != 0x00)
        ||(phead->start_code[2] != 0x01)
        || (phead->stream_id[0] != 0xBA))
    {
        // not ps stream
        return;
    }
    send_ehome_stream((char*)pPreviewCBMsg->pRecvdata,(uint32_t)pPreviewCBMsg->dwDataLen);
    return;
}
void CEhomeStreamHandle::send_ehome_stream(char* pdata,uint32_t ulDataLen)
{
}


void CEhomeStreamHandle::preview_data_cb(LONG  iPreviewHandle,
                     NET_EHOME_PREVIEW_CB_MSG *pPreviewCBMsg,void *pUserData)
{
    CEhomeStreamHandle* pHandle = (CEhomeStreamHandle*)pUserData;
    pHandle->handle_preview_data(iPreviewHandle, pPreviewCBMsg);
}

CEhomeHandle::CEhomeHandle()
{
    m_ulLastInvalidAlarmTime = 0;
    m_ulLastRecvTime         = 0;
    m_unStreamCount          = 0;
    m_ullStreamID            = 0;

    m_enPlayType          = 0;

    m_lListenHandle          = -1;
}

CEhomeHandle::~CEhomeHandle()
{
    try
    {
        this->closeHandle();
    }
    catch(...)
    {}
    m_ulLastInvalidAlarmTime = 0;
    m_unStreamCount          = 0;
    m_ullStreamID            = 0;
}

int32_t CEhomeHandle::openHandle(const ACE_INET_Addr &localAddr)
{
    if (NETWORK_HANDLE_STATUS_INIT != getStatus())
    {
        SVS_LOG((SVS_LM_WARNING,"open ehome handle address[%s:%d] fail, status[%d] abnormal.",
                        localAddr.get_host_addr(),
                        localAddr.get_port_number(),
                        getStatus()));
        return RET_ERR_STATUS_ABNORMAL;
    }

    setLocalAddr(localAddr);

    NET_EHOME_LISTEN_PREVIEW_CFG elisten;
    memset(&elisten,0,sizeof(NET_EHOME_LISTEN_PREVIEW_CFG));

    strncpy(elisten.struIPAdress.szIP,localAddr.get_host_addr(),128);
    elisten.struIPAdress.wPort = localAddr.get_port_number();
    elisten.fnNewLinkCB = handle_link;
    elisten.pUser       = this;
    elisten.byLinkMode  = CMduConfig::instance()->getEhomeTransType();

    LONG nRet = NET_ESTREAM_StartListenPreview(&elisten);
    if(-1 == nRet)
    {
        SVS_LOG((SVS_LM_ERROR,"ehome handle ip[%s] port[%d] open fail.",
            localAddr.get_host_addr(),
            localAddr.get_port_number()));
        return RET_FAIL;
    }
    m_lListenHandle = nRet;
    setStatus( NETWORK_HANDLE_STATUS_OPEN);
    SVS_LOG((SVS_LM_DEBUG,"ehome handle ip[%s] port[%d] open success.",
            localAddr.get_host_addr(),
            localAddr.get_port_number()));

    return RET_OK;
}

void CEhomeHandle::closeHandle()
{
    if(0 > m_lListenHandle)
    {
        return;
    }
    (void)NET_ESTREAM_StopListenPreview(m_lListenHandle);
    m_lListenHandle = -1;

    setStatus( NETWORK_HANDLE_STATUS_INIT);
    SVS_LOG((SVS_LM_DEBUG,"close ehome handle[%s:%d] success.",
            getLocalAddr().get_host_addr(),
            getLocalAddr().get_port_number()));
    return;
}

int32_t CEhomeHandle::startHandle(uint64_t ullStreamID, const ACE_INET_Addr &remoteAddr)
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_ListMutex);
    if ((NETWORK_HANDLE_STATUS_OPEN != getStatus())
      &&(NETWORK_HANDLE_STATUS_START != getStatus()))
    {
        SVS_LOG((SVS_LM_WARNING,
            "Start ehome handle fail, status [%d] is incorrect.",
            getStatus()));
        return RET_ERR_STATUS_ABNORMAL;
    }

    m_RemoteAddr = remoteAddr;

    CEhomeStreamHandle* pStreamHandle = NULL;

    pStreamHandle = SVS_NEW(pStreamHandle);
    if(NULL == pStreamHandle)
    {
        return RET_ERR_SYS_NEW;
    }
    pStreamHandle->setStreamID(ullStreamID);

    m_mapStreamHandle.insert(EHOMESTREAMMAP::value_type(ullStreamID,pStreamHandle));

    m_unStreamCount++;

    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        m_ulLastRecvTime = SVS_GetSecondTime();
        setStatus( NETWORK_HANDLE_STATUS_START);
    }

    SVS_LOG((SVS_LM_INFO,"start stream[%Q] ehome handle[%s:%d], all stream count[%d].",
            ullStreamID,
            getLocalAddr().get_host_addr(),
            getLocalAddr().get_port_number(),
            m_unStreamCount));

    return RET_OK;
}

int32_t CEhomeHandle::stopHandle(uint64_t ullStreamID)
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_ListMutex);
    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        return RET_ERR_STATUS_ABNORMAL;
    }

    m_unStreamCount--;

    if (0 == m_unStreamCount)
    {
        m_ullStreamID = 0;
        setStatus( NETWORK_HANDLE_STATUS_OPEN);
    }
    CEhomeStreamHandle* pStreamHandle = NULL;

    EHOMESTREAMMAP::iterator iter = m_mapStreamHandle.find(ullStreamID);
    if(iter != m_mapStreamHandle.end())
    {
        pStreamHandle = iter->second;
        pStreamHandle->stopLinkHandle();
        m_mapStreamHandle.erase(iter);
        SVS_DELETE(pStreamHandle);
        pStreamHandle = NULL;
    }

    SVS_LOG((SVS_LM_INFO,"stop stream[%Q] ehome handle[%s:%d], all stream count[%d].",
            ullStreamID,
            getLocalAddr().get_host_addr(),
            getLocalAddr().get_port_number(),
            m_unStreamCount));
    return RET_OK;
}

int32_t CEhomeHandle::sendMessage(uint64_t ullStreamID,
                            const char *pDataBuffer,
                            uint32_t unDataSize,
                            const ACE_INET_Addr &peerAddr)
{

    return RET_OK;
}

bool CEhomeHandle::checkHandleStatus(uint64_t /*ullStreamID*/, uint32_t unTimeout)
{
    if (NETWORK_HANDLE_STATUS_START != getStatus())
    {
        SVS_LOG((SVS_LM_INFO,"check udp port[%d] status fail, current status[%d].",
                getLocalAddr().get_port_number(), getStatus()));
        return false;
    }

    uint32_t ulCostTime = SVS_GetSecondTime() - m_ulLastRecvTime;
    if (unTimeout <= ulCostTime)
    {
        SVS_LOG((SVS_LM_INFO,"check udp port[%d] status fail, not recv any data in [%u]s.",
                        getLocalAddr().get_port_number(), ulCostTime));
        return false;
    }

    SVS_LOG((SVS_LM_DEBUG,"check udp port[%d] status success.",
                    getLocalAddr().get_port_number()));
    return true;
}

void CEhomeHandle::setEhomeSessionID(uint64_t ullStreamID,int32_t ulSessionID)
{
    ACE_Guard<ACE_Thread_Mutex> locker(m_ListMutex);
    CEhomeStreamHandle* pStreamHandle = NULL;

    EHOMESTREAMMAP::iterator iter = m_mapStreamHandle.find(ullStreamID);
    if(iter != m_mapStreamHandle.end())
    {
        pStreamHandle = iter->second;
        pStreamHandle->setSessionID(ulSessionID);
    }
}
BOOL CEhomeHandle::handle_input_link(LONG lLinkHandle, NET_EHOME_NEWLINK_CB_MSG *pNewLinkCBMsg)
{
    /*1. find the stream handle by session id */
    ACE_Guard<ACE_Thread_Mutex> locker(m_ListMutex);
    CEhomeStreamHandle* pStreamHandle = NULL;

    EHOMESTREAMMAP::iterator iter = m_mapStreamHandle.begin();
    for(;iter != m_mapStreamHandle.end();++iter)
    {
        pStreamHandle = iter->second;
        if(pNewLinkCBMsg->iSessionID == pStreamHandle->getSessionID())
        {
            break;
        }
        pStreamHandle = NULL;
    }

    if(NULL == pStreamHandle)
    {
        SVS_LOG((SVS_LM_WARNING,
            "handle ehome new input handle fail, ehome session id [%d] is incorrect.",
            pNewLinkCBMsg->iSessionID));
        return FALSE;
    }

    if(RET_OK != pStreamHandle->startLinkHandle(lLinkHandle))
    {
        SVS_LOG((SVS_LM_WARNING,
            "start ehome handle fail, ehome session id [%d].",
            pNewLinkCBMsg->iSessionID));
        return FALSE;
    }

    return TRUE;

}


BOOL CEhomeHandle::handle_link(LONG lLinkHandle, NET_EHOME_NEWLINK_CB_MSG *pNewLinkCBMsg, void *pUserData)
{
    CEhomeHandle* pEhomeHandle = (CEhomeHandle*)pUserData;
    return pEhomeHandle->handle_input_link(lLinkHandle, pNewLinkCBMsg);
}

