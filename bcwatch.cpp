#include "easywsclient.hpp"
//#include "easywsclient.cpp" // <-- include only if you don't want compile separately

#include <assert.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include "picojson.h"
#include <iomanip>
#include <sqlite3.h> 


using easywsclient::WebSocket;
static WebSocket::pointer ws = NULL;
static sqlite3 *db;

using namespace picojson;

template <typename T>
std::string tostring(const T& t)
{
    std::ostringstream ss;
    ss << t;
    return ss.str();
}

void handle_exit(int s){
    printf("Exiting...\n");
    delete ws;
    sqlite3_close(db);
    exit(1); 

}

void handle_message(const std::string & message)
{
    //printf(">>> %s\n", message.c_str());
    
    picojson::value v;
    struct tm* tm_info;
    double incash = 0;
    double outcash = 0;
    array inputs;
    array outputs;
    std::string in_addr;
    std::string out_addr;
    double maxout = 0;
    double maxin = 0;
    double thiscash = 0;
    unsigned int i = 0;

    std::string err = picojson::parse(v, message.c_str());
    if (!err.empty()) {
        printf("Error parsing JSON: %s\n",err.c_str());
        return;
    }
    
    // check if the type of the value is "object"
    if (!v.is<picojson::object>()) {
        printf("Error: JSON is no an object\n");
        return;
    }
    
    std::cout << std::setprecision(4) << std::fixed;
    time_t utime;
    
    if (v.get<object>()["x"].is<picojson::object>()) {
        object o = v.get<object>()["x"].get<object>();
        if (o["time"].is<double>()) {
            utime = o["time"].get<double>();
        }
        inputs = o["inputs"].get<array>();
        for (i = 0; i < inputs.size(); i++){
           thiscash = inputs[i].get<object>()["prev_out"].get<object>()["value"].get<double>()/ 100000000;
           incash += thiscash;
           if(thiscash > maxin){
               maxin = thiscash;
               if (inputs[i].get<object>()["prev_out"].get<object>()["addr"].is<std::string>())
                in_addr = inputs[i].get<object>()["prev_out"].get<object>()["addr"].get<std::string>();
           }
        }
         
        outputs = o["out"].get<array>();
        for (i = 0; i < outputs.size(); i++){
           thiscash = outputs[i].get<object>()["value"].get<double>()/ 100000000;
           outcash += thiscash;
           if(thiscash > maxout){
               maxout = thiscash;
               if(outputs[i].get<object>()["addr"].is<std::string>())
                out_addr = outputs[i].get<object>()["addr"].get<std::string>();
           }
        }
    }
    
    char buffer[256];
    std::string tstart = "";
    std::string tend = "";
    tm_info = localtime(&utime);  
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    
    if(outcash > 50){
        tend = "\033[0m";
        tstart = "\033[1m\033[93m";

    }
    if(outcash > 500){
        tend = "\033[0m";
        tstart = "\033[1m\033[91m";
    }
    //std::cout << buffer << "\t" << tstart << outcash << tend << "\t(" << incash - outcash << ")\t"<< in_addr << " >> " << out_addr <<std::endl;
    if(out_addr.length()!=0 && in_addr.length()!=0){
        std::cout << buffer << "\t" << tstart << in_addr << "  >>  " << out_addr << "\t"<<  outcash << tend << std::endl;
    }
    
           // Execute SQL statement
       char *zErrMsg = 0;
       std::string sqlt = "INSERT INTO TRANSACTIONS (TIMESTAMP) VALUES (\"" + tostring(buffer) + "\");";
       int rc = sqlite3_exec(db, sqlt.c_str(), 0, 0, &zErrMsg);
       sqlite3_int64 tid = sqlite3_last_insert_rowid(db);   
       if( rc != SQLITE_OK ){
          fprintf(stderr, "SQL error: %s\n", zErrMsg);
          sqlite3_free(zErrMsg);
          return;
       }
     
        for (i = 0; i < inputs.size(); i++){
            if(inputs[i].get<object>()["prev_out"].get<object>()["addr"].is<std::string>()){
                std::string sqlt = "INSERT INTO SRC_ADDRESSES (TID,ADDR,AMOUNT) VALUES ("
                    + tostring(tid) + ",\"" 
                    + inputs[i].get<object>()["prev_out"].get<object>()["addr"].get<std::string>()+ "\","
                    + tostring(inputs[i].get<object>()["prev_out"].get<object>()["value"].get<double>()) + ");";
                int rc = sqlite3_exec(db, sqlt.c_str(), 0, 0, &zErrMsg);
                if( rc != SQLITE_OK ){
                    fprintf(stderr, "SQL error: %s\n", zErrMsg);
                    sqlite3_free(zErrMsg);
                    return;
                }
            }   
        }
        
        for (i = 0; i < outputs.size(); i++){
            if(outputs[i].get<object>()["addr"].is<std::string>()){
                std::string sqlt = "INSERT INTO DST_ADDRESSES (TID,ADDR,AMOUNT) VALUES ("
                    + tostring(tid) + ",\"" 
                    + outputs[i].get<object>()["addr"].get<std::string>()+ "\","
                    + tostring(outputs[i].get<object>()["value"].get<double>()) + ");";
                int rc = sqlite3_exec(db, sqlt.c_str(), 0, 0, &zErrMsg);
                if( rc != SQLITE_OK ){
                    fprintf(stderr, "SQL error: %s\n", zErrMsg);
                    sqlite3_free(zErrMsg);
                    return;
                }
            }
        }
        
   
}

