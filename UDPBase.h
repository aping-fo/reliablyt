/*
 * qianqians
 * 2014-9-11
 */
#include <WinSock2.h>

#include <map>
#include <vector>

#include <boost/atomic.hpp>
#include <boost/signals2.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/cstdint.hpp>

#pragma comment(lib, "WS2_32.lib")

class UDPSession;

typedef boost::function<void(time_t) > timefn;

struct OVERLAPPEDEX{
	boost::function<void(int, int) > fn;
	OVERLAPPED op;
};

class UDPBase{
public:
	UDPBase(char * ip, short port);
	~UDPBase();

public:
	/*
	 * post a time callback
	 */
	void post_timer(time_t t, timefn fn);

protected:
	virtual void handle_receive_from(char * recvbuf, int len, sockaddr_in & remote_addr, int error) = 0;

	virtual void handle_send_to(boost::shared_ptr<char> sendbuf, int len, int error);

	void send_to(boost::shared_ptr<char> buf, int len, sockaddr_in & remote_addr);

private:
	void run();

	void _handle_receive_from(boost::shared_ptr<WSABUF> recvbuf, boost::shared_ptr<sockaddr_in> remote_addr, int len, int error);

	void handle_time();

protected:
	boost::mutex maptimefnmutex;
	std::vector<std::pair<time_t, timefn> > vinserttimer;
	std::map<time_t, timefn> maptimefn;

	boost::uint64_t ttime; 
	boost::uint64_t t;
	clock_t tclock;

	boost::uint64_t unixtime();

private:
	SOCKET s;
	HANDLE h;
	
	boost::thread_group th_group;
	boost::atomic_bool iswork;

};