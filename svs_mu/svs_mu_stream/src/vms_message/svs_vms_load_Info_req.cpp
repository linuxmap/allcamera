/*
 * MduLoadInfoReq.cpp
 *
 *  Created on: 2016-3-22
 *      Author:
 */
#include "svs_adapter_svs_retcode.h"
#include "svs_log_msg.h"
#include "svs_adapter_config.h"
#include "svs_adapter_sys_stat.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_vms_load_Info_req.h"
#include "svs_adapter_service_task.h"
CMduLoadInfoReq::CMduLoadInfoReq()
{
    m_pLoadInfo = NULL;
}

CMduLoadInfoReq::~CMduLoadInfoReq()
{
    m_pLoadInfo = NULL;
}

// �����Ϣ���ȳ�ʼ����Ϣ��ֻ��ʼ����Ϣͷ��������Ҫ����
// ����Ϣֻ��MDU�������ʲ���Ҫʵ�������Create����
int32_t CMduLoadInfoReq::create(uint32_t unLength,
                            uint32_t unTransNo)
{
    // �ȵ��ø���ĳ�ʼ���ӿ�
    int32_t nRet = CMduSvsMessage::create(unLength, unTransNo);
    if (RET_OK != nRet)
    {
        return nRet;
    }

    m_pLoadInfo = (SVS_MSG_MDU_LOAD_INFO_REQ*)(void*)getBinaryData();

    // �Լ���������Ϣ����Ҫ�����
    return RET_OK;
}

/// ��ʼ����Ϣ��
int32_t CMduLoadInfoReq::initMsgBody()
{
    if (NULL == m_pLoadInfo)
    {
        return RET_FAIL;
    }

    m_pLoadInfo->TransmitNumber = CMduConfig::instance()->getServiceCapacity();

    uint32_t ulTotal = 0;
    uint32_t ulUsed  = 0;
    CSvsSysStat::instance().GetMemoryInfo(ulTotal, ulUsed);
    m_pLoadInfo->TotalMemorySize = ulTotal / 1024;
    m_pLoadInfo->UsedMemorySize  = ulUsed / 1024;

    SVS_LOG((SVS_LM_INFO,"Stat memory: total[%u] used[%u].", ulTotal, ulUsed));

    // CPU
    CSvsSysStat::instance().GetCpuInfo(ulUsed);
    m_pLoadInfo->UsedCpuLoad   = ulUsed;
    m_pLoadInfo->IowaitCpuLoad = 0;
    SVS_LOG((SVS_LM_INFO,"Stat cpu: used[%u].", ulUsed));

    // CACHE
    m_pLoadInfo->CacheTotalNum = CMediaBlockBuffer::instance().getMaxBlockNum();
    m_pLoadInfo->CacheUsedNum  = m_pLoadInfo->CacheTotalNum
                                    - CMediaBlockBuffer::instance().getRemainBlockNum();
    SVS_LOG((SVS_LM_INFO,"Stat cache: total[%u] used[%u].",
            m_pLoadInfo->CacheTotalNum,
            m_pLoadInfo->CacheUsedNum));

    // ip
    MDU_IP_LIST  ipList;
    ACE_INET_Addr addr;
    uint32_t UsedRecvSize = 0;
    uint32_t UsedSendSize = 0;
    uint32_t  unInternalIp = CMduConfig::instance()->getInternalMediaIp();
    CMduConfig::instance()->getExternalMediaIpList(ipList);

    ipList.push_front(unInternalIp);
    m_pLoadInfo->NetworkCardNum = 0;

    for (MDU_IP_LIST::iterator iter = ipList.begin(); iter != ipList.end(); iter++)
    {
        addr.set((uint16_t)0, *iter);
        if (RET_OK == CSvsSysStat::instance().GetNetworkCardInfo(addr.get_host_addr(),
                                                                 ulTotal,
                                                                 UsedRecvSize, UsedSendSize))
        {
            m_pLoadInfo->NetworkCardInfo[m_pLoadInfo->NetworkCardNum].IngressTotalBW = ulTotal;
            m_pLoadInfo->NetworkCardInfo[m_pLoadInfo->NetworkCardNum].IngressUsedBW  = UsedRecvSize;
            m_pLoadInfo->NetworkCardInfo[m_pLoadInfo->NetworkCardNum].EgressTotalBW  = ulTotal;
            m_pLoadInfo->NetworkCardInfo[m_pLoadInfo->NetworkCardNum].EgressUsedBW   = UsedSendSize;
            m_pLoadInfo->NetworkCardNum++;
            SVS_LOG((SVS_LM_INFO,"stat netcard[%s]: TotalBW[%u] UsedRecvBW[%u] UsedSendBW[%u]",
                    addr.get_host_addr(), ulTotal, UsedRecvSize, UsedSendSize));
        }
    }

    return RET_OK;
}

uint32_t CMduLoadInfoReq::getMsgType()
{
    return SVS_MSG_TYPE_MDU_LOAD_INFO_REQ;
}

int32_t CMduLoadInfoReq::handleMessage()
{
    // ����ϢΨһ�Ĵ�����ǰ��Լ����͸�SCC
    return CMduServiceTask::instance()->sendMsgToSCC(this);
}

void CMduLoadInfoReq::dump() const
{
    if (NULL == m_pLoadInfo)
    {
        return;
    }

    CMduSvsMessage::dump();

    // ����Ϣ�ṹ��ӡ��Ϣ��
    SVS_LOG((SVS_LM_DEBUG,"MessageBody:"));
    SVS_LOG((SVS_LM_DEBUG,"\tTransmitNumber: %u", m_pLoadInfo->TransmitNumber));
    SVS_LOG((SVS_LM_DEBUG,"\tTotalMemorySize: %u", m_pLoadInfo->TotalMemorySize));
    SVS_LOG((SVS_LM_DEBUG,"\tUsedMemorySize: %u", m_pLoadInfo->UsedMemorySize));
    SVS_LOG((SVS_LM_DEBUG,"\tUsedCpuLoad: %u", m_pLoadInfo->UsedCpuLoad));
    SVS_LOG((SVS_LM_DEBUG,"\tIowaitCpuLoad: %u", m_pLoadInfo->IowaitCpuLoad));
    SVS_LOG((SVS_LM_DEBUG,"\tCacheTotalNum: %u", m_pLoadInfo->CacheTotalNum));
    SVS_LOG((SVS_LM_DEBUG,"\tCacheUsedNum: %u", m_pLoadInfo->CacheUsedNum));
    SVS_LOG((SVS_LM_DEBUG,"\tNetworkCardNum: %u", m_pLoadInfo->NetworkCardNum));
    for (uint32_t unLoop = 0; unLoop < m_pLoadInfo->NetworkCardNum; unLoop++)
    {
        SVS_LOG((SVS_LM_DEBUG,"\t\tNetworkCard: %u", unLoop));
        SVS_LOG((SVS_LM_DEBUG,"\t\tIngressTotalBW: %u", m_pLoadInfo->NetworkCardInfo[unLoop].IngressTotalBW));
        SVS_LOG((SVS_LM_DEBUG,"\t\tIngressUsedBW: %u", m_pLoadInfo->NetworkCardInfo[unLoop].IngressUsedBW));
        SVS_LOG((SVS_LM_DEBUG,"\t\tEgressTotalBW: %u", m_pLoadInfo->NetworkCardInfo[unLoop].EgressTotalBW));
        SVS_LOG((SVS_LM_DEBUG,"\t\tEgressUsedBW: %u", m_pLoadInfo->NetworkCardInfo[unLoop].EgressUsedBW));
    }

    return;
}
