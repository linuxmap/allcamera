#ifndef __INDEX_RESEND_MGR_H_HERDER_INCLUDED__
#define __INDEX_RESEND_MGR_H_HERDER_INCLUDED__

#include "svs_record_index.h"
#include "svs_rt_record_common.h"
#include "svs_timer.h"

// ���δȷ�ϵ�¼�����������·��͵�ʱ����
#define CHECK_UNACKED_INDEX_INTERVAL    60
#define CHECK_UNACKED_INDEX_EVENTID     0x9000
#define UNACKED_INDEX_RESEND_TIMES      3
#define UNACKED_INDEX_RESEND_INTERVAL   120

#define INDEX_KEY_LEN   80

class CIndexResendMgr
{
    // ��Store����ṹ�嶨��һ��
    typedef struct _RecordIndexCacheNode
    {
        Update_Record_Index stIndexInfo;
        uint32_t        uiResendTimes;      // �ط�����
        time_t              ulSendTimeTick;    // ���һ�η���ʱ��
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

    // �趨��ʱ��
    int32_t Init(CAC_Timer_Manager  *pTimerMgr);

    // ��¼�Ѿ�������¼������
    int32_t RecordPastIndex(const Update_Record_Index *pIndex);

    // �����SCC���ص������ϱ���Ӧ��Ϣ
    int32_t HandleReportConfirm(const ACE_Message_Block *mb);

    // ���δ���յ���Ӧ�Ѿ�����������
    int32_t CheckUnackedIndex();

    // ��ʱ���ص�����
    static void TimeCallback(void *pArg, int32_t eventId, int32_t timerId, int32_t /*nDummy*/);

    void DebugGetResendIndexInfo(char *pszBufferInfo, const uint32_t uiBufferLen);

protected:
    // ���·���ָ����¼������
    int32_t ResendIndex(Update_Record_Index *pIndex) const;

    void BatchDealIndex(std::list<string> indexKeyList, int32_t iFlag);

    void MakeIndexInfoKey(const Update_Record_Index *pInfo, string &strKey);

private:
    CIndexResendMgr();

private:
    std::map<string, PRecordIndexCacheNode>  m_indexMap;
    ACE_Recursive_Thread_Mutex               m_mutex;
    // ��ʱ��ID,����ȷ�ϻص������Ĳ���
    int32_t     m_iTimerId;

    // �����Ѿ���������������Ҫ�յ�һ��ȷ����Ϣ����ɾ��
    typedef std::map<string, PRecordIndexCacheNode>           RECORD_INDEX_MAP;
    typedef std::map<string, PRecordIndexCacheNode>::iterator RECORD_INDEX_ITER;
};

#endif //__INDEX_RESEND_MGR_H_HERDER_INCLUDED__
