#ifndef __CSTREAMDATASTAT_H__
#define __CSTREAMDATASTAT_H__


#include  <map>

#define DATA_STAT_PERIOD  30
class  CDataStat
{
public:
    CDataStat();
    ~CDataStat();

    void stat(uint32_t ulRecvBytes);
    void addRecvPacket(uint32_t ulRecvNum);
    void addRecvFrame(uint32_t ulRecvNum);
    void addLostPacket(uint32_t ulLostNum);
    void addLostFrame(uint32_t ulLostNum);
    void addFlux(uint32_t ulSendBytes);

    uint32_t getRate()const;
    uint32_t getFlux(uint32_t& ulStartTime, uint32_t& ulEndTime);
    uint32_t getLostPacketPercent()const;
    uint32_t getLostFramePercent()const;
private:
    void reset();
    void resetLostStat();
private:
    uint32_t m_ulStartTime;   // 起始时间
    uint32_t m_ulEndTime;     // 结束时间
    uint32_t m_ulBytes;       // 码率统计周期为30秒
    typedef std::map<uint32_t, uint32_t> CDataRecvMap;
    typedef CDataRecvMap::iterator               CDataRecvIter;

    CDataRecvMap m_recvBytesMap;

    // 丢包和丢帧数据由外部自行刷新
    // 每个统计周期过后，清零后，重新统计
    uint32_t m_ulLostStatTime;
    uint32_t m_ulLostPacket;
    uint32_t m_ulLostFrame;
    uint32_t m_ulRecvPacket;
    uint32_t m_ulRecvFrame;

    uint32_t m_ulFluxBytes;
    uint32_t m_ulFluxStartTime;
};

#endif




