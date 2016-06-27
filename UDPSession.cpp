#include "UDPSession.h"
#include "UDPReliableProtocol.h"

UDPSession::UDPSession(){
	index = 0;
	serial = 1;
	remote_serial = 1;
	state = 0;
}

UDPSession::~UDPSession(){
}

//#pragma pack(push, 1)
//	struct {
//		char			sendtype;
//		char			cmdid;
//		boost::uint32_t len;			// head + data len 
//		boost::uint32_t index;			// if connectreq 0
//		boost::uint32_t serial;			// if unsafesend 0
//	} data;
//#pragma pop()
// little end

void UDPSession::reliable_send(char * buf, int len){
	int buflen = len + head_len;

	boost::shared_ptr<char> data(new char[buflen]);
	char * databuf = data.get();

	databuf[0] = safesend;
	databuf[1] = _senddatareq;
	databuf[2] = buflen & 0xff;
	databuf[3] = (buflen & 0xff00) >> 8;
	databuf[4] = (buflen & 0xff0000) >> 16;
	databuf[5] = (buflen & 0xff000000) >> 24;
	databuf[6] = index & 0xff;
	databuf[7] = (index & 0xff00) >> 8;
	databuf[8] = (index & 0xff0000) >> 16;
	databuf[9] = (index & 0xff000000) >> 24;
	databuf[10] = serial & 0xff;
	databuf[11] = (serial & 0xff00) >> 8;
	databuf[12] = (serial & 0xff0000) >> 16;
	databuf[13] = (serial & 0xff000000) >> 24;
	for (int i = 0; i < len; i++){
		databuf[14 + i] = buf[i];
	}

	boost::mutex::scoped_lock l(sendbufmutex);
	sendbuf.push_back(std::make_pair(data, buflen));

	serial++;
}

void UDPSession::unreliable_send(char * buf, int len){
	int datalen = len + head_len;

	boost::shared_ptr<char> data(new char[datalen]);
	char * databuf = data.get();

	databuf[0] = unsafesend;
	databuf[1] = _senddatareq;
	databuf[2] = datalen & 0xff;
	databuf[3] = (datalen & 0xff00) >> 8;
	databuf[4] = (datalen & 0xff0000) >> 16;
	databuf[5] = (datalen & 0xff000000) >> 24;
	databuf[6] = index & 0xff;
	databuf[7] = (index & 0xff00) >> 8;
	databuf[8] = (index & 0xff0000) >> 16;
	databuf[9] = (index & 0xff000000) >> 24;
	databuf[10] = serial & 0xff;
	databuf[11] = (serial & 0xff00) >> 8;
	databuf[12] = (serial & 0xff0000) >> 16;
	databuf[13] = (serial & 0xff000000) >> 24;
	for (int i = 0; i < len; i++){
		databuf[14 + i] = buf[i];
	}

	boost::mutex::scoped_lock l(unreliablesendbufmutex);
	unreliablesendbuf.push_back(std::make_pair(data, datalen));
}

void UDPSession::response(int serial, char cmdid){
	int datalen = head_len;

	boost::shared_ptr<char> data(new char[datalen]);
	char * databuf = data.get();

	databuf[0] = unsafesend;
	databuf[1] = cmdid;
	databuf[2] = head_len & 0xff;
	databuf[3] = (head_len & 0xff00) >> 8;
	databuf[4] = (head_len & 0xff0000) >> 16;
	databuf[5] = (head_len & 0xff000000) >> 24;
	databuf[6] = index & 0xff;
	databuf[7] = (index & 0xff00) >> 8;
	databuf[8] = (index & 0xff0000) >> 16;
	databuf[9] = (index & 0xff000000) >> 24;
	databuf[10]= serial & 0xff;
	databuf[11] = (serial & 0xff00) >> 8;
	databuf[12] = (serial & 0xff0000) >> 16;
	databuf[13] = (serial & 0xff000000) >> 24;

	boost::mutex::scoped_lock l(recvackbufmutex);
	recvackbuf.insert(std::make_pair(serial, std::make_pair(data, datalen)));
}

void UDPSession::complete(int serial, char cmdid){
	int datalen = head_len;

	boost::shared_ptr<char> data(new char[datalen]);
	char * databuf = data.get();

	databuf[0] = unsafesend;
	databuf[1] = cmdid;
	databuf[2] = head_len & 0xff;
	databuf[3] = (head_len & 0xff00) >> 8;
	databuf[4] = (head_len & 0xff0000) >> 16;
	databuf[5] = (head_len & 0xff000000) >> 24;
	databuf[6] = index & 0xff;
	databuf[7] = (index & 0xff00) >> 8;
	databuf[8] = (index & 0xff0000) >> 16;
	databuf[9] = (index & 0xff000000) >> 24;
	databuf[10] = serial & 0xff;
	databuf[11] = (serial & 0xff00) >> 8;
	databuf[12] = (serial & 0xff0000) >> 16;
	databuf[13] = (serial & 0xff000000) >> 24;

	boost::mutex::scoped_lock l(unreliablesendbufmutex);
	unreliablesendbuf.push_back(std::make_pair(data, datalen));
}

void UDPSession::disconnect(){
	int datalen = head_len;

	boost::shared_ptr<char> data(new char[datalen]);
	char * databuf = data.get();

	databuf[0] = safesend;
	databuf[1] = _disconnectreq;
	databuf[2] = head_len & 0xff;
	databuf[3] = (head_len & 0xff00) >> 8;
	databuf[4] = (head_len & 0xff0000) >> 16;
	databuf[5] = (head_len & 0xff000000) >> 24;
	databuf[6] = index & 0xff;
	databuf[7] = (index & 0xff00) >> 8;
	databuf[8] = (index & 0xff0000) >> 16;
	databuf[9] = (index & 0xff000000) >> 24;
	databuf[10] = serial & 0xff;
	databuf[11] = (serial & 0xff00) >> 8;
	databuf[12] = (serial & 0xff0000) >> 16;
	databuf[13] = (serial & 0xff000000) >> 24;

	{
		boost::mutex::scoped_lock l(unreliablesendbufmutex);
		sendbuf.push_back(std::make_pair(data, datalen));
	}

	state.store(DisConnectreq);

	while (state.load() != DisConnect){
		boost::this_thread::yield();
	}
}