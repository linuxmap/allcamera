#include "svs_log_msg.h"
#include "svs_data_stat.h"
#include "svs_adapter_def.h"


CDataStat::CDataStat()
{
    reset();
}

CDataStat::~CDataStat()
{
}

void CDataStat::reset()
{
    // ����ͳ��
    m_ulStartTime   = (uint32_t)time(NULL);
    m_ulEndTime     = m_ulStartTime;
    m_ulBytes       = 0;
    m_recvBytesMap.clear();

    // ����ͳ��
    m_ulFluxBytes   = 0;
    m_ulFluxStartTime = m_ulStartTime;

    // ������֡ͳ��
    resetLostStat();
}

void CDataStat::resetLostStat()
{
    m_ulLostPacket  = 0;
    m_ulLostFrame   = 0;
    m_ulRecvPacket  = 0;
    m_ulRecvFrame   = 0;
    m_ulLostStatTime = 0;
}

void CDataStat::stat(uint32_t ulRecvBytes)
{
    uint32_t ulCurTime = (uint32_t)time(NULL);

    if (0 == m_ulLostStatTime)
    {
        m_ulLostStatTime = ulCurTime;
    }

    m_ulEndTime = ulCurTime;

    // ���ʱ�䷢���˷�ת���������
    if (m_ulStartTime > m_ulEndTime)
    {
        reset();
    }

    m_ulBytes += ulRecvBytes;


    CDataRecvIter iter;
    if (m_recvBytesMap.empty())
    {
        m_recvBytesMap.insert(std::make_pair(ulCurTime, ulRecvBytes));
    }
    else
    {
        iter = m_recvBytesMap.end();
        iter--;

        // �ж����һ�����ݵĽ���ʱ���Ƿ���ڵ�ǰʱ��
        if (iter->first == ulCurTime)
        {
            iter->second += ulRecvBytes;
        }
        else
        {
            m_recvBytesMap.insert(std::make_pair(ulCurTime, ulRecvBytes));
        }
    }


    // �������ͳ�����ڣ�ȥ����������
    CDataRecvIter curIter;
    for (iter = m_recvBytesMap.begin(); iter != m_recvBytesMap.end();)
    {
        curIter = iter;
        iter++;

        if (ulCurTime - curIter->first >= DATA_STAT_PERIOD)
        {
            if (m_ulBytes > curIter->second)
            {
                m_ulBytes -= curIter->second;
            }

            m_recvBytesMap.erase(curIter);
        }
        else
        {
            // ��������Ҳ���������
            // ����ͳ�Ƶ���ʼʱ��
            m_ulStartTime = curIter->first;
            break;
        }
    }

    // ������֡���ڼ��
    if (m_ulEndTime - m_ulLostStatTime >= DATA_STAT_PERIOD)
    {
        resetLostStat();
        m_ulLostStatTime = ulCurTime;
    }
}

void CDataStat::addFlux(uint32_t ulSendBytes)
{
    uint32_t ulCurTime = (uint32_t)time(NULL);
    if (0 == m_ulFluxStartTime)
    {
        m_ulFluxStartTime = ulCurTime;
    }

    m_ulFluxBytes += ulSendBytes;
}

void CDataStat::addRecvPacket(uint32_t ulRecvNum)
{
    m_ulRecvPacket += ulRecvNum;
}
void CDataStat::addRecvFrame(uint32_t ulRecvNum)
{
    m_ulRecvFrame += ulRecvNum;
}
void CDataStat::addLostPacket(uint32_t ulLostNum)
{
    m_ulLostPacket += ulLostNum;
}
void CDataStat::addLostFrame(uint32_t ulLostNum)
{
    m_ulLostFrame += ulLostNum;
}

uint32_t CDataStat::getRate()const
{
    if (m_ulEndTime <= m_ulStartTime)
    {
        return 0;
    }

    uint32_t ulRate;
    ulRate = (m_ulBytes * BYTE_NUM) / ((m_ulEndTime - m_ulStartTime) * KILO);  // kbps
    return ulRate;
}

uint32_t CDataStat::getLostPacketPercent()const
{
    if (0 == m_ulRecvPacket)
    {
        return 0;
    }

    uint32_t ulLostPercent;
    ulLostPercent = m_ulLostPacket / m_ulRecvPacket;
    return ulLostPercent;
}
uint32_t CDataStat::getLostFramePercent()const
{
    if (0 == m_ulRecvFrame)
    {
        return 0;
    }

    uint32_t ulLostPercent;
    ulLostPercent = m_ulLostFrame / m_ulRecvFrame;
    return ulLostPercent;
}


uint32_t CDataStat::getFlux(uint32_t& ulStartTime, uint32_t& ulEndTime)
{
    ulStartTime = m_ulFluxStartTime;
    ulEndTime   = (uint32_t)time(NULL);

    uint32_t ulFlux = m_ulFluxBytes;

    // ȡ�����ݺ�����ͳ��
    m_ulFluxBytes = 0;
    m_ulFluxStartTime = ulEndTime;

    return ulFlux;
}



