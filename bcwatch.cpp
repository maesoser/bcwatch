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

#define SQL_OUTPUT_OPT 0x01
#define CSV_OUTPUT_OPT 0x02
#define JSON_OUTPUT_OPT 0x03

using easywsclient::WebSocket;
using namespace picojson;

static WebSocket::pointer ws = NULL;
static sqlite3 *db;

std::ofstream ofile;

unsigned char output_opt = JSON_OUTPUT_OPT;

template <typename T>
std::string tostring(const T& t)
{
    std::ostringstream ss;
    ss << std::setprecision(0) << std::fixed << t;
    return ss.str();
}

std::string fdate(time_t utime){
    char buffer[256];
    struct tm * tm_info = localtime(&utime);  
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    return tostring(buffer);
}

void handle_exit(int s){
    printf("Exiting...\n");
    delete ws;
    sqlite3_close(db);
    ofile.close();
    exit(1); 
}

sqlite3_int64 execute_sql(std::string sqlt){
    char *zErrMsg = 0;
    int rc = sqlite3_exec(db, sqlt.c_str(), 0, 0, &zErrMsg);
    if( rc != SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        return -1;
    }
    sqlite3_free(zErrMsg);
    return sqlite3_last_insert_rowid(db);   
}

void handle_message(const std::string & message)
{    
    picojson::value v;
    array inputs;
    array outputs;
    double incash = 0;
    double outcash = 0;
    std::string in_addr;
    std::string out_addr;
    double maxout = 0;
    double maxin = 0;
    double thiscash = 0;
    unsigned int i = 0;
    time_t utime;
    std::string tend = "\033[0m";
    std::string tstart = "\033[1m";
    std::string jsonstr = "";
    double tx_index = 0;
    
    std::string err = picojson::parse(v, message.c_str());
    if (!err.empty()) {
        printf("Error parsing JSON: %s\n",err.c_str());
        return;
    }
    if (!v.is<picojson::object>()) {
        printf("Error: JSON is no an object\n");
        return;
    }
    
    if (v.get<object>()["x"].is<picojson::object>()) {
        object o = v.get<object>()["x"].get<object>();
        if (o["time"].is<double>()) {
            utime = o["time"].get<double>();
        }
        if (o["tx_index"].is<double>()) {
            tx_index = o["tx_index"].get<double>();
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
    
    if(outcash > 0)     tstart = "\033[1m\033[94m";
    if(outcash > 50)    tstart = "\033[1m\033[93m";
    if(outcash > 500)   tstart = "\033[1m\033[91m";
    if(out_addr.length()!=0 && in_addr.length()!=0){
        std::cout << fdate(utime) << "\t" << tstart << in_addr << "  >>  " << out_addr << "\t"<<  outcash << tend << std::endl;
    }
    
       switch(output_opt){
           case SQL_OUTPUT_OPT:{
               std::string sqlt = "INSERT INTO transactions (id,timestamp) VALUES ("+ tostring(tx_index) +",\"" + tostring(utime) + "\");";
               execute_sql(sqlt);
               break;
           }
           case JSON_OUTPUT_OPT:{
               jsonstr = "{\"tid\":"+tostring(tx_index)+",\"ts\":"+tostring(utime)+",\"sources\":[";
               break;
           }
        }    
     
        for (i = 0; i < inputs.size(); i++){
            if(inputs[i].get<object>()["prev_out"].get<object>()["addr"].is<std::string>()){
                std::string addr = inputs[i].get<object>()["prev_out"].get<object>()["addr"].get<std::string>();
                std::string amount = tostring(inputs[i].get<object>()["prev_out"].get<object>()["value"].get<double>());
                
                switch(output_opt){
                    case CSV_OUTPUT_OPT:
                        ofile << tostring(tx_index) << ",SRC,"<< tostring(utime) << "," << addr << "," << amount << std::endl;
                        break;
                    case SQL_OUTPUT_OPT:{
                        std::string sqlt = "INSERT INTO src_addrs (tid,addr,amount) VALUES ("
                            + tostring(tx_index) + ",\"" 
                            + addr+ "\","
                            + amount + ");";
                        execute_sql(sqlt);
                        break;
                    }
                    case JSON_OUTPUT_OPT:{
                        jsonstr += "{\"addr\":\""+ addr +"\",\"val\":"+ amount+"}";
                        if(i!=inputs.size()-1) jsonstr+= ",";
                        break;
                    }
                }
            }   
        }
        jsonstr += "],\"destinations\":[";
        for (i = 0; i < outputs.size(); i++){
            if(outputs[i].get<object>()["addr"].is<std::string>()){
                
                std::string addr = outputs[i].get<object>()["addr"].get<std::string>();
                std::string amount = tostring(outputs[i].get<object>()["value"].get<double>());
                
                switch(output_opt){
                    case CSV_OUTPUT_OPT:
                     ofile << tostring(tx_index) << ",DST,"<< tostring(utime) << "," << addr << "," << amount << std::endl;
                     break;
                    case SQL_OUTPUT_OPT:{
                        std::string sqlt = "INSERT INTO dst_addrs (tid,addr,amount) VALUES ("
                            + tostring(tx_index) + ",\"" 
                            + addr+ "\","
                            + amount + ");";
                        execute_sql(sqlt);
                        break;
                    }
                    case JSON_OUTPUT_OPT:{
                        jsonstr += "{\"addr\":\""+ addr +"\",\"val\":"+ amount+"}";
                        if(i!=outputs.size()-1) jsonstr+= ",";
                        break;   
                    }
                }
            }
        }
        if(output_opt==JSON_OUTPUT_OPT){
            ofile << jsonstr << "]}" << std::endl;
        }
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
    sqlite3_free(zErrMsg);

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
            output_opt = SQL_OUTPUT_OPT;
        }
        else if(std::strcmp(argv[2],"csv")==0){
            printf("CSV output\n");
            output_opt = CSV_OUTPUT_OPT;
        }
        else if(std::strcmp(argv[2],"json")==0){
            printf("JSON output\n");
            output_opt = JSON_OUTPUT_OPT;
        }
    }
    
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = handle_exit;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    std::cout << std::setprecision(4) << std::fixed;

    switch(output_opt){
        case SQL_OUTPUT_OPT:{
            int rc = sqlite3_open((fname + ".db").c_str(), &db);
            if( rc ) {
                fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                exit(0);
            } 
            createDB();
            break;
        }
        case CSV_OUTPUT_OPT:
            ofile.open(fname + ".csv" );
            break;
        case JSON_OUTPUT_OPT:
            ofile.open(fname + ".json" );
            break;
    }

    while(true){
        ws = WebSocket::from_url("ws://ws.blockchain.info/inv");
        if(ws==0) continue;
        //assert(ws);
        usleep(200000);
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
    ofile.close();
    return 0;
}
