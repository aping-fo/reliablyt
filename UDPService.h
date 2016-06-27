class UDPConnect;
/*
 * 2014-9-12 
 */
#include "UDPBase.h"

class UDPService : public UDPBase{
public:
	UDPService(char * ip, short port);
	~UDPService();

	/*
	 * connect signal, detonate by a session connect in 
	 */
	boost::signals2::signal<void(boost::shared_ptr<UDPConnect>) > sigConnect;

private:
	virtual void handle_receive_from(char * recvbuf, int len, sockaddr_in & remote_addr, int error);

	void onConnectreq(sockaddr_in & remote_addr, boost::uint32_t serial);

	void onConnectComplete(boost::shared_ptr<UDPConnect> c, boost::uint32_t serial);

	void onSenddatareq(std::pair<char *, int> buf, UDPConnect * c, boost::uint32_t serial);

	void onResponse(UDPConnect * c, boost::uint32_t serial);

	void onComplete(UDPConnect * c, boost::uint32_t serial);

	void onDisconnectreq(UDPConnect * c, boost::uint32_t serial);

	void onDisconnectack(UDPConnect * c, boost::uint32_t serial);

	void onDisconnectcomplete(UDPConnect * c, boost::uint32_t serial);

private:
	void handle_time_resend(time_t t);

	void handle_time_timeout(time_t t);

private:
	boost::mutex mapConnectMutex;
	std::map<boost::uint64_t, boost::shared_ptr<UDPConnect> > mapConnect;

};