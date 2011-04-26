/*
 * ofxHTTPServer.h
 *
 *  Created on: 16-may-2009
 *      Author: art
 */

#ifndef OFXHTTPSERVER_H_
#define OFXHTTPSERVER_H_

#include <cstdarg>
#include <sys/socket.h>
#include "microhttpd.h"
#include <ofMain.h>
#include <map>

class ofxHTTPServerResponse{
public:
	ofxHTTPServerResponse(){
		errCode=200;
	}
	string url;
	string response;
	int errCode;
	string errStr;
	string location; // for redirections

	std::map<string,string> requestFields;
	std::map<string,string> uploadedFiles;
};


class ofxHTTPServer {
public:
	static ofxHTTPServer * getServer() {
		return &instance;
	}

	virtual ~ofxHTTPServer();


	void start(unsigned port = 8888);
	void stop();
	void setServerRoot(const string & fsroot);
	void setUploadDir(const string & uploadDir);
	void setCallbackExtension(const string & cb_extension);
	void setMaxNumberClients(unsigned num_clients);

	ofEvent<ofxHTTPServerResponse> getEvent;
	ofEvent<ofxHTTPServerResponse> postEvent;
	ofEvent<ofxHTTPServerResponse> fileNotFoundEvent;


	static int answer_to_connection(void *cls,
				struct MHD_Connection *connection, const char *url,
				const char *method, const char *version, const char *upload_data,
				size_t *upload_data_size, void **con_cls);

private:
	static ofxHTTPServer instance;

	ofxHTTPServer();
	struct MHD_Daemon *http_daemon;
	string fsRoot;
	string uploadDir;
	string callbackExtension;
	bool callbackExtensionSet;
	unsigned port;
	unsigned numClients;
	unsigned maxClients;

	static int print_out_key (void *cls, enum MHD_ValueKind kind, const char *key, const char *value);

	static int send_page (struct MHD_Connection *connection, long length, const char* page, int status_code);
	static int send_redirect (struct MHD_Connection *connection, const char* location, int status_code);

	static void request_completed (void *cls, struct MHD_Connection *connection, void **con_cls,
	                        enum MHD_RequestTerminationCode toe);

	static int iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
							const char *filename, const char *content_type,
							const char *transfer_encoding, const char *data, uint64_t off, size_t size);


};

#endif /* OFXHTTPSERVER_H_ */
