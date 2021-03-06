#include "config.h"
#include "Server.h"
#include "Socket.h"
#include "Client.h"
	
using namespace std;

/***************
constructor 
****************/
Server::Server(int port):  Socket(port){
}

/***************
Session handler, will handle session differently for front end and back end
type =0, front end, type =1 back end; 
****************/

void Server::Handle_session(int new_fd, int type){	

	
	if(type==0){
		// Connect to all back ends
		// create N socket and connect to backend as client 
		for(int i=0; i<N; i++){
			backends.push_back(Client(3001+i));			// use backend.sockfd;
			int ans= backends[i].Connect_to_server();
			if(ans==-1){	
				cout << "Back end is not available, exiting" << endl;
				exit(-1);
			}
			else{
				cout << "Connected to backend" <<endl;
			}
		}
 	}
	else
	{
	// connect to the database
	this->create_database();
	
	}
	try{
		int byte=1;
		string action, key, value;
		string success;
		string not_found="NOT_FOUND";
		while(1){
			action=this->Read(new_fd);
			if(action=="PUT"){
				key	 = this->Read(new_fd);
				value= this->Read(new_fd);
				if(type==0){		// process data as front end
					success=this->add_to_catch(key,value);
					//send data to persistent
					success=this->send_to_persistent(key,value);
					
				}
				else{				// process data as back end
					success=this->add_to_persistent(key,value);
					
				}
				this->Write(success,new_fd);
			}
			else if(action=="GET"){
				key	 = this->Read(new_fd);
				if(type==0){		// process data as front end
					value=this->look_in_catch(key);
					cout<< "Served by Cache"<< endl;
					// if value= not_found get it from persistent
					if(value==not_found)
						value=this->get_from_persistent(key);
				}
				else{				// process data as back end
					value=this->look_in_persistent(key);
					cout<< "Served by Persistent"<< endl;
				}
				this->Write(value,new_fd);
			}
		}
	}
	catch (exception& e)
	{
		cout << "Standard exception: " << e.what() << endl;
	}
}


/**************
Add key value to map (catch)
**************/

string Server::add_to_catch(string key, string value){ //used by front end
			string success="SUCCESS";
			mtx.lock();				//mutex
			mymap[key]=value;	
			if (mymap.size()> MAX_CATCH_SIZE){
				mymap.erase(mymap.begin());
			}
			mtx.unlock();
			return success;
}

/**************
Look for value in catch
**************/

string Server::look_in_catch(string key){			// used by front end
			string not_found="NOT_FOUND";
			try{
				return mymap.at(key);
			}
			catch (const out_of_range& oor) {	
				return not_found;
			}
}

/**************
send key value pair to back end to add
**************/
string Server::send_to_persistent(string key, string value){		//used by front end
	cout<<".";cout << flush;
	string success ="SUCCESS";string reply;
	int arr[N];
	for(int i=0;i<N;i++)
		arr[i]=i;
	random_shuffle(&arr[0], &arr[N]);

	// front end will send data to back end 
	// front end will use client objects to do so
	// Use client function Get_func and Put_func
	for(int i=0; i<W; i++){
			reply=backends[arr[i]].Put_func(key,value);
	}
	return success;
}


/**************
Get value from back end
**************/
string Server::get_from_persistent(string key){				// used by front end
	// front end will receive data to back end 
	// front end will use client objects to do so
	// Use client function Get_func and Put_func
	int arr[N];
	for(int i=0;i<N;i++)
		arr[i]=i;
	random_shuffle(&arr[0], &arr[N]);

	return backends[arr[0]].Get_func(key);
}


/**************
back end will write to database
**************/
string Server::add_to_persistent(string key, string value){			//used by backend
			cout<<"-";cout << flush;
			string reply=this->add_to_database(key,value);			
			return reply;
}

/**************
back end will read from database
**************/
string Server::look_in_persistent(string key){		//used by backend
			cout<<".";cout << flush;
			string not_found="NOT_FOUND";
			try{
				string reply=this->get_from_database(key);
				if(reply.size())
					return reply;
				else
					return not_found;
				
			}
			catch (const out_of_range& oor) {	
				return not_found;
			}
}


// DATABASE FUNCTIONS:


/**************
Add key value to database
**************/
string Server::add_to_database(string key, string value){
	char *zErrMsg = 0;
    int  rc;
    char sql[1000];
	char data[1000];
	string failure="FAILURE";
	string success="SUCCCESS";
	
	//check if key exist
	string exists=get_from_database(key);
	if(exists.size()){
	 //update	
		string s="UPDATE KEY_VALUE set value_entry = '" + value +"' where key_entry = '"+ key + "'; ";
		strcpy(sql, s.c_str());
		rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
	}
	else{
	 //insert
		string q= "INSERT INTO KEY_VALUE (key_entry, value_entry) VALUES ('" + key + "', '" + value + "' ); ";
		strcpy(sql,q.c_str());
		rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
		if( rc != SQLITE_OK ){
			//fprintf(stderr, "SQL ERROR2: %s\n", sqlite3_errmsg(db)); 
			//sqlite3_free(zErrMsg);
      		fprintf(stderr, "KEY not found as well as can't be inserted %s\n", zErrMsg);
			return failure;		
		}
	}
	return success;
}


/**************
Get value from database
**************/
string Server::get_from_database(string key){
	char *zErrMsg = 0;
    int  rc;
    char sql[1000];
	char data[1000]={};
	string failure="FAILURE";
	string success="SUCCCESS";
	
	// create select query
	string s= "SELECT value_entry from KEY_VALUE where key_entry='"+ key + "';";
	strcpy(sql, s.c_str());
	rc = sqlite3_exec(db, sql, callback, (void*)data, &zErrMsg);
	return string(data);

}

/**************
Create/conenct to database
**************/
void Server::create_database(){
	cout << "inside create_database " <<endl;
	int  rc;
	char *zErrMsg = 0;
	char sql[1000];
	// connect to database
	rc = sqlite3_open("server_511.db", &db);
	if( rc ){
    	fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    	return;
   	}else{
    	fprintf(stdout, "Opened database successfully\n");
   	}

	/* Create TABLE */
	string s = "CREATE TABLE if not exists KEY_VALUE("  \
	"key_entry varchar(1000)  UNIQUE NOT NULL," \
	"value_entry varchar(1000) NOT NULL );";
	
	strcpy(sql, s.c_str());
	/* Execute SQL statement */
	while(1){
		rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
		if( rc != SQLITE_OK ){
			fprintf(stderr, "SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
			usleep(10000);
		}else{
			fprintf(stdout, "Table created successfully\n");
			break;
		}	
	}

}

/**************
Callback function for database
**************/
int Server::callback(void *data, int argc, char **argv, char **azColName){
   char * sal= (char*)data;
   //cout << "ARGC" << argc << endl;
   strcpy(sal,argv[0]);
   return 0;
}





