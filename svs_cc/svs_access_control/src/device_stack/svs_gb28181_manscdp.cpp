#include "svs_gb28181_manscdp.h"
#include "svs_log_msg.h"
#include "svs_access_control_manager.h"
#include "svs_acm_request.h"
#include "svs_device_stack_gb28181.h"

ACE_Atomic_Op<ACE_Thread_Mutex, uint32_t> CManscdp::m_nSN = 0;

int32_t CManscdp::parse(const char* pszXML)
{
    SVS_TRACE();

    TiXmlDocument objXmlDoc;

    try
    {
        objXmlDoc.Parse(pszXML);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse XML failed, content is %s.", pszXML));
        return SVS_ERROR_FAIL;
    }

    const TiXmlElement* pRoot = objXmlDoc.RootElement();
    if (NULL == pRoot)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse XML failed, content is %s.", pszXML));
        return SVS_ERROR_FAIL;
    }

    int32_t nResult = 0;

    do
    {
        if (0 == strcmp(pRoot->Value(), "Notify"))
        {
            nResult = parseNotify(*pRoot);
            break;
        }
        else if (0 == strcmp(pRoot->Value(), "Response"))
        {
            nResult = parseResponse(*pRoot);
            break;
        }
    }while(0);

    if (0 != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse XML failed, content is %s.", pszXML));
        return SVS_ERROR_FAIL;
    }

    return SVS_ERROR_OK;
}

int32_t CManscdp::parseNotify(const TiXmlElement &rRoot)
{
    SVS_TRACE();

    const TiXmlElement* pCmdType = rRoot.FirstChildElement("CmdType");
    if (NULL == pCmdType)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse notify failed. Can't find 'CmdType'."));
        return SVS_ERROR_FAIL;
    }

    SVS_LOG((SVS_LM_INFO, "Receive notify %s.", pCmdType->GetText()));

    if (0 == strcmp(pCmdType->GetText(), "Keepalive"))
    {
        return parseKeepAlive(rRoot);
    }
    else if (0 == strcmp(pCmdType->GetText(), "Alarm"))
    {
        return parseAlarm(rRoot);
    }

    return SVS_ERROR_OK;    
}

int32_t CManscdp::parseKeepAlive(const TiXmlElement &rRoot)
{
    SVS_TRACE();

    const TiXmlElement* pDeviceID = rRoot.FirstChildElement("DeviceID");
    if (NULL == pDeviceID)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse keep alive notify failed. Can't find 'DeviceID'."));
        return SVS_ERROR_FAIL;
    }
    if (NULL == m_pDeviceStackGB28181)
    {
        SVS_LOG((SVS_LM_ERROR, "Can't notfiy device keep alive, m_pDeviceStackGB28181 is NULL."));
        return SVS_ERROR_FAIL;
    }

    return m_pDeviceStackGB28181->notifyDeviceKeepAlive(pDeviceID->GetText());
}

int32_t CManscdp::parseAlarm(const TiXmlElement &rRoot)
{
    SVS_TRACE();

    const TiXmlElement* pDeviceID = rRoot.FirstChildElement("DeviceID");
    if (NULL == pDeviceID)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse alarm notify failed. Can't find 'DeviceID'."));
        return SVS_ERROR_FAIL;
    }

    const TiXmlElement* pAlarmPriority = rRoot.FirstChildElement("AlarmPriority");
    if (NULL == pAlarmPriority)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse alarm notify failed. Can't find 'AlarmPriority'."));
        return SVS_ERROR_FAIL;
    }

    const TiXmlElement* pAlarmMethod = rRoot.FirstChildElement("AlarmMethod");
    if (NULL == pAlarmMethod)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse alarm notify failed. Can't find 'AlarmMethod'."));
        return SVS_ERROR_FAIL;
    }

    const TiXmlElement* pAlarmTime = rRoot.FirstChildElement("AlarmTime");
    if (NULL == pAlarmTime)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse alarm notify failed. Can't find 'AlarmTime'."));
        return SVS_ERROR_FAIL;
    }

    SVS_ACM::REQUEST_NOTIFY_DEV_ALARM stNotify;
    strncpy(stNotify.stDevAlarmInfo.szDeviceID, pDeviceID->GetText(), sizeof(stNotify.stDevAlarmInfo.szDeviceID) - 1);
    stNotify.stDevAlarmInfo.strAlarmPriority    = pAlarmPriority->GetText();
    stNotify.stDevAlarmInfo.strAlarmMethod      = pAlarmMethod->GetText();
    stNotify.stDevAlarmInfo.strAlarmTime        = pAlarmTime->GetText();

    const TiXmlElement* pAlarmDescription = rRoot.FirstChildElement("AlarmDescription");
    if (NULL != pAlarmDescription)
    {
        stNotify.stDevAlarmInfo.strAlarmDescription = pAlarmDescription->GetText();
    }

    const TiXmlElement* pLongitude = rRoot.FirstChildElement("Longitude");
    if (NULL != pLongitude)
    {
        stNotify.stDevAlarmInfo.strLongitude = pLongitude->GetText();
    }

    const TiXmlElement* pLatitude = rRoot.FirstChildElement("Latitude");
    if (NULL != pLatitude)
    {
        stNotify.stDevAlarmInfo.strLatitude = pLatitude->GetText();
    }

    int32_t nResult = IAccessControlManager::instance().asyncRequest(stNotify);
    if (SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Notfiy device '%s' alarm failed.", stNotify.stDevAlarmInfo.szDeviceID));
        return nResult;
    }   

    SVS_LOG((SVS_LM_INFO, "Notfiy device '%s' alarm success.", stNotify.stDevAlarmInfo.szDeviceID));
    return SVS_ERROR_OK;
}

