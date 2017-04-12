#include "ofxTCPServerNullTerminated.h"
#include "ofxTCPClientNullTerminated.h"
#include "ofUtils.h"

//--------------------------
ofxTCPServerNullTerminated::ofxTCPServerNullTerminated(){
	connected	= false;
	idCount		= 0;
	port		= 0;
	str			= "";
	messageDelimiter = "[/TCP]";
	bClientBlocking = false;
}

//--------------------------
ofxTCPServerNullTerminated::~ofxTCPServerNullTerminated(){
	close();
}

//--------------------------
void ofxTCPServerNullTerminated::setVerbose(bool _verbose){
	ofLogWarning("ofxTCPServerNullTerminated") << "setVerbose(): is deprecated, replaced by ofLogWarning and ofLogError";
}

//--------------------------
bool ofxTCPServerNullTerminated::setup(int _port, bool blocking){
	if( !TCPServer.Create() ){
		ofLogError("ofxTCPServerNullTerminated") << "setup(): couldn't create server";
		return false;
	}
	if( !TCPServer.Bind(_port) ){
		ofLogError("ofxTCPServerNullTerminated") << "setup(): couldn't bind to port " << _port;
		return false;
	}

	connected		= true;
	port			= _port;
	bClientBlocking = blocking;

	std::unique_lock<std::mutex> lck(mConnectionsLock);
	startThread();
    serverReady.wait(lck);

	return true;
}

//--------------------------
bool ofxTCPServerNullTerminated::close(){
    stopThread();
	if( !TCPServer.Close() ){
		ofLogWarning("ofxTCPServerNullTerminated") << "close(): couldn't close connections";
		waitForThread(false); // wait for the thread to finish
		return false;
    }else{
        ofLogVerbose("ofxTCPServerNullTerminated") << "Closing server";
		waitForThread(false); // wait for the thread to finish
		return true;
	}
}

ofxTCPClientNullTerminated & ofxTCPServerNullTerminated::getClient(int clientID){
	return *TCPConnections.find(clientID)->second;
}

//--------------------------
bool ofxTCPServerNullTerminated::disconnectClient(int clientID){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if( !isClientSetup(clientID) ){
		ofLogWarning("ofxTCPServerNullTerminated") << "disconnectClient(): client " << clientID << " doesn't exist";
		return false;
	}else if(getClient(clientID).close()){
		TCPConnections.erase(clientID);
		return true;
	}
	return false;
}

//--------------------------
bool ofxTCPServerNullTerminated::disconnectAllClients(){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
    TCPConnections.clear();
    return true;
}

//--------------------------
bool ofxTCPServerNullTerminated::send(int clientID, std::string message){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if( !isClientSetup(clientID) ){
		ofLogWarning("ofxTCPServerNullTerminated") << "send(): client " << clientID << " doesn't exist";
		return false;
	}else{
        auto ret = getClient(clientID).send(message);
		if(!getClient(clientID).isConnected()) TCPConnections.erase(clientID);
        return ret;
	}
}

//--------------------------
bool ofxTCPServerNullTerminated::sendToAll(std::string message){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if(TCPConnections.size() == 0) return false;

	std::vector<int> disconnect;
	for(auto & conn: TCPConnections){
		if(conn.second->isConnected()) conn.second->send(message);
		else disconnect.push_back(conn.first);
	}
	for(int i=0; i<(int)disconnect.size(); i++){
    	TCPConnections.erase(disconnect[i]);
    }
	return true;
}

//--------------------------
std::string ofxTCPServerNullTerminated::receive(int clientID){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if( !isClientSetup(clientID) ){
		ofLogWarning("ofxTCPServerNullTerminated") << "receive(): client " << clientID << " doesn't exist";
		return "client " + ofToString(clientID) + "doesn't exist";
	}
	
	if( !getClient(clientID).isConnected() ){
        TCPConnections.erase(clientID);
		return "";
	}

	return getClient(clientID).receive();
}

//--------------------------
bool ofxTCPServerNullTerminated::sendRawBytes(int clientID, const char * rawBytes, const int numBytes){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if( !isClientSetup(clientID) ){
		ofLogWarning("ofxTCPServerNullTerminated") << "sendRawBytes(): client " << clientID << " doesn't exist";
		
		return false;
	}
	else{
		return getClient(clientID).sendRawBytes(rawBytes, numBytes);
	}
}

//--------------------------
bool ofxTCPServerNullTerminated::sendRawBytesToAll(const char * rawBytes, const int numBytes){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if(TCPConnections.size() == 0 || numBytes <= 0) return false;

	for(auto & conn: TCPConnections){
		if(conn.second->isConnected()){
			conn.second->sendRawBytes(rawBytes, numBytes);
		}
	}
	return true;
}


//--------------------------
bool ofxTCPServerNullTerminated::sendRawMsg(int clientID, const char * rawBytes, const int numBytes){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if( !isClientSetup(clientID) ){
		ofLogWarning("ofxTCPServerNullTerminated") << "sendRawMsg(): client " << clientID << " doesn't exist";
		return false;
	}
	else{
		return getClient(clientID).sendRawMsg(rawBytes, numBytes);
	}
}

//--------------------------
bool ofxTCPServerNullTerminated::sendRawMsgToAll(const char * rawBytes, const int numBytes){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if(TCPConnections.empty() || numBytes <= 0) return false;

	for(auto & conn: TCPConnections){
		if(conn.second->isConnected()){
			conn.second->sendRawMsg(rawBytes, numBytes);
		}
	}
	return true;
}

