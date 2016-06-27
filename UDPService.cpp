/*
* 2014-9-12
* create by qianqians
*/
#include "UDPService.h"
#include "UDPConnect.h"
#include "UDPReliableProtocol.h"
#include <vector>

UDPService::UDPService(char * ip, short port) : UDPBase(ip, port){
	post_timer(100, boost::bind(&UDPService::handle_time_resend, this, _1));
	post_timer(1000, boost::bind(&UDPService::handle_time_timeout, this, _1));
}

UDPService::~UDPService(){
}

void UDPService::handle_receive_from(char * recvbuf, int bytes_recvd, sockaddr_in & remote_addr, int error){
	do{
		if (error){
			//do_error
			return;
		}

		if (bytes_recvd < head_len){
			return;
		}

		char sendtype = recvbuf[0];
		char cmd = recvbuf[1];
		int len = recvbuf[2] | recvbuf[3] << 8 | recvbuf[4] << 16 | recvbuf[5] << 24;

		if (bytes_recvd != len){
			return;
		}

		boost::uint32_t index = recvbuf[6] | recvbuf[7] << 8 | recvbuf[8] << 16 | recvbuf[9] << 24;
		boost::uint32_t serial = recvbuf[10] | recvbuf[11] << 8 | recvbuf[12] << 16 | recvbuf[13] << 24;

		boost::uint64_t addrindex = (boost::uint64_t)remote_addr.sin_addr.S_un.S_addr << 32 | remote_addr.sin_port;

		std::map<boost::uint64_t, boost::shared_ptr<UDPConnect> >::iterator find = mapConnect.find(addrindex);

		if (cmd == _connectreq){
			if (find != mapConnect.end()){
				break;
			}

			onConnectreq(remote_addr, serial);
			break;
		}

		if (find == mapConnect.end()){
			break;
		}


		UDPConnect * c = find->second.get();
		if (c->remote_addr.sin_port != remote_addr.sin_port || c->remote_addr.sin_addr.S_un.S_addr != remote_addr.sin_addr.S_un.S_addr){
			break;
		}
		if (c->index != index){
			break;
		}

		if (cmd == _connectcomplete){
			onConnectComplete(find->second, serial);
			break;
		}

		if (c->state.load() == Connect){
			switch (cmd){
			case _senddatareq:
				onSenddatareq(std::make_pair(&recvbuf[14], len - 14), c, serial);
				break;

			case _response:
				onResponse(c, serial);
				break;

			case _complete:
				onComplete(c, serial);
				break;

			case _disconnectreq:
				onDisconnectreq(c, serial);
				break;

			default:
				break;
			}
		}

		if (c->state.load() == DisConnectreq){
			switch (cmd){
			case _disconnectack:
				onDisconnectack(c, serial);
				break;

			case _disconnectcomplete:
				onDisconnectcomplete(c, serial);
				break;

			default:
				break;
			}
		}
	} while (0);
}

void UDPService::onConnectreq(sockaddr_in & remote_addr, boost::uint32_t serial){
	boost::uint64_t addrindex = ((boost::uint64_t)remote_addr.sin_addr.S_un.S_addr) << 32 | remote_addr.sin_port;

	boost::uint32_t index = 1;
	if (!mapConnect.empty()){
		index += mapConnect.rbegin()->second->index;
	}

	boost::shared_ptr<UDPConnect> cptr(new UDPConnect());
	cptr->remote_addr = remote_addr;
	cptr->index = index;
	cptr->state.store(ConnectAccept);

	mapConnect.insert(std::make_pair(addrindex, cptr));

	cptr->response(serial, _connectack);
	cptr->state.store(ConnectAccept);
}