int32_t CManscdp::parseResponse(const TiXmlElement &rRoot)
{
    SVS_TRACE();

    const TiXmlElement* pCmdType = rRoot.FirstChildElement("CmdType");
    if (NULL == pCmdType)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse response failed. Can't find 'CmdType'."));
        return SVS_ERROR_FAIL;
    }

    SVS_LOG((SVS_LM_INFO, "Receive response %s.", pCmdType->GetText()));

    int32_t nResult = 0;
    if (0 == strcmp(pCmdType->GetText(), "Catalog"))
    {
        nResult = parseQueryCatalog(rRoot);
    }

    if (0 != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse response %s failed.", pCmdType->GetText()));
        return SVS_ERROR_FAIL;
    }

    return SVS_ERROR_OK;
}

int32_t CManscdp::parseQueryCatalog(const TiXmlElement &rRoot)
{
    SVS_TRACE();

    const TiXmlElement* pDeviceID = rRoot.FirstChildElement("DeviceID");
    if (NULL == pDeviceID)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse response failed. Can't find 'DeviceID'."));
        return SVS_ERROR_FAIL;
    }

    const TiXmlElement* pDeviceList = rRoot.FirstChildElement("DeviceList");
    if (NULL == pDeviceList)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse response failed. Can't find 'DeviceList'."));
        return SVS_ERROR_FAIL;
    }

    int32_t nDeviceNum = 0;
    int32_t nResult = pDeviceList->QueryIntAttribute("Num", &nDeviceNum);
    if (TIXML_SUCCESS != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse Num of DeviceList failed, error code is %d.", nResult));
        return SVS_ERROR_FAIL;
    }

    if (0 >= nDeviceNum)
    {
        SVS_LOG((SVS_LM_INFO, "Num of DeviceList is 0."));
        return SVS_ERROR_OK;
    }

    SVS_ACM::REQUEST_NOTIFY_DEV_INFO stRequest;
    SVS_ACM::DEVICE_INFO& stDeviceInfo = stRequest.stDeviceInfo;
    stDeviceInfo.eDeviceType = SVS_DEV_TYPE_GB28181;
    stDeviceInfo.eDeviceStatus = SVS_DEV_STATUS_ONLINE;
    strncpy(stDeviceInfo.szDeviceID, pDeviceID->GetText(), sizeof(stDeviceInfo.szDeviceID) - 1);
    int32_t nItemNum = 0;
    const TiXmlElement* pItem = pDeviceList->FirstChildElement("Item");
    while (NULL != pItem)
    {
        SVS_ACM::LENS_INFO stLensInfo;
        nResult = parseDeviceItem(*pItem, stLensInfo);
        if (0 != nResult)
        {
            SVS_LOG((SVS_LM_ERROR, "Parse item of device list failed."));
            return SVS_ERROR_FAIL;
        }

        strncpy(stLensInfo.szDeviceID, stDeviceInfo.szDeviceID, sizeof(stLensInfo.szDeviceID) - 1);
        stDeviceInfo.vecLensInfo.push_back(stLensInfo);

        nItemNum++;
        pItem = pItem->NextSiblingElement();
    }

    if (nDeviceNum != nItemNum)
    {
        SVS_LOG((SVS_LM_ERROR, "Real item num(%d) of device list is not the same as num(%d) of device list.",
            nItemNum, nDeviceNum));
        return SVS_ERROR_FAIL;
    }

    if (NULL == m_pDeviceStackGB28181)
    {
        SVS_LOG((SVS_LM_ERROR, "Can't notfiy device info, m_pDeviceStackGB28181 is NULL."));
        return SVS_ERROR_FAIL;
    }

    nResult = m_pDeviceStackGB28181->notifyDeviceOnline(stRequest);
    if (0 != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Notfiy device info failed."));
        return SVS_ERROR_FAIL;
    }

    return SVS_ERROR_OK;
}

