/*
* 2014-9-12
* create by qianqians
*/
#include "UDPClient.h"
#include "UDPReliableProtocol.h"


UDPClient::UDPClient(char * ip, short port) : UDPBase(ip, port){
	post_timer(100, boost::bind(&UDPClient::handle_time_resend, this, _1));
}

UDPClient::~UDPClient(){
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

bool UDPClient::connect(char * ip, short port){
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = ntohs(port);
	remote_addr.sin_addr.S_un.S_addr = inet_addr(ip);

	state.store(ConnectReq);

	boost::shared_ptr<char> data(new char[head_len]);
	char * databuf = data.get();

	int datalen = head_len;

	databuf[0] = safesend;
	databuf[1] = _connectreq;
	databuf[2] = datalen & 0xff;
	databuf[3] = (datalen & 0xff00) >> 8;
	databuf[4] = (datalen & 0xff0000) >> 16;
	databuf[5] = (datalen & 0xff000000) >> 24;
	databuf[6] = 0;
	databuf[7] = 0;
	databuf[8] = 0;
	databuf[9] = 0;
	databuf[10] = 0;
	databuf[11] = 0;
	databuf[12] = 0;
	databuf[13] = 0;

	{
		boost::mutex::scoped_lock l(sendbufmutex);
		sendbuf.push_back(std::make_pair(data, head_len));
	}

	while (state.load() == ConnectReq){
		boost::this_thread::yield();
	}

	return true;
}


void UDPClient::handle_receive_from(char * recvbuf, int bytes_recvd, sockaddr_in & faddr, int error){
	do{
		if (error){
			//do_error
			break;
		}

		if (bytes_recvd < head_len){
			break;
		}

		char sendtype = recvbuf[0];
		char cmd = recvbuf[1];
		int len = recvbuf[2] | recvbuf[3] << 8 | recvbuf[4] << 16 | recvbuf[5] << 24;

		if (bytes_recvd != len){
			return;
		}

		boost::uint32_t _index = recvbuf[6] | recvbuf[7] << 8 | recvbuf[8] << 16 | recvbuf[9] << 24;
		boost::uint32_t serial = recvbuf[10] | recvbuf[11] << 8 | recvbuf[12] << 16 | recvbuf[13] << 24;

		if (faddr.sin_port != remote_addr.sin_port || faddr.sin_addr.S_un.S_addr != remote_addr.sin_addr.S_un.S_addr){
			return;
		}

		if (cmd == _connectack){
			if (state.load() != ConnectReq){
				//break;
			}

			index = _index;
			onConnectAck(serial);
			break;
		}

		if (state.load() == Connect){
			switch (cmd){
			case _senddatareq:
				onSenddatareq(std::make_pair(&recvbuf[14], len - 14), serial);
				break;

			case _response:
				onResponse(serial);
				break;

			case _complete:
				onComplete(serial);
				break;

			case _disconnectreq:
				onDisconnectreq(serial);
				break;

			default:
				break;
			}
		}

		if (state.load() == DisConnectreq){
			switch (cmd){
			case _disconnectack:
				onDisconnectack(serial);
				break;

			case _disconnectcomplete:
				onDisconnectcomplete(serial);
				break;

			default:
				break;
			}
		}
	} while (0);
}

//void UDPClient::handle_send_to(const boost::system::error_code& error, size_t bytes_recvd){
//	if (error){
//		//do_error;
//	}
//}

void UDPClient::handle_time_resend(time_t t){
	{
		boost::mutex::scoped_lock l(recvackbufmutex);
		for (auto buf : recvackbuf){
			send_to(buf.second.first, buf.second.second, remote_addr);
		}
	}

	{
		boost::mutex::scoped_lock l(unreliablesendbufmutex);
		for (auto buf : unreliablesendbuf){
			send_to(buf.first, buf.second, remote_addr);
		}
		unreliablesendbuf.clear();
	}

	{
		boost::mutex::scoped_lock l(sendbufmutex);
		if (!sendbuf.empty()){
			std::pair<boost::shared_ptr<char>, int> buf = sendbuf.front();
			send_to(buf.first, buf.second, remote_addr);
		}
	}

	post_timer(100, boost::bind(&UDPClient::handle_time_resend, this, _1));
}

void UDPClient::onConnectAck(boost::uint32_t serial){
	do{
		boost::mutex::scoped_lock l(sendbufmutex); 
		if (sendbuf.empty()){
			break;
		}
		std::pair<boost::shared_ptr<char>, int> buf = sendbuf.front();
		boost::uint32_t sendserial = (buf.first.get())[10] | (buf.first.get())[11] << 8 | (buf.first.get())[12] << 16 | (buf.first.get())[13] << 24;
		if (sendserial != serial){
			break;
		}
		sendbuf.pop_front();
	} while (0);

	complete(serial, _connectcomplete);
	state.store(Connect);
}

void UDPClient::onSenddatareq(std::pair<char *, int> buf, boost::uint32_t serial){
	if (remote_serial == serial){
		sigRecv(buf.first, buf.second);
		response(serial, _response);
		remote_serial++;
	}
}

void UDPClient::onResponse(boost::uint32_t serial){
	do{
		boost::mutex::scoped_lock l(sendbufmutex);
		if (sendbuf.empty()){
			break;
		}
		std::pair<boost::shared_ptr<char>, int> buf = sendbuf.front();
		boost::uint32_t sendserial = (buf.first.get())[10] | (buf.first.get())[11] << 8 | (buf.first.get())[12] << 16 | (buf.first.get())[13] << 24;
		if (sendserial != serial){
			return;
		}
		sendbuf.pop_front();
	} while (0);

	complete(serial, _complete);
}

void UDPClient::onComplete(boost::uint32_t serial){
	{
		boost::mutex::scoped_lock l(recvackbufmutex);
		std::map<boost::uint32_t, std::pair<boost::shared_ptr<char>, int > >::iterator find = recvackbuf.find(serial);
		if (find != recvackbuf.end()){
			recvackbuf.erase(find);
		}
	}
}

void UDPClient::onDisconnectreq(boost::uint32_t serial){
	state.store(DisConnectreq);
	response(serial, _disconnectack);
}

void UDPClient::onDisconnectack(boost::uint32_t serial){
	state.store(DisConnect);
	complete(serial, _disconnectcomplete);
}

void UDPClient::onDisconnectcomplete(boost::uint32_t serial){
	state.store(DisConnect);
}