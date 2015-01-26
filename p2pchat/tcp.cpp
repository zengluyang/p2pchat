//
//  tcp.cpp
//  p2pchat
//
//  Created by ZLY on 15/1/26.
//  Copyright (c) 2015年 ZLY. All rights reserved.
//
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <map>
#include <fstream>

#include "tcp.h"
#include "json.h"
extern std::map<std::string,std::string> live_user_list;
extern std::string username;
static pthread_t recv_message_secret_thread;
static pthread_t send_message_secret_thread;

static std::fstream ofstream_secret;
static std::fstream ifstream_secret;

void init_tcp_db() {
    ofstream_secret.open("secret.db", std::ios_base::app);
    ifstream_secret.open("secret.db", std::ios_base::in);
}

void init_tcp_server(){
    init_tcp_db();
    pthread_create(&recv_message_secret_thread, NULL, recv_message_secret,NULL);
    pthread_create(&send_message_secret_thread, NULL, send_message_secret,NULL);
}

void destroy_tcp_server() {
    pthread_cancel(recv_message_secret_thread);
    pthread_cancel(send_message_secret_thread);
}

void tcp_send(std::string ip,std::string message) {
    int sockfd = 0, n = 0;
    char recvBuff[1024];
    struct sockaddr_in serv_addr;
    
    
    memset(recvBuff, '0',sizeof(recvBuff));
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return ;
    }
    
    memset(&serv_addr, '0', sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(TCP_LISTEN_PORT);
    
    if(inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return ;
    }
    
    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connect");
        return ;
    }
    if( ::send(sockfd, message.c_str(), message.length()+1, 0)<0){
        printf("\n Error : Send Failed \n");
        return ;
    }
    close(sockfd);
    printf("TCP SEND: %s\n",message.c_str());

}

void* recv_message_secret(void* thread_id){

    struct sockaddr_in serv_addr;
    int listenfd = 0;

    char sendBuff[1025];
    
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));
    memset(sendBuff, '0', sizeof(sendBuff));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(TCP_LISTEN_PORT);
    
    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    
    listen(listenfd, 10);
    
    while(1)
    {
        int connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
        ssize_t bytes_recieved;
        char incoming_data_buffer[4096];
        bytes_recieved = recv(connfd, incoming_data_buffer,4095, 0);
        incoming_data_buffer[bytes_recieved] = '\0';
        printf("TCP RECV: %s\n",incoming_data_buffer);
        std::string packet(incoming_data_buffer);
        on_recv(packet);
        close(connfd);
    }
    return NULL;
}
static std::queue<SecretMessage> send_sercert_message_queue;

static pthread_mutex_t send_secret_message_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t send_secret_message_condvar = PTHREAD_COND_INITIALIZER;

void* send_message_secret(void* thread_id){
    while (1){
        pthread_mutex_lock(&send_secret_message_queue_mutex);
        pthread_cond_wait(&send_secret_message_condvar, &send_secret_message_queue_mutex);
        if(!send_sercert_message_queue.empty()){
            //std::cout<<"send_message_broadcast reading from queue.."<<std::endl;
            SecretMessage sm = send_sercert_message_queue.front();
            send_sercert_message_queue.pop();
            std::string name = sm.name;
            std::string message = sm.message;
            pthread_mutex_unlock(&send_secret_message_queue_mutex);
            printf("live_user_list[%s]:%s\n",name.c_str(),live_user_list[name].c_str());
            tcp_send(live_user_list[name],message);
            
            //send to network
        }
    }
    return NULL;

}

void on_secret_message(std::string name,std::string message){
    SecretMessage sm;
    sm.name=name;
    nlohmann::json j;
    j["type"]="secret";
    j["name"]=username;
    j["data"]=message;
    sm.message=j.dump();
    push_to_queue_and_signal(sm);
    nlohmann::json log;
    log["type"] = "secret";
    log["srcname"] = username;
    log["dstname"] = name;
    log["data"] = message;
    char time_buff[1024];
    time_t now;
    strftime(time_buff, 1024, "%Y-%m-%d %H:%M:%S", localtime(&now));
    std::string time(time_buff);
    log["time"] = time;
    ofstream_secret<<log.dump()<<std::endl;

    
}

