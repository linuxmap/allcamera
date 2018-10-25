#ifndef __INDEX_RESEND_MGR_H_HERDER_INCLUDED__
#define __INDEX_RESEND_MGR_H_HERDER_INCLUDED__

#include "svs_record_index.h"
#include "svs_rt_record_common.h"
#include "svs_timer.h"

// 检查未确认的录像索引并重新发送的时间间隔
#define CHECK_UNACKED_INDEX_INTERVAL    60
#define CHECK_UNACKED_INDEX_EVENTID     0x9000
#define UNACKED_INDEX_RESEND_TIMES      3
#define UNACKED_INDEX_RESEND_INTERVAL   120

#define INDEX_KEY_LEN   80

class CIndexResendMgr
{
    // 与Store缓存结构体定义一致
    typedef struct _RecordIndexCacheNode
    {
        Update_Record_Index stIndexInfo;
        uint32_t        uiResendTimes;      // 重发次数
        time_t              ulSendTimeTick;    // 最后一次发送时间
    }RecordIndexCacheNode, *PRecordIndexCacheNode;

    enum
    {
        DELETE_INDEX = 1,
        RESEND_INDEX = 2,
    };

public:
    virtual ~CIndexResendMgr();

    static CIndexResendMgr *instance()
    {
        static CIndexResendMgr objInstance;
        return &objInstance;
    }

    // 设定定时器
    int32_t Init(CAC_Timer_Manager  *pTimerMgr);

    // 记录已经结束的录像索引
    int32_t RecordPastIndex(const Update_Record_Index *pIndex);

    // 处理从SCC返回的索引上报响应消息
    int32_t HandleReportConfirm(const ACE_Message_Block *mb);

    // 检测未接收到响应已经结束的索引
    int32_t CheckUnackedIndex();

    // 定时器回调函数
    static void TimeCallback(void *pArg, int32_t eventId, int32_t timerId, int32_t /*nDummy*/);

    void DebugGetResendIndexInfo(char *pszBufferInfo, const uint32_t uiBufferLen);

protected:
    // 重新发送指定的录像索引
    int32_t ResendIndex(Update_Record_Index *pIndex) const;

    void BatchDealIndex(std::list<string> indexKeyList, int32_t iFlag);

    void MakeIndexInfoKey(const Update_Record_Index *pInfo, string &strKey);

private:
    CIndexResendMgr();

private:
    std::map<string, PRecordIndexCacheNode>  m_indexMap;
    ACE_Recursive_Thread_Mutex               m_mutex;
    // 定时器ID,用来确认回调函数的参数
    int32_t     m_iTimerId;

    // 保存已经结束的索引，需要收到一个确认消息才能删除
    typedef std::map<string, PRecordIndexCacheNode>           RECORD_INDEX_MAP;
    typedef std::map<string, PRecordIndexCacheNode>::iterator RECORD_INDEX_ITER;
};

#endif //__INDEX_RESEND_MGR_H_HERDER_INCLUDED__