void createDB(){
    char *zErrMsg = 0;
    int rc;
    std::string sql_transaction;
           /* Create SQL statement */
   sql_transaction = std::string("CREATE TABLE IF NOT EXISTS TRANSACTIONS("  \
         "ID INT PRIMARY KEY," \
         "TIMESTAMP DATETIME NOT NULL);");

   /* Execute SQL statement */
   rc = sqlite3_exec(db, sql_transaction.c_str(), 0, 0, &zErrMsg);
   
   if( rc != SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        exit(0);
   } else {
        fprintf(stdout, "Table created successfully\n");
   }
   
    printf("Checking src_addresses table\n");
   sql_transaction = std::string("CREATE TABLE IF NOT EXISTS SRC_ADDRESSES("  \
         "ID INT PRIMARY KEY," \
         "TID INTEGER NOT NULL," \
         "ADDR DOUBLE NOT NULL," \
         "AMOUNT TEXT NOT NULL," \
         "FOREIGN KEY(TID) REFERENCES TRANSACTIONS(ID))");


   /* Execute SQL statement */
   rc = sqlite3_exec(db, sql_transaction.c_str(), 0, 0, &zErrMsg);
   
   if( rc != SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        exit(0);
   } else {
        fprintf(stdout, "Table created successfully\n");
   }
   
   printf("Checking dst_addresses table\n");
   sql_transaction = std::string("CREATE TABLE IF NOT EXISTS DST_ADDRESSES("  \
         "ID INT PRIMARY KEY," \
         "TID INTEGER NOT NULL," \
         "ADDR DOUBLE NOT NULL," \
         "AMOUNT TEXT NOT NULL," \
         "FOREIGN KEY(TID) REFERENCES TRANSACTIONS(ID))");


   /* Execute SQL statement */
   rc = sqlite3_exec(db, sql_transaction.c_str(), 0, 0, &zErrMsg);
   
   if( rc != SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        exit(0);
   } else {
        fprintf(stdout, "Table created successfully\n");
   }
   

}
int main()
{
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = handle_exit;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);
   
    int rc = sqlite3_open("transactions.db", &db);

    if( rc ) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(0);
    } else {
        fprintf(stderr, "Opened database successfully\n");
    }
    
    createDB();
    
    while(true){
        ws = WebSocket::from_url("ws://ws.blockchain.info/inv");
        assert(ws);
        usleep(250000);
        printf("Suscribing\n");
        ws->send("{\"op\":\"unconfirmed_sub\"}");
        while (ws->getReadyState() != WebSocket::CLOSED) {
          ws->poll();
          ws->dispatch(handle_message);
        }
        usleep(1000000);
    }
    
    delete ws;
    sqlite3_close(db);
    return 0;
}
