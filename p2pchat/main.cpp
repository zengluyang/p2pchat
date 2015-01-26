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
#include "json.h"

#define HEARTBEAT_PORT 8000
#define MESSAGE_PORT 8001
#define BROADCAST_ADDR "255.255.255.255"

using namespace nlohmann;

std::map<std::string,std::string> live_user_list;
static std::string username;

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
            sprintf(ip_port, "%s:%u", inet_ntoa(clientAddr.sin_addr),ntohs(clientAddr.sin_port));
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
            char time_buff[20];
            time_t now = time(NULL);
            strftime(time_buff, 20, "%H:%M:%S", localtime(&now));
            json j = json::parse(buff);
            std::string type = j["type"];
            if(type=="broadcast"){
                std::string name = j["name"];
                std::string data = j["data"];
                printf("%s [%s]: %s\n",name.c_str(),time_buff,data.c_str());
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
        printf("SEND: %s\n",msg.c_str());
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
    // insert code here...
    std::cout << "Hello, World!\n";
    pthread_t send_heartbeat_thread, recv_heartbeat_thread;
    pthread_t send_message_broadcast_thread, recv_message_broadcast_thread;
    pthread_create(&recv_heartbeat_thread, NULL, recv_heartbeat,NULL);
    pthread_create(&send_message_broadcast_thread, NULL, send_message_broadcast,NULL);
    pthread_create(&recv_message_broadcast_thread, NULL, recv_message_broadcast,NULL);
    char name[4066];
    std::cout<<"input username:";
    std::cin.getline(name,sizeof(name));
    std::string name_str(name);
    on_online(name_str);
    pthread_create(&send_heartbeat_thread, NULL, send_heartbeat,NULL);
    while(1){
        char buff[4096];
        std::cin.getline(buff,sizeof(buff));
        std::string command(buff);
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
            break;
        } else if (command[0]!='/') {
            std::string message = command;
            on_broadcast(message);
        }
    }
    //pthread_exit(NULL);
    return 0;
}
