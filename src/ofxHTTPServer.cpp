/*
 * ofxHTTPServer.cpp
 *
 *  Created on: 16-may-2009
 *      Author: art
 */

#include "ofxHTTPServer.h"
#include <cstring>
#include <fstream>
#include <map>

using namespace std;

ofxHTTPServer ofxHTTPServer::instance;

// Helper functions and structures only used internally by the server
//------------------------------------------------------
static const int GET = 0;
static const int POST = 1;
static const unsigned POSTBUFFERSIZE = 4096;
static const char* CONTENT_TYPE = "Content-Type";

class connection_info{
	static int id;
public:
	connection_info(){
		conn_id = ++id;
	}

	map<string,string> fields;
	map<string,FILE*> file_fields;
	int connectiontype;
	bool connection_complete;
	struct MHD_PostProcessor *postprocessor;
	int conn_id;
	char new_content_type[1024];

};

class fs_file{
public:
	fs_file(){
		buffer = NULL;
		length = -1;
	}

	~fs_file(){
		//if(buffer) delete[] buffer;
	}
	char * buffer;
	long length;
};

// helper functions

static fs_file getFileContent(const string & path){
	fs_file file;

	ifstream indata; // indata is like cin
	indata.open( path.c_str() ); // opens the file
	if(!indata) return file;


	indata.seekg (0, ios::end);
	file.length = indata.tellg();
	indata.seekg (0, ios::beg);

	// allocate memory:
	file.buffer = new char [file.length];

	// read data as a block:
	indata.read (file.buffer,file.length);
	indata.close();

	return file;
}

int connection_info::id=0;


// private methods

int ofxHTTPServer::print_out_key (void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
  printf ("%s = %s\n", key, value);
  return MHD_YES;
}

int ofxHTTPServer::iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
                  const char *filename, const char *content_type,
                  const char *transfer_encoding, const char *data, uint64_t off, size_t size)
{
	connection_info *con_info = (connection_info*) coninfo_cls;

	cout << "processing connection " << con_info->conn_id << endl;
	if (size > 0){
		cout << "processing post field of size: "<< size <<","<< endl;
		if(transfer_encoding)
			cout << " transfer encoding: "<< transfer_encoding<< endl;
		if(content_type)
			cout << ", content: " << content_type<< endl;
		if(filename)
			cout << ", filename: "<< filename<< endl;

		cout << ", "  << key << endl;//": " <<data << endl;

		if(!filename){
		 char * aux_data = new char[off+size+1];
		 memset(aux_data,0,size+1);
		 if(off > 0)
			memcpy(aux_data,con_info->fields[key].c_str(),off);

		 memcpy(aux_data+off*sizeof(char),data,size);
		 con_info->fields[key] = aux_data;
	  }else{
			if(con_info->file_fields.find(filename)==con_info->file_fields.end()){
				con_info->file_fields[filename] = NULL;
				con_info->file_fields[filename] = fopen ((instance.uploadDir +"/"+ filename).c_str(), "ab");
				if(con_info->file_fields[filename] == NULL){
					con_info->file_fields.erase(filename);
					return MHD_NO;
				}
			}
			if(size>0){
				if (!fwrite (data, size, sizeof(char), con_info->file_fields[filename])){
					cout << "ofxHttpServer:" << "error on writing" << endl;
					con_info->file_fields.erase(filename);
					return MHD_NO;
				}
			}

		}
	}
	return MHD_YES;


}

void ofxHTTPServer::request_completed (void *cls, struct MHD_Connection *connection, void **con_cls,
                        enum MHD_RequestTerminationCode toe)
{
  connection_info *con_info = (connection_info*) *con_cls;


  if (NULL == con_info) return;

  if (con_info->connectiontype == POST){
      MHD_destroy_post_processor (con_info->postprocessor);
  }



  delete con_info;
  *con_cls = NULL;

  instance.numClients--;
}

int ofxHTTPServer::send_page (struct MHD_Connection *connection, long length, const char* page, int status_code)
{
  int ret;
  struct MHD_Response *response;


  response = MHD_create_response_from_data (length, (void*) page, MHD_NO, MHD_YES);
  if (!response) return MHD_NO;

  ret = MHD_queue_response (connection, status_code, response);
  MHD_destroy_response (response);

  return ret;
}



// public methods
//------------------------------------------------------
ofxHTTPServer::ofxHTTPServer() {
	callbackExtensionSet = false;
	maxClients = 10;
	numClients = 0;
	uploadDir = ofToDataPath("",true);
}

ofxHTTPServer::~ofxHTTPServer() {
	// TODO Auto-generated destructor stub
}



