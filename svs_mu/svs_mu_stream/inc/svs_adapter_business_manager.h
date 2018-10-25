/*
 * svs_adapter_business_manager.h
 *
 *  Created on: 2016-1-21
 *      Author:
 */

#ifndef STREAMBUSINESSMANAGER_H_
#define STREAMBUSINESSMANAGER_H_

#include <map>
#include "svs_ace_header.h"
#include "svs_adapter_business.h"

class CSessionFluxTimer:public ACE_Event_Handler
{
public:
    virtual int32_t handle_timeout(const ACE_Time_Value &tv, const void *arg);
};


typedef std::multimap<uint64_svs, CStreamBusiness*>           STREAM_BUSINESS_MAP;
typedef std::multimap<uint64_svs, CStreamBusiness*>::iterator STREAM_BUSINESS_MAP_ITER;

typedef std::list<CStreamBusiness*>                           BUSINESS_LIST;
typedef std::list<CStreamBusiness*>::iterator                 BUSINESS_LIST_ITER;



class CStreamBusinessManager
{
public:
    virtual ~CStreamBusinessManager();

    static CStreamBusinessManager* instance();

    int32_t initManager();

    void closeManager();

    CStreamBusiness* createBusiness(uint64_svs recvSessionId,
                                 uint64_svs sendSessionId,
                                 PLAY_TYPE enPlayType);

    CStreamBusiness* findBusiness(uint64_svs streamID);

    void findBusiness(uint64_svs streamID, BUSINESS_LIST &list);

    uint32_t getAttachedBusinessCount(uint64_svs streamID);

    void getAllBusiness(BUSINESS_LIST& businessList);

    void releaseBusiness(CStreamBusiness* &pBusiness);

    void releaseBusiness(BUSINESS_LIST &list);

    uint32_t getBusinessCount();

    uint32_t getBusinessCount(uint64_svs ullStreamId);

    void statFlux();

    uint32_t getInputRate()const;

    uint32_t getOutputRate()const;

private:
    CStreamBusinessManager();

    void destroyBusiness(CStreamBusiness *&pBusiness);

    void startFluxTimer();

    void stopFluxTimer();

    bool isBusinessInList(const CStreamBusiness *pBusiness, BUSINESS_LIST& businessList)const;
private:
    static CStreamBusinessManager*  m_pStreamBusinessManager;

    STREAM_BUSINESS_MAP          m_BusinessMap;
    ACE_Thread_Mutex             m_BusinessMapMutex;

    uint32_t                     m_unMaxBusinessNum;
    CSessionFluxTimer*           m_pFluxTimer;
    uint32_t                     m_InputRate;            // Mbps
    uint32_t                     m_OutputRate;           // Mbps
};

#endif /* STREAMBUSINESSMANAGER_H_ */
