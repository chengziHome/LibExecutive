#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "CLEpoll.h"
#include "CLLogger.h"
#include "CLDelayedDataPoster.h"
#include "CLMsgLoopManagerForEpoll.h"

#define DEFAULT_EPOLLWAIT_TIME 5

CLEpoll::CLEpoll()
{
	m_pEpollEvents = NULL;
	m_iEpollFd = -1;
	m_iMaxEventSize = 0;
}

CLEpoll::~CLEpoll()
{
	close(m_iEpollFd);
}

CLStatus CLEpoll::Initialize(int maxFdSize)
{
	m_iMaxEventSize = maxFdSize;

	m_iEpollFd = epoll_create(maxFdSize);
	if(0 > m_iEpollFd)
	{	
		CLLogger::WriteLogMsg("In CLEpoll::Initialize(), epoll_create error", errno);
		return CLStatus(-1, errno);
	}

	m_pEpollEvents = new epoll_event[m_iMaxEventSize];
	memset(m_pEpollEvents, 0, m_iMaxEventSize * sizeof(struct epoll_event));

	// m_pMsgLoopManager = pMsgLoopManager;

	return CLStatus(0, 0);
}

CLStatus CLEpoll::DoEvent(CLEpollEvent *pEvent, int fd, int epollOpt, int epollEvents)
{
	struct epoll_event ev;
	memset(&ev, 0, sizeof(struct epoll_event));
	ev.data.ptr = pEvent->GetHandler();
	ev.events = epollEvents;

	if(0 > epoll_ctl(m_iEpollFd, epollOpt, fd, &ev))
	{
		CLLogger::WriteLogMsg("In CLEpoll::DoEvent(), epoll_ctl() error", errno);
		return CLStatus(-1, errno);
	}

	return CLStatus(0, 0);
}


CLStatus CLEpoll::Run()
{
	int nfds;

	while(1)
	{
		if( (nfds = epoll_wait(m_iEpollFd, m_pEpollEvents, m_iMaxEventSize, DEFAULT_EPOLLWAIT_TIME)) < 0)
		{
			if(EINTR == errno)
				continue;
			else
				CLLogger::WriteLogMsg("In CLEpoll::Run(), epoll_wait error", errno);
		}

		for(int i = 0; i < nfds; ++i)
		{
			if ( m_pEpollEvents[i].events & EPOLLERR || m_pEpollEvents[i].events & EPOLLHUP )
            {
               continue;
            }
            if ( m_pEpollEvents[i].events & EPOLLOUT )
            {
            	CLDelayedDataPoster *pDataPoster = (CLDelayedDataPoster *)m_pEpollEvents[i].data.ptr;
            	if(pDataPoster == NULL)
            	{
            		CLLogger::WriteLogMsg("In CLEpoll::Run(), pDataPoster == NULL", 0);
            		continue;
               	}
               	CLStatus s = pDataPoster->SendData();
               	if(!s.IsSuccess())
               	{
               		CLLogger::WriteLogMsg("In CLEpoll::Run(), pDataPoster->SendData() error", 0);
               	}

               	continue;
            }
            if ( m_pEpollEvents[i].events & EPOLLIN )
            {
            	CLMsgLoopManagerForEpoll *pMsgLoopManager = (CLMsgLoopManagerForEpoll *)m_pEpollEvents[i].data.ptr;
            	if(pMsgLoopManager)
            	{
            		CLStatus s = pMsgLoopManager->NotifyReadable(m_pEpollEvents[i].data.fd);
            		if(!s.IsSuccess())
            		{
            			CLLogger::WriteLogMsg("In CLEpoll::Run(), pMsgLoopManager->NotifyReadable() error", 0);
            		}
            	}
            	else
            	{
            		CLLogger::WriteLogMsg("In CLEpoll::Run(), pMsgLoopManager is NULL", 0);
            	}
                continue;
            }
		}
	}


	return CLStatus(0, 0);
}
