/*****************************************************************************
   版权所有 (C), 2001-2011, 奥看信息技术有限公司
 ******************************************************************************
  文件名          : SVS_Bakup_Front_Record_Lens.h
  版本号          : 1.0
  生成日期        : 2008-8-15
  最近修改        :
  功能描述        : 前端录像备份镜头管理类和备份镜头类，管理类实现对每个备份镜头
                    对象的管理，备份镜头负责根据备份任务结合平台和前端录像索引的
                    重叠情况，通过录像回放请求把前端录像备份到平台的磁盘阵列上。
  函数列表        :
  修改历史        :
  1 日期          : 2008-8-15
    修改内容      : 生成 *******************************************************************************/
#ifndef _SVS_BAKUP_FRONT_RECORD_LENS_H_
#define _SVS_BAKUP_FRONT_RECORD_LENS_H_

// 备份任务中每次需要备份的段最大数，暂定
#define RECORDBAK_SECTION_MAX_NUM 255
typedef struct RTSECTIONINFO_
{
    time_t StartTime;
    time_t EndTime;
} RTSECTIONINFO, *PRTSECTIONINFO;

typedef ACE_Vector <RTSECTIONINFO> SECTIONINFO_VECTOR;

// 前端录像备份设备的状态
enum BAKUP_DEVICE_STATE
{
    RECORDBAK_STATE_INIT = 0,        //初始化状态
    RECORDBAK_STATE_SLEEP,
    RECORDBAK_STATE_REQUEST_ROUTER, //请求调度子系统分配VTDU
    RECORDBAK_STATE_CONNECT_VTDU,   //连接VTDU，通知接收端口
    RECORDBAK_STATE_RECEIVING_FILE, //接收文件流
    RECORDBAK_STATE_WAITING_OVER,   //等待结束
};

// 前端录像备份设备类
class CAC_Bakup_Record_Dev : public CSVS_Record_Lens
{
public:
    enum
    {
        TIMER_TYPE_NORMAL = 1,
        TIMER_TYPE_CHECK_STATUS = 2,
    };
    CAC_Bakup_Record_Dev(const char *strLensId, const uint32_t nIndex );
    virtual ~CAC_Bakup_Record_Dev();
    // --------------------设备管理类调用接口-----------------------------
    // 初始化
    UTAPI bool init_device(const ACE_Message_Block *mb);

    // 停止备份前端录像
    UTAPI int32_t  stop_backup_record(bool procRouter = true);

    // 路由请求
    UTAPI int32_t  request_router();

    // 停止路由
    UTAPI int32_t  shutdown_router();

    // 路由消息处理
    UTAPI int32_t  handle_routerinfo(const void *pbyMsg);

    // 镜头对象复位
    UTAPI int32_t  reset();

    UTAPI void on_timer(int32_t eventId, int32_t timerId ,int32_t nDummy);

    // 获取调试信息
    UTAPI int32_t debug_get_device_info(char * strDeviceInfo, uint32_t ulBuffLen);

    // 获取设备状态
    UTAPI uint8_t get_state() const
    {
        return state_;
    }

    // 获取路由ID
    UTAPI routerid_t get_router_id()  const
    {
        return router_id_;
    }

public:
    // 处理结束包
    UTAPI int32_t proc_eos_packet();
    int32_t play_media_request();

    UTAPI uint16_t get_router_msgno() const
    {
        return router_msgno_;
    }

    // 回放下一段
    UTAPI int32_t drop_next_section();

private:
    // 向VTDU发送注册消息
    UTAPI int32_t  start_backup_record();
    // 更新前端录像备份索引信息到db

    // 设置备份状态
    UTAPI int32_t set_status(uint8_t newStatus);

    // 请求路由定时器任务
    UTAPI void request_router_on_timer();

    // 连接VTDU定时器任务
    UTAPI void conn_vtdu_on_timer();

    // 接收流状体的定时器任务
    UTAPI void revc_stream_on_timer();

    // 等待录像结束定时器
    UTAPI void sleep_on_timer();

    // 获取设备状态--输出参数 string
    UTAPI void debug_get_device_status(string & strDeviceStatus)const;
private:
    //UTAPI void parse_flat_record_index(DTRecordIndex &stRecordIndex);

    // 查询平台录像索引
    //UTAPI int32_t  query_record_info();

    // 生成备份策略
    //UTAPI int32_t  build_recordbak_strategy();

    // 筛选掉与指点的平台索引重叠的前端索引信息
    //UTAPI void filter_section(const RTSECTIONINFO& sectInfo, SECTIONINFO_VECTOR &resultSectVector);

    //UTAPI int32_t  bubble_up_sort_sectioninfo(SECTIONINFO_VECTOR & vecSectionInfo) const;

    // 屏蔽掉前端重复的索引段信息
    UTAPI void delete_overlap_sections(SECTIONINFO_VECTOR & sectVector)const;

    // 拼接连续的索引段
    //UTAPI void join_replay_sections(SECTIONINFO_VECTOR & sectVector) const;

    // 发送停止结束流消息
    UTAPI int32_t send_stop_recv_msg();
public:

    enum RECORD_REPLAR_TYPE
    {
        RECORD_REPLAR_TYPE_FRONT = 0x04    // 前端录像回放
    };
private:
    CAC_Bakup_Record_Dev();
private:
    // =======================BEING 镜头对象特性成员============================
    // 路由ID
    routerid_t router_id_;

    // 请求路由消息对应的消息ID号
    uint16_t router_msgno_;

    //VTDU地址
    ACE_INET_Addr vtdu_addr_;

    uint32_t    m_ulIOHandlerIndex;

    // 当前录像文件
    ACE_FILE_IO record_file_;

