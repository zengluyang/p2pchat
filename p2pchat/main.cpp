//
//  main.cpp
//  p2pchat
//
//  Created by ZLY on 15/1/15.
//  Copyright (c) 2015年 ZLY. All rights reserved.
//

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <map>
#include <string>
#include <queue>
#include <vector>
#include <sstream>
#include <fstream>

#include "json.h"
#include "state.h"
#include "tcp.h"
#include "ultility.h"

#define HEARTBEAT_PORT 8000
#define MESSAGE_PORT 8001
#define BROADCAST_ADDR "255.255.255.255"

using namespace nlohmann;

static std::fstream ofstream_broadcast;
static std::fstream ifstream_broadcast;
static std::fstream ifstream_secret;
void init_db() {
    ofstream_broadcast.open("broadcast.db", std::ios_base::app);
    ifstream_broadcast.open("broadcast.db", std::ios_base::in);
    ifstream_secret.open("secret.db", std::ios_base::in);
}


void *recv_heartbeat(void* thread_id) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(HEARTBEAT_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    int sock;
    if ( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(1);
    }
    char buff[4096];
    struct sockaddr_in clientAddr;
    int n;
    ssize_t len = sizeof(clientAddr);
    while (1)
    {
        n = recvfrom(sock, buff, 4095, 0, (struct sockaddr*)&clientAddr, (socklen_t *)&len);
        if (n>0)
        {
            buff[n] = 0;
            char ip_port[512];
            sprintf(ip_port, "%s", inet_ntoa(clientAddr.sin_addr));
            //printf("RECV: %s %s\n", ip_port, buff);
            json jon = json::parse(buff);
            live_user_list[jon["name"]]=ip_port;
            n = sendto(sock, buff, n, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
            if (n < 0)
            {
                perror("sendto");
                break;
            }
        }
        else
        {
            perror("recv");
            break;
        }
    }
    return NULL;
}

static std::queue<std::string> send_message_queue;
static pthread_mutex_t send_message_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t send_message_condvar = PTHREAD_COND_INITIALIZER;
void *send_message_broadcast(void* thread_id) {
    //std::cout<<"send_message_broadcast started.."<<std::endl;
    while (1){
        pthread_mutex_lock(&send_message_queue_mutex);
        pthread_cond_wait(&send_message_condvar, &send_message_queue_mutex);
        if(!send_message_queue.empty()){
            //std::cout<<"send_message_broadcast reading from queue.."<<std::endl;
            
            const std::string packet = send_message_queue.front();
            //std::cout<<"from queue: "<<message<<std::endl;
            send_message_queue.pop();
            char buff[4096];
            ::strcpy(buff, packet.c_str());
            pthread_mutex_unlock(&send_message_queue_mutex);

            //send to network
            struct sockaddr_in addr;
            int sock;
            
            if ( (sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) <0)
            {
                perror("socket");
                exit(1);
            }
            int on=1;
            setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&on,sizeof(on));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(MESSAGE_PORT);
            addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
            ssize_t len = packet.length()+1;
            int n;
            n = sendto(sock, buff, len, 0, (struct sockaddr *)&addr, sizeof(addr));
            //printf("SEND: %s\n",buff);
            if (n < 0)
            {
                perror("sendto");
                close(sock);
                break;
            }
        }
    }
    return NULL;
}

void *recv_message_broadcast(void* thread_id) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(MESSAGE_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    int sock;
    if ( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(1);
    }
    char buff[4096];
    struct sockaddr_in clientAddr;
    int n;
    ssize_t len = sizeof(clientAddr);
    while (1)
    {
        n = recvfrom(sock, buff, 4095, 0, (struct sockaddr*)&clientAddr, (socklen_t *)&len);
        if (n>0)
        {
            buff[n] = 0;
            char time_buff[1024];
            time_t now = time(NULL);
            strftime(time_buff, 20, "%H:%M:%S", localtime(&now));
            json j = json::parse(buff);
            std::string type = j["type"];
            if(type=="broadcast"){
                std::string name = j["name"];
                std::string data = j["data"];
                printf("[B][%s][%s]: %s\n",time_buff,name.c_str(),data.c_str());
                strftime(time_buff, 1024, "%Y-%m-%d %H:%M:%S", localtime(&now));
                j["time"] = std::string(time_buff);
                std::string log(j.dump());
                ofstream_broadcast<<log<<std::endl;
            }
        }
        else
        {
            perror("recv msg");
            break;
        }
    }
    return NULL;

}


