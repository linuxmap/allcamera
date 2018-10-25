/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, �¿���Ϣ�������޹�˾

 ******************************************************************************
  �� �� ��   : SVS_Record_Trace.h
  �� �� ��   : ����
  ��������   : 2010��3��10��
  ����޸�   :
  ��������   : ¼�����������ࡣ����¼����ٲ��ԣ������Բɼ�¼�����������������ϱ�
               ������̨��
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2010��3��10��
    �޸�����   : �����ļ�

******************************************************************************/
#ifndef __SVS_RECORD_TRACE_H__
#define __SVS_RECORD_TRACE_H__

// ��ʱ�����
enum
{
    SVS_TRACE_TIMER_TYPE_STRATEGY   = 1,
    SVS_TRACE_TIMER_TYPE_SAMPLE     = 2,
    SVS_TRACE_TIMER_TYPE_REPORT     = 3,
};

typedef ACE_Vector <SVS_RECORD_QUALITY_REPORT> RECORD_QUALITY_REPORT_VECTOR;

// ����id��¼����ٲ���map
typedef ACE_Hash_Map_Manager < ACE_CString, SVS_RECORD_TRACE_POLICY *, ACE_Null_Mutex >
    SVS_TRACE_POLICY_MAP;

// ��ͷid��¼���豸���������
typedef ACE_Hash_Map_Iterator < ACE_CString, SVS_RECORD_TRACE_POLICY *, ACE_Null_Mutex >
    SVS_TRACE_POLICY_MAP_ITER;

typedef ACE_Hash_Map_Entry < ACE_CString, SVS_RECORD_TRACE_POLICY * >
    SVS_TRACE_POLICY_MAP_ENTRY;

// ¼����ٲ����ࡣÿ����ͷ��Ӧһ������
class SVS_Record_Trace_Strategy
{
public:
    SVS_Record_Trace_Strategy(const char* strDeviceId, uint32_t interalId);
    virtual ~SVS_Record_Trace_Strategy();

    // ���ã���Դ��λ
    UTAPI int32_t reset();

    // ͬ��¼����ٲ���
    UTAPI int32_t syncTraceStrategy(const SVS_RECORD_TRACE_POLICY* pTracePolicy);

    // ¼�������ɼ�
    UTAPI int32_t sampleRecordQuality();

    // ¼�������ϱ�
    UTAPI int32_t getRecordQualityReports(RECORD_QUALITY_REPORT_VECTOR &szRecordReportVct);

    // �����Ƿ��Ѿ�����
    UTAPI int32_t isTraceFinished();

    // ����Ӧ�ü���
    UTAPI int32_t increaseReference();

    // �ݼ����ü���������ݼ������ü������ڣ�����·���Ѿ�ֹͣ����ô����ɾ���ζ���
    UTAPI int32_t decreaseReference();

    // ��ȡ��ͷ��id
    UTAPI const char* deviceId()const;

    // ����ɸѡ
    UTAPI int32_t selectTraceStrategy();

private:
    // ���¼����ٲ���
    UTAPI int32_t addTraceStrategy(const SVS_RECORD_TRACE_POLICY* pTracePolicy);

    // ����¼����ٲ���
    UTAPI int32_t modTraceStrategy(const SVS_RECORD_TRACE_POLICY* pTracePolicy);

    // ɾ��¼����ٲ���
    UTAPI int32_t delTraceStrategy(const char* strStrategyId);

    // ���͸��ٲ������֪ͨ��Ϣ
    UTAPI void sendStrategyDoneNotifyMsg(const char * strStrategyId)const;

    // ���ٲ��Ըո���Ч����λ��������
    UTAPI void resetSampleInfo(bool bOldSampleFlag, bool bNewSampleFlag);

private:
    // ���������б�
    RECORD_QUALITY_REPORT_VECTOR m_szReportVct;

    // ��ͷID
    char m_strDeviceId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    // ¼����ٲ���map
    SVS_TRACE_POLICY_MAP m_strategyMap;
    ACE_Recursive_Thread_Mutex m_mutex;

    // ���ü���
    uint32_t m_ulRefCount;

