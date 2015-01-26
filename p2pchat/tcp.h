//
//  tcp.h
//  p2pchat
//
//  Created by ZLY on 15/1/26.
//  Copyright (c) 2015年 ZLY. All rights reserved.
//

#ifndef __p2pchat__tcp__
#define __p2pchat__tcp__

#include <stdio.h>
#include <queue>
#include <utility>
#include <string>
#include <pthread.h>
#include "state.h"
#define TCP_LISTEN_PORT 8000
class SecretMessage{
public:
    std::string name;
    std::string message;
};
void* recv_message_secret(void* thread_id);
void* send_message_secret(void* thread_id);
void init_tcp_server();
void destroy_tcp_server();
void on_secret_message(std::string name,std::string message);
void on_recv(std::string &packet);
void push_to_queue_and_signal(SecretMessage &sm);
void send_secret_request(std::string name);
void send_secret_accept(std::string name);
void change_state(input_state state);

#endif /* defined(__p2pchat__tcp__) */
