PROJECT(reliablyt)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6) 

AUX_SOURCE_DIRECTORY(. DIR_SRC)

SET(header_files
    UDPBase.h
    UDPClient.h
    UDPConnect.h
    UDPReliableProtocol.h
    UDPService.h
    UDPSession.h
)

SET(SRC_LIST 
    ${DIR_SRC}
)

ADD_LIBRARY(reliablyt ${SRC_LIST} ${header_files})