//--------------------------
int ofxTCPServerNullTerminated::getNumReceivedBytes(int clientID){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if( !isClientSetup(clientID) ){
		ofLogWarning("ofxTCPServerNullTerminated") << "getNumReceivedBytes(): client " << clientID << " doesn't exist";
		return 0;
	}

	return getClient(clientID).getNumReceivedBytes();
}

//--------------------------
int ofxTCPServerNullTerminated::receiveRawBytes(int clientID, char * receiveBytes,  int numBytes){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if( !isClientSetup(clientID) ){
		ofLogWarning("ofxTCPServerNullTerminated") << "receiveRawBytes(): client " << clientID << " doesn't exist";
		return 0;
	}

	return getClient(clientID).receiveRawBytes(receiveBytes, numBytes);
}


//--------------------------
int ofxTCPServerNullTerminated::peekReceiveRawBytes(int clientID, char * receiveBytes,  int numBytes){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if( !isClientSetup(clientID) ){
		ofLog(OF_LOG_WARNING, "ofxTCPServerNullTerminated: client " + ofToString(clientID) + " doesn't exist");
		return 0;
	}

	return getClient(clientID).peekReceiveRawBytes(receiveBytes, numBytes);
}

//--------------------------
int ofxTCPServerNullTerminated::receiveRawMsg(int clientID, char * receiveBytes,  int numBytes){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if( !isClientSetup(clientID) ){
		ofLogWarning("ofxTCPServerNullTerminated") << "receiveRawMsg(): client " << clientID << " doesn't exist";
		return 0;
	}

	return getClient(clientID).receiveRawMsg(receiveBytes, numBytes);
}

//--------------------------
int ofxTCPServerNullTerminated::getClientPort(int clientID){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if( !isClientSetup(clientID) ){
		ofLogWarning("ofxTCPServerNullTerminated") << "getClientPort(): client " << clientID << " doesn't exist";
		return 0;
	}
	else return getClient(clientID).getPort();
}

//--------------------------
std::string ofxTCPServerNullTerminated::getClientIP(int clientID){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if( !isClientSetup(clientID) ){
		ofLogWarning("ofxTCPServerNullTerminated") << "getClientIP(): client " << clientID << " doesn't exist";
		return "000.000.000.000";
	}
	else return getClient(clientID).getIP();
}

//--------------------------
int ofxTCPServerNullTerminated::getNumClients(){
	return TCPConnections.size();
}

//--------------------------
int ofxTCPServerNullTerminated::getLastID(){
	return idCount;
}

//--------------------------
int ofxTCPServerNullTerminated::getPort(){
	return port;
}

//--------------------------
bool ofxTCPServerNullTerminated::isConnected(){
	return connected;
}

//--------------------------
bool ofxTCPServerNullTerminated::isClientSetup(int clientID){
	return TCPConnections.find(clientID)!=TCPConnections.end();
}

//--------------------------
bool ofxTCPServerNullTerminated::isClientConnected(int clientID){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	return isClientSetup(clientID) && getClient(clientID).isConnected();
}


void ofxTCPServerNullTerminated::waitConnectedClient(){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if(TCPConnections.empty()){
		serverReady.wait(lck);
	}
}

void ofxTCPServerNullTerminated::waitConnectedClient(int ms){
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	if(TCPConnections.empty()){
		serverReady.wait_for(lck, std::chrono::milliseconds(ms));
	}
}

//don't call this
//--------------------------
void ofxTCPServerNullTerminated::threadedFunction(){

	ofLogVerbose("ofxTCPServerNullTerminated") << "listening thread started";
	while( isThreadRunning() ){
		
		int acceptId;
		for(acceptId = 0; acceptId <= idCount; acceptId++){
			if(!isClientConnected(acceptId)) break;
		}
		
		if(acceptId == TCP_MAX_CLIENTS){
			ofLogWarning("ofxTCPServerNullTerminated") << "no longer accepting connections, maximum number of clients reached: " << TCP_MAX_CLIENTS;
			break;
		}

		if( !TCPServer.Listen(TCP_MAX_CLIENTS) ){
			if(isThreadRunning()) ofLogError("ofxTCPServerNullTerminated") << "listening failed";
		}

        {
			std::unique_lock<std::mutex> lck( mConnectionsLock );
            serverReady.notify_one();
        }
		
		//	we need to lock here, but can't as it blocks...
		//	so use a temporary to not block the lock 
		std::shared_ptr<ofxTCPClientNullTerminated> client(new ofxTCPClientNullTerminated);
		if( !TCPServer.Accept( client->TCPClient ) ){
			if(isThreadRunning()) ofLogError("ofxTCPServerNullTerminated") << "couldn't accept client " << acceptId;
		}else{
			std::unique_lock<std::mutex> lck( mConnectionsLock );
			//	take owenership of socket from NewClient
			TCPConnections[acceptId] = client;
            TCPConnections[acceptId]->setupConnectionIdx(acceptId, bClientBlocking);
			TCPConnections[acceptId]->setMessageDelimiter(messageDelimiter);
			ofLogVerbose("ofxTCPServerNullTerminated") << "client " << acceptId << " connected on port " << TCPConnections[acceptId]->getPort();
			if(acceptId == idCount) idCount++;
			serverReady.notify_all();
		}
	}
	idCount = 0;
	std::unique_lock<std::mutex> lck( mConnectionsLock );
	TCPConnections.clear();
	connected = false;
	ofLogVerbose("ofxTCPServerNullTerminated") << "listening thread stopped";
}