int32_t CManscdp::parseDeviceItem(const TiXmlElement &rItem, SVS_ACM::LENS_INFO& rLensInfo)
{
    SVS_TRACE();

    const TiXmlElement* pDeviceID = rItem.FirstChildElement("DeviceID");
    if (NULL == pDeviceID)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse response failed. Can't find 'DeviceID' of 'Item'."));
        return SVS_ERROR_FAIL;
    }

    const TiXmlElement* pName = rItem.FirstChildElement("Name");
    if (NULL == pName)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse response failed. Can't find 'Name' of 'Item'."));
        return SVS_ERROR_FAIL;
    }

    const TiXmlElement* pManufacturer = rItem.FirstChildElement("Manufacturer");
    if (NULL == pManufacturer)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse response failed. Can't find 'Manufacturer' of 'Item'."));
        return SVS_ERROR_FAIL;
    }

    const TiXmlElement* pModel = rItem.FirstChildElement("Model");
    if (NULL == pModel)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse response failed. Can't find 'Model' of 'Item'."));
        return SVS_ERROR_FAIL;
    }

    const TiXmlElement* pStatus = rItem.FirstChildElement("Status");
    if (NULL == pStatus)
    {
        SVS_LOG((SVS_LM_ERROR, "Parse response failed. Can't find 'Status' of 'Item'."));
        return SVS_ERROR_FAIL;
    }

    rLensInfo.eLensType = SVS_DEV_TYPE_GB28181;
    rLensInfo.eLensStatus = (0 == strcmp(pStatus->GetText(), "ON"))
                                ? SVS_DEV_STATUS_ONLINE
                                : SVS_DEV_STATUS_OFFLINE;
    strncpy(rLensInfo.szLensID, pDeviceID->GetText(), sizeof(rLensInfo.szLensID) - 1);
    strncpy(rLensInfo.szLensName, pName->GetText(), sizeof(rLensInfo.szLensName) - 1);
    strncpy(rLensInfo.szManufacturer, pManufacturer->GetText(), sizeof(rLensInfo.szManufacturer) - 1);

    return SVS_ERROR_OK;
}

int32_t CManscdp::createQueryCatalog(const char* pszDeviceID)
{
    SVS_TRACE();
    try
    {
        TiXmlDeclaration * xmlDec = new TiXmlDeclaration("1.0", "UTF-8", "yes");
        m_objXmlDoc.LinkEndChild(xmlDec);

        TiXmlElement *xmlQuery = new TiXmlElement("Query");
        m_objXmlDoc.LinkEndChild(xmlQuery);

        TiXmlElement *xmlCmdType = new TiXmlElement("CmdType");
        xmlCmdType->LinkEndChild(new TiXmlText("Catalog"));
        xmlQuery->LinkEndChild(xmlCmdType);

        TiXmlElement *xmlSN = new TiXmlElement("SN");
        xmlSN->LinkEndChild(new TiXmlText(getSN()));
        xmlQuery->LinkEndChild(xmlSN);

        TiXmlElement *xmlDeviceID = new TiXmlElement("DeviceID");
        xmlDeviceID->LinkEndChild(new TiXmlText(pszDeviceID));
        xmlQuery->LinkEndChild(xmlDeviceID);

        /*
        TiXmlElement *xmlStartTime = new TiXmlElement("StartTime");
        xmlStartTime->LinkEndChild(new TiXmlText("2018-03-26T00:00:00"));
        xmlQuery->LinkEndChild(xmlStartTime);

        TiXmlElement *xmlEndTime = new TiXmlElement("EndTime");
        xmlEndTime->LinkEndChild(new TiXmlText("2018-04-26T00:00:00"));
        xmlQuery->LinkEndChild(xmlEndTime);
        */
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_ERROR, "Create query catalog xml failed."));
        return SVS_ERROR_FAIL;
    }

    return SVS_ERROR_OK;
}