void UDPService::onConnectComplete(boost::shared_ptr<UDPConnect> c, boost::uint32_t serial){
	{
		boost::mutex::scoped_lock l(c->recvackbufmutex);
		std::map<boost::uint32_t, std::pair<boost::shared_ptr<char>, int > >::iterator find = c->recvackbuf.find(serial);
		if (find != c->recvackbuf.end()){
			c->recvackbuf.erase(find);
		}
	}

	c->complete(serial, _connectcomplete);
	if (c->state.load() != Connect){
		sigConnect(c);
		c->state.store(Connect);
	}
}

void UDPService::onSenddatareq(std::pair<char *, int> buf, UDPConnect * c, boost::uint32_t serial){
	if (c->remote_serial == serial){
		c->sigRecv(buf.first, buf.second);
		c->response(serial, _connectack);
		c->remote_serial++;
	}
}

void UDPService::onResponse(UDPConnect * c, boost::uint32_t serial){
	do{
		boost::mutex::scoped_lock l(c->sendbufmutex);
		if (c->sendbuf.empty()){
			break;
		}
		std::pair<boost::shared_ptr<char>, int > buf = c->sendbuf.front();
		boost::uint32_t sendserial = (buf.first.get())[10] | (buf.first.get())[11] << 8 | (buf.first.get())[12] << 16 | (buf.first.get())[13] << 24;
		if (sendserial != serial){
			break;
		}
		c->sendbuf.pop_front();
	} while (0);

	c->complete(serial, _complete);
}

void UDPService::onComplete(UDPConnect * c, boost::uint32_t serial){
	{
		boost::mutex::scoped_lock l(c->recvackbufmutex);
		std::map<boost::uint32_t, std::pair<boost::shared_ptr<char>, int > >::iterator find = c->recvackbuf.find(serial);
		if (find != c->recvackbuf.end()){
			c->recvackbuf.erase(find);
		}
	}
}

void UDPService::onDisconnectreq(UDPConnect * c, boost::uint32_t serial){
	c->state.store(DisConnectreq);
	c->response(serial, _disconnectack);
}

void UDPService::onDisconnectack(UDPConnect * c, boost::uint32_t serial){
	c->state.store(DisConnect);
	c->complete(serial, _disconnectcomplete);

	c->sigDisConnect();
}

void UDPService::onDisconnectcomplete(UDPConnect * c, boost::uint32_t serial){
	{
		boost::mutex::scoped_lock l(c->recvackbufmutex);
		std::map<boost::uint32_t, std::pair<boost::shared_ptr<char>, int > >::iterator find = c->recvackbuf.find(serial);
		if (find != c->recvackbuf.end()){
			c->recvackbuf.erase(find);
		}
	}

	c->state.store(DisConnect);

	{
		boost::mutex::scoped_lock l(mapConnectMutex);
		mapConnect.erase(c->index);
	}
}

//void UDPService::handle_send_to(const boost::system::error_code& error, size_t bytes_recvd){
//	if (error){
//		//do_error
//	}
//}

void UDPService::handle_time_resend(time_t t){
	boost::mutex::scoped_lock l(mapConnectMutex);
	for (auto var : mapConnect){
		{
			boost::mutex::scoped_lock l(var.second->unreliablesendbufmutex);
			for (auto buf : var.second->unreliablesendbuf){
				send_to(buf.first, buf.second, var.second->remote_addr);
			}
			var.second->unreliablesendbuf.clear();
		}

		{
			boost::mutex::scoped_lock l(var.second->sendbufmutex);
			if (!var.second->sendbuf.empty()){
				std::pair<boost::shared_ptr<char>, int > buf = var.second->sendbuf.front();
				send_to(buf.first, buf.second, var.second->remote_addr);
			}
		}

		{
			boost::mutex::scoped_lock l(var.second->recvackbufmutex);
			for (auto buf : var.second->recvackbuf){
				send_to(buf.second.first, buf.second.second, var.second->remote_addr);
			}
		}
	}

	post_timer(100, boost::bind(&UDPService::handle_time_resend, this, _1));
}

void UDPService::handle_time_timeout(time_t t){
	post_timer(1000, boost::bind(&UDPService::handle_time_timeout, this, _1));
}