int ofxHTTPServer::answer_to_connection(void *cls,
			struct MHD_Connection *connection, const char *url,
			const char *method, const char *version, const char *upload_data,
			size_t *upload_data_size, void **con_cls) {

	string strmethod = method;

	connection_info  * con_info;

	// to process post we need several iterations, first we set a connection info structure
	// and return MHD_YES, that will make the server call us again
	if(NULL == *con_cls){
		con_info = new connection_info;
		instance.numClients++;

		// super ugly hack to manage poco multipart post connections as it sets boundary between "" and
		// libmicrohttpd doesn't seem to support that
		string contentType;
		if(MHD_lookup_connection_value(connection, MHD_HEADER_KIND, CONTENT_TYPE)!=NULL)
			contentType = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, CONTENT_TYPE);
		if ( contentType.substr(0,31) == "multipart/form-data; boundary=\""){
			contentType = "multipart/form-data; boundary="+contentType.substr(31,contentType.size()-32);
			cout << "changing content type: " << contentType << endl;
			strcpy(con_info->new_content_type,contentType.c_str());
			MHD_set_connection_value(connection,MHD_HEADER_KIND,CONTENT_TYPE,con_info->new_content_type);
		}
		MHD_get_connection_values (connection, MHD_HEADER_KIND, print_out_key, NULL);



		if (instance.numClients >= instance.maxClients){
			fs_file file;
			file = getFileContent("503.html");
			if(!file.buffer){
				file.buffer = new char[100];
				strcpy(file.buffer, "error 503 - server too busy");
				file.length = strlen(file.buffer);
			}
			cout << "httpServer: error max clients"<<endl;
		    return send_page(connection, file.length, file.buffer, MHD_HTTP_SERVICE_UNAVAILABLE);
		}

		if(strmethod=="GET"){
			con_info->connectiontype = GET;
		}else if (strmethod=="POST"){


			con_info->postprocessor = MHD_create_post_processor (connection, POSTBUFFERSIZE, iterate_post, (void*) con_info);

			if (NULL == con_info->postprocessor)
			{
				cout << "error creating postprocessor" << endl;
			  delete con_info;
			  return MHD_NO;
			}

			con_info->connectiontype = POST;
		}

		*con_cls = (void*) con_info;
		return MHD_YES;
	}


	// second and next iterations
	string strurl = url;
	int ret = MHD_HTTP_SERVICE_UNAVAILABLE;


	// if the extension of the url is that set to the callback, call the events to generate the response
	if(instance.callbackExtensionSet && strurl.substr(strurl.size()-instance.callbackExtension.size())==instance.callbackExtension){
		cout << method << " serving from callback: " << url << endl;

		ofxHTTPServerResponse response;
		response.url = strurl;


		if(strmethod=="GET"){
			ofNotifyEvent(instance.getEvent,response);

			ret = send_page(connection, response.response.size(), response.response.c_str(), MHD_HTTP_OK);
		}else if (strmethod=="POST"){
			connection_info *con_info = (connection_info *)*con_cls;

			if (*upload_data_size != 0){
				ret = MHD_post_process(con_info->postprocessor, upload_data, *upload_data_size);
				*upload_data_size = 0;

			}else{
				cout << "upload_data_size =  0" << endl;
				response.requestFields = con_info->fields;
				map<string,FILE*>::iterator it;
				  for(it=con_info->file_fields.begin();it!=con_info->file_fields.end();it++){
					  if(it->second!=NULL){
						  fflush(it->second);
						  fclose(it->second);
						  response.uploadedFiles.push_back(it->first);
					  }
				  }
				ofNotifyEvent(instance.postEvent,response);

				ret = send_page(connection, response.response.size(), response.response.c_str(), MHD_HTTP_OK);
			}

		}

	// if the extension of the url is any other try to serve a file
	}else{
		cout << method << " serving from filesystem: " << url << endl;

		fs_file file;

		file = getFileContent(instance.fsRoot + url);

		if(!file.buffer) { // file couldn't be opened
			cerr << "Error: file could not be opened trying to serve 404.html" << endl;
			file = getFileContent(ofToDataPath("404.html", true));
			if(!file.buffer){
				file.buffer = new char[100];
				strcpy(file.buffer, "error 404 - page not found");
				file.length = strlen(file.buffer);
			}
			ret = send_page(connection, file.length, file.buffer, MHD_HTTP_NOT_FOUND);
		}else{
			ret = send_page(connection, file.length, file.buffer, MHD_HTTP_OK);
		}

		if(file.buffer)	delete[] file.buffer;
	}

	return ret;

}


void ofxHTTPServer::start(unsigned _port) {
	port = _port;
	http_daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, _port, NULL, NULL,
			&answer_to_connection, NULL, MHD_OPTION_NOTIFY_COMPLETED,
            &request_completed, NULL, MHD_OPTION_END);
}

void ofxHTTPServer::stop(){
	MHD_stop_daemon(http_daemon);
}

void ofxHTTPServer::setServerRoot(const string & fsroot){
	fsRoot = ofToDataPath(fsroot,true);
	cout << "serving files at " << fsRoot << endl;
}


void ofxHTTPServer::setUploadDir(const string & uploaddir){
	uploadDir = ofToDataPath(uploaddir,true);
	cout << "uploading posted files to " << uploadDir << endl;
}

void ofxHTTPServer::setCallbackExtension(const string & cb_extension){
	callbackExtensionSet = true;
	callbackExtension = cb_extension;
}


void ofxHTTPServer::setMaxNumberClients(unsigned num_clients){
	maxClients = num_clients;
}