    // ���Զ�ʱ��
    int32_t m_lStrategyTimer;

    // �ڲ�id
    uint32_t m_ulInternalId;

    // �Ƿ���Ҫ�ɼ��ı�ʾ
    bool m_bSampleFlag;

    // ��¼��ɼ����ڵ���ʼʱ��
    time_t m_startSampleTime;
private:
#ifdef UNITTEST
    friend class SVS_Record_Trace_Strategy_Test;
#endif
};


// ��ͷid��¼������豸�����map
typedef ACE_Hash_Map_Manager < ACE_CString, SVS_Record_Trace_Strategy *, ACE_Null_Mutex >
    SVS_TRACE_DEVICE_MAP;

// �ڲ�ID��¼������豸�����map
typedef ACE_Hash_Map_Manager <uint32_t, SVS_Record_Trace_Strategy *, ACE_Null_Mutex>
    SVS_TRACE_DEVICE_INTERNAL_ID_MAP;

// ��ͷid��¼���豸���������
typedef ACE_Hash_Map_Iterator < ACE_CString, SVS_Record_Trace_Strategy *, ACE_Null_Mutex >
    SVS_TRACE_DEVICE_MAP_ITER;

typedef ACE_Hash_Map_Entry < ACE_CString, SVS_Record_Trace_Strategy * >
    SVS_TRACE_DEVICE_MAP_ENTRY;

// ¼����ٲ��Թ����ࡣ
class SVS_Record_Trace_Manager
{
public:
    virtual ~SVS_Record_Trace_Manager();

    // ����
    static SVS_Record_Trace_Manager *instance()
    {
        static SVS_Record_Trace_Manager szTraceManager;
        return &szTraceManager;
    }

    // ���Ը���
    int32_t syncRecordTraceStrategy(const ACE_Message_Block* mb);

    // ��ʱ���ص�����
    static void timerCallback(  void *pArg, int32_t nEventId, int32_t lTimerId, int32_t nDummy );

    // ��ʱ��ʱ�䴦��
    int32_t onTimer(int32_t nEventId, int32_t lTimerId, int32_t nDummy);

     // ��ָ����������ü�������������˶����Ѿ�Ϊֹͣ״̬�����ü���Ϊ������ô�����ٴ˶���
    int32_t decrease_reference(SVS_Record_Trace_Strategy *pDevice);

private:
    // ����ģʽ���࣬���캯������
    SVS_Record_Trace_Manager();

    // ¼�������ɼ�
    int32_t sampleRecordQuality();

    // ¼�������ϱ�
    int32_t reportRecordQuality();

    // ���²������ϱ�����
    int32_t resetTraceInterval(uint32_t ulSampleInterval, uint32_t ulReportInterval);

    // ͨ���豸ID��ȡ�ɼ��豸����
    SVS_Record_Trace_Strategy* getDevice(const char * strDeviceId);

    // ͨ���ڲ�ID��ȡ�ɼ��豸����
    SVS_Record_Trace_Strategy *getDevice(uint32_t ulInternalId);

    // ��Ӳɼ��豸����
    int32_t addDevice(const char * strDeviceId, SVS_Record_Trace_Strategy * & pTraceDevice);

    // ��һ������ID
    uint32_t nextInternalId();

    // ����¼����������
    void sendRecordQuality(RECORD_QUALITY_REPORT_VECTOR &szRecordReportVct)const;

private:
    // ¼����������Զ�ʱ��������
    int32_t m_lSampleTimer;
    uint32_t m_ulSampleInterval;

    // ¼�������ϱ������Զ�ʱ��������
    int32_t m_lReportTimer;
    uint32_t m_ulReportInterval;

    // ��¼����ٵľ�ͷmap
    ACE_Recursive_Thread_Mutex m_deviceMapMutex;
    SVS_TRACE_DEVICE_MAP m_deviceMap;
    SVS_TRACE_DEVICE_INTERNAL_ID_MAP m_deviceTnternalIdMap;
    uint32_t m_ulNextInternalId; // �豸������ڲ�����id����ʱ���ص�ʱ��
};

#endif // __SVS_RECORD_TRACE_H__