    // 当前状态的起始时间
    time_t status_start_time_;

    // =======================BEGIN 备份特殊成员================================
    // 当前正在回放的section段
    uint32_t cur_replay_sect_index_;

    // 接收到第一帧时系统时间和当前所要回放的录像段起始时间差值
    time_t time_remove_value;

    // 需要回放请求的section信息
    SECTIONINFO_VECTOR replay_sections_vector_;

    // 平台索引段section信息
    //SECTIONINFO_VECTOR flat_sections_vector_;// 为单元测试增加友元类
#ifdef UNITTEST
    friend class TEST_CAC_Bakup_Record_Dev;
#endif
};

// 前端录像备份设备管理类
class CAC_Bakup_Record_Dev_Manager
{
public:
    // 单例
    static CAC_Bakup_Record_Dev_Manager *instance()
    {
        static CAC_Bakup_Record_Dev_Manager manager;
        return &manager;
    }

    // 初始化函数，nMaxLens指定录像最大镜头数，包括实时录像和备份镜头
    bool init(uint32_t nMaxLens);

    // 当前正在前端录像备份的镜头数
    uint32_t get_bakup_device_num()
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, 0);
        return bakup_device_map_.current_size();
    }

    int32_t  decrease_reference(CAC_Bakup_Record_Dev *pBakDevice);

    // 备份任务通知
    int32_t  bakup_record_notify(const ACE_Message_Block *mb);

    // 前端录像回放路由处理
    int32_t  handle_routerinfo(const ACE_Message_Block *mb);

    int32_t add_msgno_map(CAC_Bakup_Record_Dev* bakDevice, uint16_t msgNo)
    {
        // 加锁，把对象加入map
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, 0);
        return bakup_device_msgno_map_.bind(msgNo, bakDevice);
    }

    int32_t delete_msgno_map(uint16_t msgNo)
    {
        ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, pObj, map_mutex_, 0);
        return bakup_device_msgno_map_.unbind(msgNo);
    }

    // 删除指定ID的备份镜头对象
    int32_t  stop_device(const char *strDevId);

    // 删除指定路由ID的镜头
    int32_t  stop_device(const uint32_t routerId);

    // 停止所有备份任务
    int32_t  stop_all_device();

    // 处理结束包
    int32_t  proc_eos_packet(uint32_t streamId);
    int32_t play_media_request(uint32_t streamId);


    // 定时器到期后的处理函数
    int32_t on_timer(int32_t eventId, int32_t timerId, int32_t nDummy);

    int32_t debug_all_bakup_device_info(char* debugBuf,
                                                const uint32_t bufLen);

    static void timer_callback(void *pArg, int32_t eventId, int32_t timerId, int32_t nDummy );

    int32_t debug_get_device_info(const char * strFrontDevId,
                                    char * strDeviceInfo,
                                    uint32_t szDeviceInfo);

    // 查询镜头是否在前端录像备份
    bool is_lens_bakup_record(const char* strLensId);

    int32_t update_backup_device_flag();
protected:
    // 构造函数
    CAC_Bakup_Record_Dev_Manager();

    // 析构函数
    ~CAC_Bakup_Record_Dev_Manager();

    // 添加备份镜头对象
    CAC_Bakup_Record_Dev * add_device( const char *strDevId );

    // 根据镜头ID获取备份镜头对象
    CAC_Bakup_Record_Dev * get_device(const char* strDevId);

    // 根据内部ID获取设备镜头对象
    CAC_Bakup_Record_Dev * get_device(const uint32_t RouterId);

    // 根据路由ID获取设备镜头对象
    CAC_Bakup_Record_Dev * get_device_by_routerid(const uint32_t RouterId);

    // 根据消息号获取备份镜头对象
    CAC_Bakup_Record_Dev * get_device_by_msgno(uint16_t msgNo);
protected:
    typedef ACE_Hash_Map_Manager < const char *, CAC_Bakup_Record_Dev *, ACE_Null_Mutex >
    BAKUP_DEV_MAP;

   typedef ACE_Hash_Map_Manager < uint16_t, CAC_Bakup_Record_Dev *, ACE_Null_Mutex >
    BAKUP_DEV_MSGNO_MAP;

   typedef ACE_Hash_Map_Iterator < const char *, CAC_Bakup_Record_Dev *, ACE_Null_Mutex >
    BAKUP_DEV_MAP_ITER;

    typedef ACE_Hash_Map_Entry < const char *, CAC_Bakup_Record_Dev * >
    BAKUP_DEV_MAP_ENTRY;

    typedef ACE_Hash_Map_Manager < ACE_CString, time_t, ACE_Null_Mutex > BACKUP_TIME_MAP;

    typedef ACE_Hash_Map_Iterator < ACE_CString, time_t, ACE_Null_Mutex > BACKUP_TIME_MAP_ITER;

    typedef ACE_Hash_Map_Entry < ACE_CString, time_t> BACKUP_TIME_MAP_ENTRY;

    // 最大备份镜头数
    uint32_t max_bakup_device_;

    // 备份镜头ID与备份对象映射表
    BAKUP_DEV_MAP bakup_device_map_;

    // 消息传输号和备份镜头对象映射表
    BAKUP_DEV_MSGNO_MAP bakup_device_msgno_map_;

    vector <CAC_Bakup_Record_Dev *> using_vector_;

    ACE_DLList <char> free_list_;

    // 镜头对象锁
    ACE_Recursive_Thread_Mutex map_mutex_;
    BACKUP_TIME_MAP backup_time_map_;

// 为单元测试增加友元类
#ifdef UNITTEST
    friend class TEST_CAC_Bakup_Record_Dev_Manager;
#endif
};
#endif // _SVS_BAKUP_FRONT_RECORD_LENS_H_


