#ifndef _CSYSTEMAPI_H_
#define _CSYSTEMAPI_H_


#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <pthread.h>
#include <vms/vms.h>


#ifdef  UNITTEST
#define UTAPI  virtual
#else
#define UTAPI
#endif


class CSystem
{

#ifdef UNITTEST
    friend class MockCSystem;
#endif

public:
    UTAPI ~CSystem()
    {
    }
    static CSystem* instance()
    {
        if (NULL == g_pSystem)
        {
            try
            {
                g_pSystem = new CSystem;
            }
            catch(...)
            {
            }
        }
        return g_pSystem;
    }
    UTAPI int32_t open(const char* path, int32_t flags)const;
    UTAPI int32_t open(const char *pathname, int32_t flags, mode_t mode)const;

    UTAPI int32_t pthread_mutex_init(pthread_mutex_t * mutex,
                                const pthread_mutexattr_t * attr)const;
    UTAPI int32_t pthread_mutex_lock(pthread_mutex_t *mutex)const;

    UTAPI int32_t pthread_mutex_unlock(pthread_mutex_t *mutex)const;

    UTAPI int32_t pthread_mutex_destroy(pthread_mutex_t *mutex)const;

    UTAPI int32_t pthread_rwlock_init(pthread_rwlock_t * rwlock,
                                     const pthread_rwlockattr_t * attr)const;
    UTAPI int32_t pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)const;

    UTAPI int32_t pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)const;

    UTAPI int32_t pthread_rwlock_unlock(pthread_rwlock_t *rwlock)const;

    UTAPI int32_t socket(int32_t domain, int32_t type, int32_t protocol)const;
    UTAPI int32_t bind(int32_t sockfd, const struct sockaddr *my_addr, socklen_t addrlen)const;
    UTAPI int32_t connect(int32_t isocket,
                        const struct sockaddr *address,
                        socklen_t address_len)const;
    UTAPI int32_t shutdown(int32_t s, int32_t how)const;
    UTAPI int32_t close(int32_t fd)const;
    UTAPI int32_t listen(int32_t sockfd, int32_t backlog)const;
    UTAPI int32_t accept(int32_t sockfd, struct sockaddr *addr, socklen_t *addrlen)const;
    UTAPI ssize_t recv(int32_t isocket, void *buffer, size_t length, int32_t flags)const;
    UTAPI ssize_t send(int32_t s, const void *buf, size_t len, int32_t flags)const;
    UTAPI int32_t setsockopt(int32_t isocket,
                           int32_t level,
                           int32_t option_name,
                           const void *option_value,
                           socklen_t option_len)const;

    UTAPI int32_t getsockopt(int32_t isocket,
                                    int32_t level,
                                    int32_t option_name,
                                    void * option_value,
                                    socklen_t * option_len)const;
    UTAPI int32_t pthread_create(pthread_t * thread,
                            const pthread_attr_t * attr,
                            void *(*start_routine)(void*),
                            void * arg)const;
    UTAPI int32_t pthread_join(pthread_t thread, void **value_ptr)const;

    UTAPI int32_t epoll_create(int32_t size)const;
    UTAPI int32_t epoll_wait(int32_t epfd, struct epoll_event *events,
                      int32_t maxevents, int32_t timeout)const;
    UTAPI int32_t epoll_ctl(int32_t epfd, int32_t op, int32_t fd, struct epoll_event *event)const;
    UTAPI size_t strlen(const char *s)const;
    UTAPI int32_t stat(const char *path,struct stat *buf)const;
    UTAPI off_t lseek(int32_t fildes, off_t offset, int32_t whence)const;
    UTAPI ssize_t read(int32_t fd, void *buf, size_t count)const;
    UTAPI int32_t memcmp(const void *s1, const void *s2, size_t n)const;
    UTAPI int32_t strncmp(const char *s1, const char *s2, size_t n)const;
    UTAPI void *memmove(void *dest, const void *src, size_t n)const;
    UTAPI void *memcpy(void *dest, const void *src, size_t n)const;
    UTAPI void *memset(void *s, int32_t c, size_t n)const;
private:
    static CSystem* g_pSystem;

};



#endif
