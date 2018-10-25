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
    // 码率统计
    m_ulStartTime   = (uint32_t)time(NULL);
    m_ulEndTime     = m_ulStartTime;
    m_ulBytes       = 0;
    m_recvBytesMap.clear();

    // 流量统计
    m_ulFluxBytes   = 0;
    m_ulFluxStartTime = m_ulStartTime;

    // 丢包丢帧统计
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

    // 如果时间发生了反转，清空数据
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

        // 判断最后一个数据的接收时间是否等于当前时间
        if (iter->first == ulCurTime)
        {
            iter->second += ulRecvBytes;
        }
        else
        {
            m_recvBytesMap.insert(std::make_pair(ulCurTime, ulRecvBytes));
        }
    }


    // 如果超过统计周期，去除过期数据
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
            // 后续数据也不会过期了
            // 更新统计的起始时间
            m_ulStartTime = curIter->first;
            break;
        }
    }

    // 丢包丢帧周期检测
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

    // 取完数据后，重新统计
    m_ulFluxBytes = 0;
    m_ulFluxStartTime = ulEndTime;

    return ulFlux;
}



