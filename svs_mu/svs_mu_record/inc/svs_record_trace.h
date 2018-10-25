/******************************************************************************

                  版权所有 (C), 2001-2011, 奥看信息技术有限公司

 ******************************************************************************
  文 件 名   : SVS_Record_Trace.h
  版 本 号   : 初稿
  生成日期   : 2010年3月10日
  最近修改   :
  功能描述   : 录像质量跟踪类。根据录像跟踪策略，周期性采集录像质量，并周期性上报
               到控制台。
  函数列表   :
  修改历史   :
  1.日    期   : 2010年3月10日
    修改内容   : 创建文件

******************************************************************************/
#ifndef __SVS_RECORD_TRACE_H__
#define __SVS_RECORD_TRACE_H__

// 定时器类别
enum
{
    SVS_TRACE_TIMER_TYPE_STRATEGY   = 1,
    SVS_TRACE_TIMER_TYPE_SAMPLE     = 2,
    SVS_TRACE_TIMER_TYPE_REPORT     = 3,
};

typedef ACE_Vector <SVS_RECORD_QUALITY_REPORT> RECORD_QUALITY_REPORT_VECTOR;

// 策略id和录像跟踪策略map
typedef ACE_Hash_Map_Manager < ACE_CString, SVS_RECORD_TRACE_POLICY *, ACE_Null_Mutex >
    SVS_TRACE_POLICY_MAP;

// 镜头id和录像设备对象的容器
typedef ACE_Hash_Map_Iterator < ACE_CString, SVS_RECORD_TRACE_POLICY *, ACE_Null_Mutex >
    SVS_TRACE_POLICY_MAP_ITER;

typedef ACE_Hash_Map_Entry < ACE_CString, SVS_RECORD_TRACE_POLICY * >
    SVS_TRACE_POLICY_MAP_ENTRY;

// 录像跟踪策略类。每个镜头对应一个对象。
class SVS_Record_Trace_Strategy
{
public:
    SVS_Record_Trace_Strategy(const char* strDeviceId, uint32_t interalId);
    virtual ~SVS_Record_Trace_Strategy();

    // 重置，资源复位
    UTAPI int32_t reset();

    // 同步录像跟踪策略
    UTAPI int32_t syncTraceStrategy(const SVS_RECORD_TRACE_POLICY* pTracePolicy);

    // 录像质量采集
    UTAPI int32_t sampleRecordQuality();

    // 录像质量上报
    UTAPI int32_t getRecordQualityReports(RECORD_QUALITY_REPORT_VECTOR &szRecordReportVct);

    // 策略是否都已经过期
    UTAPI int32_t isTraceFinished();

    // 递增应用计数
    UTAPI int32_t increaseReference();

    // 递减引用计数，如果递减后，引用计数等于０，且路由已经停止，那么可以删除次对象
    UTAPI int32_t decreaseReference();

    // 获取镜头的id
    UTAPI const char* deviceId()const;

    // 策略筛选
    UTAPI int32_t selectTraceStrategy();

private:
    // 添加录像跟踪策略
    UTAPI int32_t addTraceStrategy(const SVS_RECORD_TRACE_POLICY* pTracePolicy);

    // 更新录像跟踪策略
    UTAPI int32_t modTraceStrategy(const SVS_RECORD_TRACE_POLICY* pTracePolicy);

    // 删除录像跟踪策略
    UTAPI int32_t delTraceStrategy(const char* strStrategyId);

    // 发送跟踪策略完成通知消息
    UTAPI void sendStrategyDoneNotifyMsg(const char * strStrategyId)const;

    // 跟踪策略刚刚生效，复位采样数据
    UTAPI void resetSampleInfo(bool bOldSampleFlag, bool bNewSampleFlag);

private:
    // 采样数据列表
    RECORD_QUALITY_REPORT_VECTOR m_szReportVct;

    // 镜头ID
    char m_strDeviceId[LOCALIZE_DEVICE_ID_MAX_LEN + 1];