void *send_heartbeat(void * thread_id) {
    struct sockaddr_in addr;
    int sock;
    
    if ( (sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) <0)
    {
        perror("socket");
        exit(1);
    }
    int on=1;
    setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&on,sizeof(on));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(HEARTBEAT_PORT);
    addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
    json jon;
    jon["type"] = "heartbeat";
    jon["name"] = username;
    jon["address"] = "ip_addr";
    std::string msg = jon.dump();
    ssize_t len = sizeof(addr);
    while (1)
    {
        int n;
        n = sendto(sock, msg.c_str(), msg.length()+1, 0, (struct sockaddr *)&addr, sizeof(addr));
        //printf("SEND: %s\n",msg.c_str());
        if (n < 0)
        {
            perror("sendto");
            close(sock);
            break;
        }
        usleep(5000000);
    }
    return NULL;
}

void print_live_user_list() {
    for (auto& kv : live_user_list) {
        std::cout << kv.first << " " << kv.second << std::endl;
    }
}

void push_to_queue_and_signal(std::string &packet) {
    pthread_mutex_lock(&send_message_queue_mutex);
    send_message_queue.push(packet);
    pthread_mutex_unlock(&send_message_queue_mutex);
    pthread_cond_signal(&send_message_condvar);
}

void on_broadcast(std::string &message){
    json j;
    j["data"] = message;
    j["name"] = username;
    j["type"] = "broadcast";
    std::string packet = j.dump();
    push_to_queue_and_signal(packet);
}

void on_online(std::string &name){
    username = name;
    json j;
    j["type"] = "online";
    j["name"] = username;
    
}


int main(int argc, const char * argv[]) {
    char name[4066];
    std::cout<<"input username:";
    std::cin.getline(name,sizeof(name));
    std::string name_str(name);
    pthread_t send_heartbeat_thread, recv_heartbeat_thread;
    pthread_t send_message_broadcast_thread, recv_message_broadcast_thread;
    init_tcp_server();
    pthread_create(&recv_heartbeat_thread, NULL, recv_heartbeat,NULL);
    pthread_create(&send_message_broadcast_thread, NULL, send_message_broadcast,NULL);
    pthread_create(&recv_message_broadcast_thread, NULL, recv_message_broadcast,NULL);
    on_online(name_str);
    pthread_create(&send_heartbeat_thread, NULL, send_heartbeat,NULL);
    init_db();
    while(1){
        char buff[4096];
        std::string command;
        if(app_state==input_state::WAIT_BRAODCAST_INPUT) {
                printf("get stdin!\n");
                //switch (app_state) {
                //    case input_state::WAIT_BRAODCAST_INPUT:
                std::cin.getline(buff,sizeof(buff));
                command = std::string(buff);
                if (command.substr(0,2)=="/l"){
                    print_live_user_list();
                } else if(command.substr(0,2)=="/b"){
                    std::string message = command.substr(2);
                    on_broadcast(message);
                } else if (command.substr(0,2)=="/q") {
                    pthread_cancel(send_heartbeat_thread);
                    pthread_cancel(recv_heartbeat_thread);
                    pthread_cancel(recv_message_broadcast_thread);
                    pthread_cancel(send_message_broadcast_thread);
                    destroy_tcp_server();
                    break;
                } else if (command[0]!='/') {
                    std::string message = command;
                    on_broadcast(message);
                } else if (command.substr(0,3)=="/s "){
                    std::string name_msg = command.substr(3);
                    std::vector<std::string> vs = split(name_msg,' ');
                    std::string name=vs[0];
                    std::cout<<name;
                    std::string msg;
                    for(int i=1;i<vs.size();i++) {
                        msg+=vs[i];
                    }
                    on_secret_message(name, msg);
                } else if (command.substr(0,2)=="/h"){
                    char buff[4096];
                    while(ifstream_broadcast.getline(buff, sizeof(buff)))
                    {
                        std::string log(buff);
                        json j = json::parse(buff);
                        std::string time = j["time"];
                        std::string name = j["name"];
                        std::string data = j["data"];
                        printf("[B][%s][%s]: %s\n",time.c_str(),name.c_str(),data.c_str());
                    }
                    ifstream_broadcast.clear();
                    ifstream_broadcast.seekg(0,std::ios::beg);
                } else if(command.substr(0,4)=="/sh "){
                    std::string name = command.substr(4);
                    char buff[4096];
                    while(ifstream_secret.getline(buff, sizeof(buff)))
                    {
                        std::string log(buff);
                        json j = json::parse(buff);
                        std::string time = j["time"];
                        std::string srcname = j["srcname"];
                        std::string dstname = j["dstname"];
                        std::string data = j["data"];
                        if(srcname==name || dstname==name)
                            printf("[M][%s][%s]: %s\n",time.c_str(),srcname.c_str(),data.c_str());
                    }
                    ifstream_secret.clear();
                    ifstream_secret.seekg(0,std::ios::beg);


                }
        } else {
            std::cin.getline(buff,sizeof(buff));
            command = std::string(buff);
            if(command=="y") {
                pthread_mutex_lock(&yesno_mutex);
                yesno = true;
                pthread_cond_signal(&yesno_condvar);
                pthread_mutex_unlock(&yesno_mutex);
            }
        }
    }
    //pthread_exit(NULL);
    return 0;
}
