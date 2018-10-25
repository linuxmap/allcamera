#include "svs_vms_system_api.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>


CSystem* CSystem::g_pSystem = NULL;
int32_t CSystem::open(const char* path, int32_t flags) const
{
    return ::open(path, flags);
}

int32_t CSystem::open(const char *pathname, int32_t flags, mode_t mode)const
{
    return ::open(pathname, flags, mode);
}

int32_t CSystem::pthread_mutex_init(pthread_mutex_t * mutex,
                            const pthread_mutexattr_t * attr)const
{

    return ::pthread_mutex_init(mutex, attr);
}

int32_t CSystem::pthread_mutex_lock(pthread_mutex_t *mutex)const
{
    return ::pthread_mutex_lock(mutex);
}

int32_t CSystem::pthread_mutex_unlock(pthread_mutex_t *mutex)const
{
    return ::pthread_mutex_unlock(mutex);
}

int32_t CSystem::pthread_mutex_destroy(pthread_mutex_t *mutex)const
{
    return::pthread_mutex_destroy(mutex);
}

int32_t CSystem::pthread_rwlock_init(pthread_rwlock_t * rwlock,
                                const pthread_rwlockattr_t * attr)const
{
    return ::pthread_rwlock_init(rwlock, attr);
}

int32_t CSystem::pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)const
{
    return ::pthread_rwlock_rdlock(rwlock);
}

int32_t CSystem::pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)const
{
    return ::pthread_rwlock_wrlock(rwlock);
}

int32_t CSystem::pthread_rwlock_unlock(pthread_rwlock_t *rwlock)const
{
    return ::pthread_rwlock_unlock(rwlock);
}

int32_t CSystem::socket(int32_t domain, int32_t type, int32_t protocol)const
{
    return ::socket(domain, type, protocol);
}

int32_t CSystem::shutdown(int32_t s, int32_t how)const
{
    return ::shutdown(s, how);
}

int32_t CSystem::close(int32_t fd)const
{
    return ::close(fd);
}

int32_t CSystem::bind(int32_t sockfd, const struct sockaddr *my_addr, socklen_t addrlen)const
{
    return ::bind(sockfd, my_addr, addrlen);
}

int32_t CSystem::connect(int32_t isocket, const struct sockaddr *address,
                        socklen_t address_len)const
{
    return ::connect(isocket, address, address_len);
}

int32_t CSystem::listen(int32_t sockfd, int32_t backlog)const
{
    return ::listen(sockfd, backlog);
}

int32_t CSystem::accept(int32_t sockfd, struct sockaddr *addr, socklen_t *addrlen)const
{
    return ::accept(sockfd, addr, addrlen);
}

ssize_t CSystem::recv(int32_t isocket, void *buffer, size_t length, int32_t flags)const
{
    return :: recv(isocket, buffer, length, flags);
}
ssize_t CSystem::send(int32_t s, const void *buf, size_t len, int32_t flags)const
{
    return :: send(s, buf, len, flags);
}

int32_t CSystem::setsockopt(int32_t isocket, int32_t level, int32_t option_name,
                            const void *option_value, socklen_t option_len)const
{
    return ::setsockopt(isocket, level, option_name, option_value, option_len);
}

int32_t CSystem::getsockopt(int32_t isocket, int32_t level, int32_t option_name,
              void * option_value, socklen_t * option_len)const
{
    return ::getsockopt(isocket, level, option_name, option_value, option_len);
}

int32_t CSystem::pthread_create(pthread_t * thread,
                                const pthread_attr_t * attr,
                                void *(*start_routine)(void*), void * arg)const
{
    return ::pthread_create(thread, attr, start_routine,arg);
}

int32_t CSystem::pthread_join(pthread_t thread, void **value_ptr)const
{
    return ::pthread_join(thread, value_ptr);
}

int32_t CSystem::epoll_create(int32_t size)const
{
    return ::epoll_create(size);
}

int32_t CSystem::epoll_wait(int32_t epfd, struct epoll_event *events,
                      int32_t maxevents, int32_t timeout)const
{
    return ::epoll_wait(epfd, events, maxevents, timeout);
}

int32_t CSystem::epoll_ctl(int32_t epfd, int32_t op, int32_t fd, struct epoll_event *event)const
{
    return ::epoll_ctl(epfd, op, fd, event);
}

size_t CSystem::strlen(const char *s)const
{
    return ::strlen(s);
}

int32_t CSystem::stat(const char *path,struct stat *buf)const
{
    return ::stat(path,buf);
}

off_t CSystem::lseek(int32_t fildes, off_t offset, int32_t whence)const
{
    return ::lseek(fildes,offset,whence);
}

ssize_t CSystem::read(int32_t fd, void *buf, size_t count)const
{
    return ::read(fd,buf,count);
}

int32_t CSystem::memcmp(const void *s1, const void *s2, size_t n)const
{
    return ::memcmp(s1,s2,n);
}

int32_t CSystem::strncmp(const char *s1, const char *s2, size_t n)const
{
    return ::strncmp(s1,s2,n);
}

void* CSystem::memmove(void *dest, const void *src, size_t n)const
{
    return ::memmove(dest, src, n);
}

void* CSystem::memcpy(void *dest, const void *src, size_t n)const
{
    return ::memcpy(dest, src, n);
}

void* CSystem::memset(void *s, int32_t c, size_t n)const
{
    return ::memset(s, c, n);
}