    // 录像跟踪策略map
    SVS_TRACE_POLICY_MAP m_strategyMap;
    ACE_Recursive_Thread_Mutex m_mutex;

    // 引用计数
    uint32_t m_ulRefCount;

    // 策略定时器
    int32_t m_lStrategyTimer;

    // 内部id
    uint32_t m_ulInternalId;

    // 是否需要采集的标示
    bool m_bSampleFlag;

    // 本录像采集周期的起始时间
    time_t m_startSampleTime;
private:
#ifdef UNITTEST
    friend class SVS_Record_Trace_Strategy_Test;
#endif
};


// 镜头id和录像跟踪设备对象的map
typedef ACE_Hash_Map_Manager < ACE_CString, SVS_Record_Trace_Strategy *, ACE_Null_Mutex >
    SVS_TRACE_DEVICE_MAP;

// 内部ID和录像跟踪设备对象的map
typedef ACE_Hash_Map_Manager <uint32_t, SVS_Record_Trace_Strategy *, ACE_Null_Mutex>
    SVS_TRACE_DEVICE_INTERNAL_ID_MAP;

// 镜头id和录像设备对象的容器
typedef ACE_Hash_Map_Iterator < ACE_CString, SVS_Record_Trace_Strategy *, ACE_Null_Mutex >
    SVS_TRACE_DEVICE_MAP_ITER;

typedef ACE_Hash_Map_Entry < ACE_CString, SVS_Record_Trace_Strategy * >
    SVS_TRACE_DEVICE_MAP_ENTRY;

// 录像跟踪策略管理类。
class SVS_Record_Trace_Manager
{
public:
    virtual ~SVS_Record_Trace_Manager();

    // 单例
    static SVS_Record_Trace_Manager *instance()
    {
        static SVS_Record_Trace_Manager szTraceManager;
        return &szTraceManager;
    }

    // 策略更新
    int32_t syncRecordTraceStrategy(const ACE_Message_Block* mb);

    // 定时器回调函数
    static void timerCallback(  void *pArg, int32_t nEventId, int32_t lTimerId, int32_t nDummy );

    // 定时器时间处理
    int32_t onTimer(int32_t nEventId, int32_t lTimerId, int32_t nDummy);

     // 将指定对象的引用计数减１，如果此对象已经为停止状态且引用计数为０，那么将销毁此对象。
    int32_t decrease_reference(SVS_Record_Trace_Strategy *pDevice);

private:
    // 单例模式的类，构造函数隐藏
    SVS_Record_Trace_Manager();

    // 录像质量采集
    int32_t sampleRecordQuality();

    // 录像质量上报
    int32_t reportRecordQuality();

    // 更新采样和上报周期
    int32_t resetTraceInterval(uint32_t ulSampleInterval, uint32_t ulReportInterval);

    // 通过设备ID获取采集设备对象
    SVS_Record_Trace_Strategy* getDevice(const char * strDeviceId);

    // 通过内部ID获取采集设备对象
    SVS_Record_Trace_Strategy *getDevice(uint32_t ulInternalId);

    // 添加采集设备对象
    int32_t addDevice(const char * strDeviceId, SVS_Record_Trace_Strategy * & pTraceDevice);

    // 下一个内容ID
    uint32_t nextInternalId();

    // 发送录像质量报告
    void sendRecordQuality(RECORD_QUALITY_REPORT_VECTOR &szRecordReportVct)const;

private:
    // 录像采样周期性定时器和周期
    int32_t m_lSampleTimer;
    uint32_t m_ulSampleInterval;

    // 录像质量上报周期性定时器和周期
    int32_t m_lReportTimer;
    uint32_t m_ulReportInterval;

    // 被录像跟踪的镜头map
    ACE_Recursive_Thread_Mutex m_deviceMapMutex;
    SVS_TRACE_DEVICE_MAP m_deviceMap;
    SVS_TRACE_DEVICE_INTERNAL_ID_MAP m_deviceTnternalIdMap;
    uint32_t m_ulNextInternalId; // 设备对象的内部管理id，定时器回调时用
};

#endif // __SVS_RECORD_TRACE_H__

