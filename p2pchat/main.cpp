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

#define HEARTBEAT_PORT 8000
#define BROADCAST_ADDR "255.255.255.255"

std::map<std::string,std::string> live_user_list;

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
    char buff[512];
    struct sockaddr_in clientAddr;
    int n;
    ssize_t len = sizeof(clientAddr);
    while (1)
    {
        n = recvfrom(sock, buff, 511, 0, (struct sockaddr*)&clientAddr, (socklen_t *)&len);
        if (n>0)
        {
            buff[n] = 0;
            char ip_port[512];
            sprintf(ip_port, "%s:%u", inet_ntoa(clientAddr.sin_addr),ntohs(clientAddr.sin_port));
            //printf("RECV: %s %s\n", ip_port, buff);
            live_user_list[ip_port]="true";
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
    char buff[512] = "test";
    ssize_t len = sizeof(addr);
    while (1)
    {
        int n;
        n = sendto(sock, buff, strlen(buff), 0, (struct sockaddr *)&addr, sizeof(addr));
        //printf("SEND: %s\n",buff);
        if (n < 0)
        {
            perror("sendto");
            close(sock);
            break;
        }
                usleep(300000);
    }
    return NULL;
}

void print_live_user_list() {
    for (auto& kv : live_user_list) {
        std::cout << kv.first << " " << kv.second << std::endl;
    }
}

int main(int argc, const char * argv[]) {
    // insert code here...
    std::cout << "Hello, World!\n";
    pthread_t send_heartbeat_thread, recv_heartbeat_thread;
    pthread_create(&send_heartbeat_thread, NULL, send_heartbeat,NULL);
    pthread_create(&recv_heartbeat_thread, NULL, recv_heartbeat,NULL);
    while(1){
        std::string command;
        std::cin>>command;
        if (command=="/l"){
            print_live_user_list();
        } else if (command=="/q") {
            pthread_cancel(send_heartbeat_thread);
            pthread_cancel(recv_heartbeat_thread);
            break;
        }
    }
    //pthread_exit(NULL);
    return 0;
}
