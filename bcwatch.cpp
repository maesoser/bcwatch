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
using namespace picojson;


static WebSocket::pointer ws = NULL;
static sqlite3 *db;

std::ofstream src_out;
std::ofstream dst_out;

long long tindex;

bool csv = true;
bool sql = true;

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
    src_out.close();
    dst_out.close();
    exit(1); 

}

void handle_message(const std::string & message)
{    
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
    char *zErrMsg = 0;

    std::string err = picojson::parse(v, message.c_str());
    if (!err.empty()) {
        printf("Error parsing JSON: %s\n",err.c_str());
        return;
    }
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
    if(out_addr.length()!=0 && in_addr.length()!=0){
        std::cout << buffer << "\t" << tstart << in_addr << "  >>  " << out_addr << "\t"<<  outcash << tend << std::endl;
    }
    sqlite3_int64 tid;
       if(sql){
           std::string sqlt = "INSERT INTO transactions (timestamp) VALUES (\"" + tostring(buffer) + "\");";
           int rc = sqlite3_exec(db, sqlt.c_str(), 0, 0, &zErrMsg);
           tid = sqlite3_last_insert_rowid(db);   
           if( rc != SQLITE_OK ){
              fprintf(stderr, "SQL error: %s\n", zErrMsg);
              sqlite3_free(zErrMsg);
              return;
           }
        }    
     
        for (i = 0; i < inputs.size(); i++){
            if(inputs[i].get<object>()["prev_out"].get<object>()["addr"].is<std::string>()){
                std::string addr = inputs[i].get<object>()["prev_out"].get<object>()["addr"].get<std::string>();
                std::string amount = tostring(inputs[i].get<object>()["prev_out"].get<object>()["value"].get<double>());
                
                if(csv) src_out  << tostring(tindex) << ","<< tostring(utime) << "," << addr << "," << amount << std::endl;
                
                if(sql){
                    std::string sqlt = "INSERT INTO src_addrs (tid,addr,amount) VALUES ("
                        + tostring(tid) + ",\"" 
                        + addr+ "\","
                        + amount + ");";
                    int rc = sqlite3_exec(db, sqlt.c_str(), 0, 0, &zErrMsg);
                    if( rc != SQLITE_OK ){
                        fprintf(stderr, "SQL error: %s\n", zErrMsg);
                        sqlite3_free(zErrMsg);
                        return;
                    }
                }
            }   
        }
        
        for (i = 0; i < outputs.size(); i++){
            if(outputs[i].get<object>()["addr"].is<std::string>()){
                
                std::string addr = outputs[i].get<object>()["addr"].get<std::string>();
                std::string amount = tostring(outputs[i].get<object>()["value"].get<double>());
                
                if(csv) dst_out  << tostring(tindex) << ","<< tostring(utime) << "," << addr << "," << amount << std::endl;
                
                if(sql){
                    std::string sqlt = "INSERT INTO dst_addrs (tid,addr,amount) VALUES ("
                        + tostring(tid) + ",\"" 
                        + addr+ "\","
                        + amount + ");";
                    int rc = sqlite3_exec(db, sqlt.c_str(), 0, 0, &zErrMsg);
                    if( rc != SQLITE_OK ){
                        fprintf(stderr, "SQL error: %s\n", zErrMsg);
                        sqlite3_free(zErrMsg);
                        return;
                    }
                }
            }
        }
        tindex += 1;
}

void createDB(){
    char *zErrMsg = 0;
    int rc;
    std::string sql_transaction;
           /* Create SQL statement */
   sql_transaction = std::string("CREATE TABLE IF NOT EXISTS transactions("  \
         "id INT PRIMARY KEY," \
         "timestamp DATETIME NOT NULL);");

   /* Execute SQL statement */
   rc = sqlite3_exec(db, sql_transaction.c_str(), 0, 0, &zErrMsg);
   
   if( rc != SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        exit(0);
   }
   
   sql_transaction = std::string("CREATE TABLE IF NOT EXISTS src_addrs("  \
         "id INT PRIMARY KEY," \
         "tid INTEGER NOT NULL," \
         "addr DOUBLE NOT NULL," \
         "amount TEXT NOT NULL," \
         "FOREIGN KEY(tid) REFERENCES TRANSACTIONS(id))");

   rc = sqlite3_exec(db, sql_transaction.c_str(), 0, 0, &zErrMsg);
   
   if( rc != SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        exit(0);
   }
   
   sql_transaction = std::string("CREATE TABLE IF NOT EXISTS dst_addrs("  \
         "id INT PRIMARY KEY," \
         "tid INTEGER NOT NULL," \
         "addr DOUBLE NOT NULL," \
         "amount TEXT NOT NULL," \
         "FOREIGN KEY(tid) REFERENCES TRANSACTIONS(id))");


   /* Execute SQL statement */
   rc = sqlite3_exec(db, sql_transaction.c_str(), 0, 0, &zErrMsg);
   
   if( rc != SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        exit(0);
   }
   

}
int main(int argc, char* argv[])
{   
    
    std::string fname = "transactions";
    if (argc > 1) {
        fname = argv[1];
    }
    if(argc == 3){
        if(std::strcmp(argv[2],"sql")==0){
            printf("SQl output\n");
            csv = false;
        }
        if(std::strcmp(argv[2],"csv")==0){
            printf("CSV output\n");
            sql = false;
        }
    }
    
    
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = handle_exit;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);
   
    if(sql){
        int rc = sqlite3_open((fname + ".db").c_str(), &db);
        if( rc ) {
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
            exit(0);
        } 
        createDB();

    }
    if(csv){
        src_out.open(fname + "_src.csv" );
        dst_out.open(fname + "_dst.csv" );
    }

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
    src_out.close();
    dst_out.close();
    return 0;
}
