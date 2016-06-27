#include "UDPBase.h"

UDPBase::UDPBase(char * ip, short port){
	WSADATA wsaData;
	WSAStartup(2, &wsaData);

	ttime = time(0);
	tclock = clock();
	t = ttime + tclock;

	iswork.store(true);

	th_group.create_thread(boost::bind(&UDPBase::run, this));

	h = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);
	if (h == 0){
		return;
	}
	
	s = socket(AF_INET, SOCK_DGRAM, 0);

	if (CreateIoCompletionPort((HANDLE)s, h, 0, 1) == 0){
		return;
	}

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = ntohs(port);
	addr.sin_addr.S_un.S_addr = inet_addr(ip);
	if (bind(s, (sockaddr*)&addr, sizeof(sockaddr_in)) == SOCKET_ERROR){
		return;
	}

	boost::shared_ptr<WSABUF> recvbuf(new WSABUF);
	recvbuf->buf = new char[4096];
	recvbuf->len = 4096;
	DWORD len = 0;
	DWORD flag = 0;
	boost::shared_ptr<sockaddr_in> _addr(new sockaddr_in);
	int faddrlen = sizeof(sockaddr_in);
	memset(_addr.get(), 0, faddrlen);
	OVERLAPPEDEX * op = new OVERLAPPEDEX;
	memset(&op->op, 0, sizeof(OVERLAPPED));
	op->fn = boost::bind(&UDPBase::_handle_receive_from, this, recvbuf, _addr, _1, _2);
	if (WSARecvFrom(s, recvbuf.get(), 1, &len, &flag, (sockaddr*)_addr.get(), &faddrlen, &op->op, 0) == SOCKET_ERROR){
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING){
			if (!op->fn.empty()){
				op->fn(0, err);
			}
		}
	}
}

UDPBase::~UDPBase(){
	iswork.store(false);
	th_group.join_all();

	closesocket(s);
	CloseHandle(h);

	WSACleanup();
}

void UDPBase::run(){
	while (iswork.load() == true){
		DWORD len;
		unsigned long key;
		OVERLAPPED * op;
		while (1){
			BOOL ret = GetQueuedCompletionStatus(h, &len, &key, &op, 25);
			if (ret){
				OVERLAPPEDEX * opex = (OVERLAPPEDEX *)((char*)op - (int)&((OVERLAPPEDEX *)0)->op);
				opex->fn(len, 0);
				delete opex;
			}else{
				break;
			}
		}

		handle_time();
	}
}

void UDPBase::handle_send_to(boost::shared_ptr<char> sendbuf, int len, int error){
	if (error){
		//do_error;
	}
}

void UDPBase::_handle_receive_from(boost::shared_ptr<WSABUF> recvbuf, boost::shared_ptr<sockaddr_in> remote_addr, int len, int error){
	if (error == 0){
		handle_receive_from(recvbuf->buf, len, *remote_addr, 0);
	}

	DWORD recvlen = 0;
	DWORD flag = 0;
	int faddrlen = sizeof(sockaddr_in);
	OVERLAPPEDEX * op = new OVERLAPPEDEX;
	memset(&op->op, 0, sizeof(OVERLAPPED));
	op->fn = boost::bind(&UDPBase::_handle_receive_from, this, recvbuf, remote_addr, _1, _2);
	if (WSARecvFrom(s, recvbuf.get(), 1, &recvlen, &flag, (sockaddr*)remote_addr.get(), &faddrlen, &op->op, 0) == SOCKET_ERROR){
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING){
			if (!op->fn.empty()){
				op->fn(0, err);
			}
		}
	}
}

void UDPBase::send_to(boost::shared_ptr<char> buf, int len, sockaddr_in & remote_addr){
	WSABUF wsabuf;
	wsabuf.buf = buf.get();
	wsabuf.len = len;
	DWORD sendlen = 0;
	DWORD flag = 0;
	OVERLAPPEDEX * op = new OVERLAPPEDEX;
	memset(&op->op, 0, sizeof(OVERLAPPED));
	op->fn = boost::bind(&UDPBase::handle_send_to, this, buf, _1, _2);
	if (WSASendTo(s, &wsabuf, 1, &sendlen, flag, (sockaddr*)&remote_addr, sizeof(remote_addr), &op->op, 0) == SOCKET_ERROR){
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING){
			if (!op->fn.empty()){
				op->fn(0, err);
			}
		}
	}
}

boost::uint64_t UDPBase::unixtime(){
	clock_t tmp = clock();
	if (tmp < tclock){
		t = time(0) + tmp;
	}else{
		t += tmp - tclock;
	}

	return t;
}

void UDPBase::handle_time(){
	boost::uint64_t t = unixtime();

	{
		boost::mutex::scoped_lock l(maptimefnmutex);
		for (std::vector<std::pair<time_t, timefn> >::iterator it = vinserttimer.begin(); it != vinserttimer.end(); it++){
			maptimefn.insert(*it);
		}
		vinserttimer.clear();
	}

	std::map<time_t, timefn>::iterator begin = maptimefn.begin();
	if (begin != maptimefn.end()){
		for (std::map<time_t, timefn>::iterator iter = begin; iter != maptimefn.upper_bound(t) && iter != maptimefn.end(); iter++){
			timefn fn = iter->second;
			if (!fn.empty()){
				fn(t);
			}
		}
		maptimefn.erase(begin, maptimefn.upper_bound(t));
	}
}

void UDPBase::post_timer(time_t t, timefn fn){
	boost::mutex::scoped_lock l(maptimefnmutex);
	vinserttimer.push_back(std::make_pair(unixtime() + t, fn));
}