void send_secret_request(std::string name){
    nlohmann::json j;
    j["type"]="request_secret";
    j["dstname"]=name;
    j["srcname"]=username;
    SecretMessage sm;
    sm.name=name;
    sm.message=j.dump();
    push_to_queue_and_signal(sm);
}

void send_secret_accept(std::string name) {
    nlohmann::json j;
    j["type"]="accept_secret";
    j["dstname"]=name;
    j["srcname"]=username;
    SecretMessage sm;
    sm.name=name;
    sm.message=j.dump();
    push_to_queue_and_signal(sm);


}

void send_secret_decline(std::string name) {
    nlohmann::json j;
    j["type"]="decline_secret";
    j["dstname"]=name;
    j["srcname"]=username;
    SecretMessage sm;
    sm.name=name;
    sm.message=j.dump();
    push_to_queue_and_signal(sm);
    
}

void push_to_queue_and_signal(SecretMessage &sm) {
    pthread_mutex_lock(&send_secret_message_queue_mutex);
    send_sercert_message_queue.push(sm);
    pthread_mutex_unlock(&send_secret_message_queue_mutex);
    pthread_cond_signal(&send_secret_message_condvar);
}

void change_state(input_state state) {
    pthread_mutex_lock(&stdin_mutex);
    app_state = state;
    pthread_cond_signal(&stdin_condvar);
    pthread_mutex_unlock(&stdin_mutex);
}

void on_recv(std::string &packet) {
    nlohmann::json j = nlohmann::json::parse(packet);
    std::string type=j["type"];
    char time_buff[1024];
    time_t now = time(NULL);
    strftime(time_buff, 1024, "%H:%M:%S", localtime(&now));
    if(type=="secret") {
        std::string name = j["name"];
        std::string data = j["data"];
        printf("[M][%s][%s] says to you: %s\n",time_buff,name.c_str(),data.c_str());
        nlohmann::json log;
        log["type"] = type;
        log["srcname"] = name;
        log["dstname"] = username;
        log["data"] = data;
        strftime(time_buff, 1024, "%Y-%m-%d %H:%M:%S", localtime(&now));
        std::string time(time_buff);
        log["time"] = time;
        ofstream_secret<<log.dump()<<std::endl;
    } else if(type=="request_secret" && j["dstname"]==username){
        char buff[4096];
        std::string name = j["srcname"];
        app_state = input_state::WAIT_SECRET_YES_NO;
        printf("[I][%s][%s] requests to chat with you, y/n:",time_buff,name.c_str());
        pthread_mutex_lock(&yesno_mutex);
        pthread_cond_wait(&yesno_condvar,&yesno_mutex);
        pthread_mutex_unlock(&yesno_mutex);
        if(yesno==true){
            send_secret_accept(name);
            std::cin.getline(buff,sizeof(buff));
            std::string message(buff);
            while(1) {
                char buff[4096];
                std::cin.getline(buff,sizeof(buff));
                std::string message(buff);
                if(message=="/e") {
                    break;
                } else {
                    on_secret_message(j["srcname"],message);
                }
            }
        } else {
            send_secret_decline(name);
        }
    } else if(type=="accept_secret") {
        while(1) {
            char buff[4096];
            std::cin.getline(buff,sizeof(buff));
            std::string message(buff);
            if(message=="/e") {
                break;
            } else {
                on_secret_message(j["srcname"],message);
            }
        }
        
    } else if (type=="decline_secret"){
        std::string srcname = j["srcname"];
        printf("[I][%s][%s] declines to chat with you\n",time_buff,srcname.c_str());
    }
}
