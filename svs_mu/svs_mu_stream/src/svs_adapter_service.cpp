/**
* @file    MduService.cpp
* @brief   Mdu���������ڣ����������������
*
* Copyright (c) 2010 AllCam Tech.Co.,Ltd
*
* @date      2010-12-11
*/
#ifdef UNITTEST
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#endif

#include "svs_ace_header.h"
#include "svs_log_msg.h"
#include "svs_utility.h"
#include "svs_logging_strategy.h"
#include "svs_config.h"

#include "svs_adapter_svs_retcode.h"
#include "svs_adapter_def.h"
#include "svs_adapter_config.h"
#include "svs_adapter_media_data_queue.h"
#include "svs_adapter_media_block_buffer.h"
#include "svs_adapter_service.h"
#include "svs_adapter_port_manager.h"
#include "svs_adapter_net_conn_manager.h"
#include "svs_adapter_session.h"
#include "svs_adapter_session_factory.h"
#include "svs_adapter_media_exchange.h"
#include "svs_daemon_thread.h"
#include "svs_adapter_media_processor.h"
#include <svs_media_nat_mgr.h>


/**
* Description:  ��ȡ�������ʵ�ֺ�����Main����
* @return       char *: ����������ַ�
*/

CSVS_Logging_Strategy g_svrLogStrategy;

const char *getServiceName()
{
    return (char*)SVS_MDU_SERVICE_NAME;
}

void startService(void)
{
    (void)SVS_SS_UTILITIES::backup_log(SVS_MDU_LOG_PATH);
    int32_t initLogStrategyResult =
        g_svrLogStrategy.initLogStrategy( SVS_MDU_CONFIG_FILE, SVS_MDU_LOG_PATH, true );
    if ( RET_OK != initLogStrategyResult )
    {
        SVS_LOG((SVS_LM_CRITICAL, "Initialize log strategy failed."));
        return ;
    }
    int32_t nRet = CMduService::instance()->openService();
    if ( RET_OK != nRet )
    {
        SVS_LOG((SVS_LM_CRITICAL, "open service failed."));
        return ;
    }
    return;
}


void stopService()
{
     int32_t nRet = CMduService::instance()->closeService();
     if ( RET_OK != nRet )
    {
        SVS_LOG((SVS_LM_CRITICAL, "stop service failed."));
        return ;
    }
     return;
}



CMduService::CMduService()
{
    m_bRunFlag      = true;
}

CMduService::~CMduService()
{
}

int32_t CMduService::openService()
{
    SVS_LOG((SVS_LM_INFO,"start svs_mdu service."));

    m_bRunFlag = true;

    int32_t runType   = 0;
    int32_t logEnable = 0;
    int32_t logLM = 0;

    if (0 != getDebugConfig((char*)SVS_MDU_CONFIG_FILE, runType, logEnable, logLM))
    {
        SVS_LOG(( SVS_LM_CRITICAL,
                    "Get debug info from config file failed."));
        return 0;
    }

    //������־�ȼ�
    SVS_LOG_LM( logLM );

    //�򿪣��ر���־����
    if (logEnable)
    {
        SVS_LOG_ENABLE();
    }
    else
    {
        SVS_LOG_DISABLE();
    }

    int32_t nRet = CMduConfig::instance()->init(SVS_MDU_CONFIG_FILE);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"init mdu config fail."));
        return RET_FAIL;
    }

    nRet = CSVS_Daemon_Thread::instance()->Init(CMduConfig::instance()->getDebugCheckThreadInterval(),
                            CMduConfig::instance()->getDebugRestartServerFlag(),
                            CMduConfig::instance()->getDebugDumpServerFlag());
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"init daemon thread fail."));
        return RET_FAIL;
    }

    uint32_t unBlockCnt = CMduConfig::instance()->getServiceCapacity() * MDU_BLOCK_NUM_PER_CHANNEL;
    nRet = CMediaBlockBuffer::instance().init(MDU_MSG_BLOCK_SIZE, unBlockCnt);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"init media block buffer fail."));
        return RET_FAIL;
    }

    nRet = CNetConnManager::instance()->openManager(MDU_MEDIA_RECV_THREAD_NUM);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"init mdu connect manager fail."));
        return RET_FAIL;
    }

    nRet = CMduPortManager::instance()->initManager();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"init mdu port manager fail."));
        return RET_FAIL;
    }

    nRet = CMduSessionFactory::instance()->init();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"init mdu session factory fail."));
        return RET_FAIL;
    }

    nRet = CMduMediaExchange::instance()->Init(MDU_EXCHANGE_THREAD_NUM);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"init mdu meida exchange fail."));
        return RET_FAIL;
    }



    nRet = CMduServiceTask::instance()->openServiceTask();
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"open adapter service task fail."));
        return RET_FAIL;
    }

    //start the nat info configure task
    ACE_Reactor *pReactor = CMduServiceTask::instance()->getTimerReactor();
    if (NULL == pReactor)
    {
        return RET_FAIL;
    }

    nRet = CMduNatMgr::GetInstance().Open(SVS_MDU_CONFIG_FILE,pReactor);
    if (RET_OK != nRet)
    {
        SVS_LOG((SVS_LM_CRITICAL,"open adapter nat configure manager fail."));
        return RET_FAIL;
    }

    nRet = CMduConfig::instance()->startReloadTimer();

    SVS_LOG((SVS_LM_WARNING,"start mdu reload config timer ret[%d].", nRet));

    while (m_bRunFlag)
    {
        sleep(1);
    }
    return RET_OK;
}

int32_t CMduService::closeService()
{
    SVS_LOG((SVS_LM_INFO,"stop svs_mdu service."));

    m_bRunFlag = false;

    // ����open�෴��˳��������ģ���˳�
    // �ر������ļ�ˢ�¶�ʱ��
    CMduConfig::instance()->stopReloadTimer();

    // �˳�ServiceTask
    CMduServiceTask::instance()->closeServiceTask();

    // �ر����ж˿�
    CMduPortManager::instance()->closeManager();

    // �ر����ӹ�����
    (void)CNetConnManager::instance()->closeManager();

    // ����ڴ��
    CMediaBlockBuffer::instance().close();

    return RET_OK;
}

#ifdef UNITTEST

int32_t utmain(int32_t argc, char* argv[])
{
    //����InitGoogleTest����֤�����в�������
    testing::GTEST_FLAG(output) = "xml:gtest_report.xml";
    //�����쳣
    testing::GTEST_FLAG(catch_exceptions) = true;

    string strServerName =  "ut_" ;
    strServerName += SVS_MDU_SERVICE_NAME;
    string strLogFile = "ut_";
    strLogFile += SVS_MDU_LOG_PATH;

    // ��ʼ����־ģ��
    (void)CSVS_Log_Manager::instance().initLog(strServerName.c_str(),
                                               strLogFile.c_str(),
                                               SVS_MDU_CONFIG_FILE);

    testing::InitGoogleTest(&argc, argv);
    int32_t iRet = RUN_ALL_TESTS();

    // �ر���־
    (void)CSVS_Log_Manager::instance().endLog();

    return iRet;
}
#endif


