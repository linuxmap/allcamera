#ifndef _SVS_Auto_Reference_Ptr_h
#define _SVS_Auto_Reference_Ptr_h
/*
* CLASS: SVS_Auto_Reference_Ptr<T>
* DESCRIPTION: 自动解除引用计数类模板
* REMARK:
* 类T的定义形式如下：
*class T
*{
*public:
*       typedef KEY int32_t;
*       T();
*       ~T();
*public:
*        void decrease_reference(KEY key)
*        {
*           ACE_GUARD_RETURN(ACE_Recursive_Thread_Mutex, guard, mutex_, -1);
*           CAC_Router *pObj = NULL;
*           map_.find(key, pObj);
*           if(0 == pObj->decrease_reference_count())
*           {
*               map_.unbind(key, pObj);
*               delete pObj;
*               pObj = NULL;
*           }
*       }
*
*        CAC_Router *get_reference(KEY key)
*        {
*           CAC_Router *pObj = NULL;
*           map_.find(key, pObj);
*           pObj->increase_reference_count();
*           return pObj;
*        }
*
*       void test(KEY key)
*       {
*           CAC_Router *pObj = get_reference(key);
*           SVS_Auto_Reference_Ptr ptr(this, key);
*           ....
*           return;
*        }
*
*protected:
*   ACE_Recursive_Thread_Mutex mutex_;
*   ACE_Hash_Map_Manager<int32_t, CAC_Router *, ACE_Null_Mutex> map_;
*};
*
*
*/
template <class T, typename SVSKEY>
class  CAC_Auto_Reference_Ptr
{
public:
    CAC_Auto_Reference_Ptr(T *pT, SVSKEY key)
    {
        p_T_ = pT;
        key_ = key;
    }
    virtual ~CAC_Auto_Reference_Ptr()
    {
        p_T_->decrease_reference(key_);
        key_ = 0;
        p_T_ = NULL;
    }
private:
    CAC_Auto_Reference_Ptr();
    CAC_Auto_Reference_Ptr(const CAC_Auto_Reference_Ptr &obj);
    CAC_Auto_Reference_Ptr& operator=(const CAC_Auto_Reference_Ptr& obj);
protected:
    T *p_T_;
    SVSKEY key_;
};
#endif //_SVS_Auto_Reference_Ptr_h


