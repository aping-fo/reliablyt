/*
* 2014-9-1
* create by qianqians
*/
#include <boost/cstdint.hpp>

enum sendtype{
	safesend,
	unsafesend,
};

enum cmdid{
	_connectreq,
	_connectack,
	_connectcomplete,
	_senddatareq,
	_response,
	_complete,
	_disconnectreq,
	_disconnectack,
	_disconnectcomplete,
};

enum UDPState{
	DisConnectreq,
	DisConnect,
	Connect,
	ConnectReq,
	ConnectAccept,
	ConnectAck,
};

#define outtime 3

/*
* protocol			begin
* sendtype			char
* cmdid				char
* len				uint32 ## head + data len ##
* index				uint32 if connectreq 0
* serial			uint32 if unsafesend 0
* data				char *
* protocol end
*/
#define head_len sizeof(char) + sizeof(char) + sizeof(boost::uint32_t) + sizeof(boost::uint32_t) + sizeof(boost::uint32_t)