int32_t CManscdp::createKeyFrame()
{
        SVS_TRACE();
    try
    {
        TiXmlDeclaration * xmlDec = new TiXmlDeclaration("1.0", "UTF-8", "yes");
        m_objXmlDoc.LinkEndChild(xmlDec);

        TiXmlElement *xmlControl = new TiXmlElement("Control");
        m_objXmlDoc.LinkEndChild(xmlControl);

        TiXmlElement *xmlIFrameCmd = new TiXmlElement("IFameCmd");
        xmlIFrameCmd->LinkEndChild(new TiXmlText("Send"));
        xmlControl->LinkEndChild(xmlIFrameCmd);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_ERROR, "Create key frame xml failed."));
        return SVS_ERROR_FAIL;
    }

    return SVS_ERROR_OK;
}

const char* CManscdp::getSN()
{
    uint32_t nSN = ++m_nSN;
    snprintf(m_szSN, sizeof(m_szSN), "%u", nSN);
    return m_szSN;
}

int32_t CManscdpDevCtrl::checkPTZCmd()
{
    SVS_TRACE();

    int32_t nResult = SVS_ERROR_OK;

    switch (m_rRequest.eCtrlType)
    {
    case SVS_ACM::DEV_CTRL_TYPE_ZOOM_IN:
    case SVS_ACM::DEV_CTRL_TYPE_ZOOM_OUT:
        {
            if (m_rRequest.nCtrlParam1 > DEV_CTRL_MAX_ZOOM_PARAM)
            {
                nResult = SVS_ERROR_FAIL;
                SVS_LOG((SVS_LM_ERROR, "Device control zoom param is %d, greater than %d.",
                    m_rRequest.nCtrlParam1, DEV_CTRL_MAX_ZOOM_PARAM));
            }
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_PREFAB_BIT_SET:
    case SVS_ACM::DEV_CTRL_TYPE_PREFAB_BIT_DEL:
    case SVS_ACM::DEV_CTRL_TYPE_PREFAB_BIT_RUN:
        {
            if (0 == m_rRequest.nCtrlParam1)
            {
                nResult = SVS_ERROR_FAIL;
                SVS_LOG((SVS_LM_ERROR, "Device control prefab bit param can't equal 0."));
            }
            break;
        }
    default:
        break;
    }

    return nResult;
}

int32_t CManscdpDevCtrl::setPTZCmd()
{
    SVS_TRACE();

    int32_t nResult = checkPTZCmd();
    if (SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Check device control param failed."));
        return nResult;
    }

    //初始化为Magic、Version和校验码
    unsigned char szPTZCmd[DEV_CTRL_POS_MAX] = {0xA5, 0x0F};
    switch (m_rRequest.eCtrlType)
    {
    case SVS_ACM::DEV_CTRL_TYPE_ZOOM_IN:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]          = DEV_CTRL_CMD_ZOOM_IN;
            szPTZCmd[DEV_CTRL_POS_DATA3_HIGH4]  = m_rRequest.nCtrlParam1 << 4;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_ZOOM_OUT:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]          = DEV_CTRL_CMD_ZOOM_OUT;
            szPTZCmd[DEV_CTRL_POS_DATA3_HIGH4]  = m_rRequest.nCtrlParam1 << 4;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_UP:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_UP;
            szPTZCmd[DEV_CTRL_POS_DATA2]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_DOWN:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_DOWN;
            szPTZCmd[DEV_CTRL_POS_DATA2]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_LEFT:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_LEFT;
            szPTZCmd[DEV_CTRL_POS_DATA1]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_RIGHT:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_RIGHT;
            szPTZCmd[DEV_CTRL_POS_DATA1]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_LEFT_UP:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_LEFT | DEV_CTRL_CMD_UP;
            szPTZCmd[DEV_CTRL_POS_DATA1]    = m_rRequest.nCtrlParam1;
            szPTZCmd[DEV_CTRL_POS_DATA2]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_LEFT_DOWN:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_LEFT | DEV_CTRL_CMD_DOWN;
            szPTZCmd[DEV_CTRL_POS_DATA1]    = m_rRequest.nCtrlParam1;
            szPTZCmd[DEV_CTRL_POS_DATA2]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_RIGHT_UP:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_RIGHT | DEV_CTRL_CMD_UP;
            szPTZCmd[DEV_CTRL_POS_DATA1]    = m_rRequest.nCtrlParam1;
            szPTZCmd[DEV_CTRL_POS_DATA2]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_RIGHT_DOWN:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_RIGHT | DEV_CTRL_CMD_DOWN;
            szPTZCmd[DEV_CTRL_POS_DATA1]    = m_rRequest.nCtrlParam1;
            szPTZCmd[DEV_CTRL_POS_DATA2]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_STOP:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_STOP;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_STOP_FI:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_STOP_FI;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_APERTURE_OPEN:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_APERTURE_OPEN;
            szPTZCmd[DEV_CTRL_POS_DATA2]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_APERTURE_CLOSE:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_APERTURE_CLOSE;
            szPTZCmd[DEV_CTRL_POS_DATA2]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_FOCAL_NEAR:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_FOCAL_NEAR;
            szPTZCmd[DEV_CTRL_POS_DATA1]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_FOCAL_FAR:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_FOCAL_FAR;
            szPTZCmd[DEV_CTRL_POS_DATA1]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_PREFAB_BIT_SET:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_PREFAB_BIT_SET;
            szPTZCmd[DEV_CTRL_POS_DATA2]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_PREFAB_BIT_DEL:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_PREFAB_BIT_DEL;
            szPTZCmd[DEV_CTRL_POS_DATA2]    = m_rRequest.nCtrlParam1;
            break;
        }
    case SVS_ACM::DEV_CTRL_TYPE_PREFAB_BIT_RUN:
        {
            szPTZCmd[DEV_CTRL_POS_CMD]      = DEV_CTRL_CMD_PREFAB_BIT_RUN;
            szPTZCmd[DEV_CTRL_POS_DATA2]    = m_rRequest.nCtrlParam1;
            break;
        }
    default:
        {
            SVS_LOG((SVS_LM_ERROR, "Set device control cmd failed, unsupported control type '%s'.", m_rRequest.eCtrlType));
            return SVS_ERROR_FAIL;
            //break;
        }
    }

    //生成校验码
    for (int32_t i = 0; i < DEV_CTRL_POS_CHECK_CODE; i++)
    {
        szPTZCmd[DEV_CTRL_POS_CHECK_CODE] += szPTZCmd[i];
    }

    //转成16进制
    for (int32_t i = 0; i < DEV_CTRL_POS_MAX; i++)
    {
        snprintf(m_szPTZCmd + i * 2, 3, "%02X", szPTZCmd[i]);
    }

    return SVS_ERROR_OK;
}

