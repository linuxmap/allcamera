#ifndef _ace_header_h
#define _ace_header_h

#include <ace/OS.h>
#include <ace/Event_Handler.h>
#include <ace/ACE.h>
#include <ace/Auto_Ptr.h>
#include <ace/FILE_IO.h>
#include <ace/Log_Msg.h>
#include <ace/Sched_Params.h>
#include <ace/Signal.h>
#include <ace/Thread_Manager.h>
#include <ace/INET_Addr.h>
#include <ace/SOCK_Acceptor.h>
#include <ace/SOCK_Stream.h>
#include <ace/SOCK_Dgram.h>
#include <ace/Log_Record.h>
#include <ace/Handle_Set.h>
#include <ace/Timer_Queue.h>
#include <ace/OS_NS_sys_time.h>
#include <ace/Select_Reactor.h>
#include <ace/Reactor.h>
#include <ace/Timer_Heap.h>
#include <ace/FILE_Connector.h>
#include <ace/Hash_Map_Manager_T.h>
#include <ace/Synch.h>
#include <ace/SOCK_Connector.h>
#include <ace/Svc_Handler.h>
#include <ace/Connector.h>
#include <ace/Acceptor.h>
#include <ace/Vector_T.h>
#include <ace/Auto_Ptr.h>
#include <ace/FIFO_Recv.h>
#include <ace/TSS_T.h>
#include <ace/Reactor_Notification_Strategy.h>
#include <ace/Get_Opt.h>
#include <ace/Date_Time.h>
#include <ace/Time_Value.h>
#include <ace/High_Res_Timer.h>
#include <ace/Message_Block.h>
#include <ace/Message_Queue.h>
#include <ace/Basic_Types.h>
#include <ace/OS_NS_string.h>
#include <ace/Recursive_Thread_Mutex.h>
#include <ace/OS_NS_stdio.h>
#include <ace/OS_NS_strings.h>

#include <ace/Asynch_Acceptor.h>
#include <ace/Asynch_Connector.h>
#include <ace/Asynch_IO.h>
#include <ace/CDR_Stream.h>
#include <ace/Unbounded_Set.h>
#include <ace/Proactor.h>

#include "ace/Service_Config.h"
#include "ace/Proactor.h"
#include "ace/Asynch_IO.h"
#include "ace/Asynch_IO_Impl.h"
#include "ace/Asynch_Acceptor.h"
#include "ace/INET_Addr.h"
#include "ace/RW_Mutex.h"
#include <ace/streams.h>
#include <ace/Get_Opt.h>
#include "ace/OS_NS_sys_stat.h"
#include <ace/Strategies_T.h>

#include "ace/String_Base.h"
#include "ace/Null_Mutex.h"
#include "ace/Thread_Mutex.h"
#include "ace/SString.h"

#include <ace/Task.h>
#include <ace/Lock_Adapter_T.h>
#include <ace/Task_Ex_T.h>
#include <ace/Thread_Hook.h>

#include <ace/File_Lock.h>
#include <ace/TP_Reactor.h>
#include <ace/Dev_Poll_Reactor.h>
#include <ace/Tokenizer_T.h>


#include <ACEXML/common/CharStream.h>
#include <ACEXML/common/StrCharStream.h>
#include <ACEXML/common/DefaultHandler.h>
#include <ACEXML/common/InputSource.h>
#include <ACEXML/parser/parser/Parser.h>

#include <string>
#include <iostream>

#ifdef WIN32
#include <windows.h>

#include <tchar.h>
#endif

#include <assert.h>


#endif //_ace_header_h



