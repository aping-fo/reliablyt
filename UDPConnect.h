/*
* 2014-9-12
* create by qianqians
*/
#include <list>
#include <map>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>
#include <boost/cstdint.hpp>

#include "UDPSession.h"

class UDPConnect : public UDPSession{
private:
	friend class UDPService;

};