int32_t CManscdpDevCtrl::createDeviceControl()
{
    SVS_TRACE();

    int32_t nResult = setPTZCmd();
    if (SVS_ERROR_OK != nResult)
    {
        SVS_LOG((SVS_LM_ERROR, "Check device control param failed."));
        return nResult;
    }

    try
    {
        TiXmlDeclaration * xmlDec = new TiXmlDeclaration("1.0", "UTF-8", "yes");
        m_objXmlDoc.LinkEndChild(xmlDec);

        TiXmlElement *xmlQuery = new TiXmlElement("Control");
        m_objXmlDoc.LinkEndChild(xmlQuery);

        TiXmlElement *xmlCmdType = new TiXmlElement("CmdType");
        xmlCmdType->LinkEndChild(new TiXmlText("DeviceControl"));
        xmlQuery->LinkEndChild(xmlCmdType);

        TiXmlElement *xmlSN = new TiXmlElement("SN");
        xmlSN->LinkEndChild(new TiXmlText(getSN()));
        xmlQuery->LinkEndChild(xmlSN);

        TiXmlElement *xmlDeviceID = new TiXmlElement("DeviceID");
        xmlDeviceID->LinkEndChild(new TiXmlText(m_rRequest.szLensID));
        xmlQuery->LinkEndChild(xmlDeviceID);

        TiXmlElement *xmlPTZCmd = new TiXmlElement("PTZCmd");
        xmlPTZCmd->LinkEndChild(new TiXmlText(m_szPTZCmd));
        xmlQuery->LinkEndChild(xmlPTZCmd);

        TiXmlElement *xmlInfo = new TiXmlElement("Info");
        xmlQuery->LinkEndChild(xmlInfo);

        char szControlPriority[11]; //uint32_t的最大长度
        snprintf(szControlPriority, sizeof(szControlPriority), "%u", m_rRequest.nPriority);
        TiXmlElement *xmlPriority = new TiXmlElement("ControlPriority");
        xmlPriority->LinkEndChild(new TiXmlText(szControlPriority));
        xmlInfo->LinkEndChild(xmlPriority);
    }
    catch(...)
    {
        SVS_LOG((SVS_LM_ERROR, "Create device control xml failed."));
        return SVS_ERROR_FAIL;
    }

    return SVS_ERROR_OK;
}
