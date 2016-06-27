/*
* qianqians
* 2014-9-12
*/
#include "UDPBase.h"
#include "UDPSession.h"

class UDPClient : public UDPBase, public UDPSession{
public:
	UDPClient(char * ip, short port);
	~UDPClient();

	/*
	 * connect to a remote service
	 */
	bool connect(char * ip, short port);

private:
	virtual void handle_receive_from(char * recvbuf, int len, sockaddr_in & remote_addr, int error);

private:
	void handle_time_resend(time_t t);

private:
	void onConnectAck(boost::uint32_t serial);

	void onSenddatareq(std::pair<char *, int> buf, boost::uint32_t serial);

	void onResponse(boost::uint32_t serial);

	void onComplete(boost::uint32_t serial);

	void onDisconnectreq(boost::uint32_t serial);

	void onDisconnectack(boost::uint32_t serial);

	void onDisconnectcomplete(boost::uint32_t serial);

};