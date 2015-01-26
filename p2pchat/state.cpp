//
//  state.cpp
//  p2pchat
//
//  Created by ZLY on 15/1/26.
//  Copyright (c) 2015年 ZLY. All rights reserved.
//

#include <stdio.h>
#include <pthread.h>
#include <string>
#include "state.h"
#include <map>

std::map<std::string,std::string> live_user_list;
std::string username;

input_state app_state = input_state::WAIT_BRAODCAST_INPUT;
pthread_mutex_t input_state_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t input_state_convar = PTHREAD_COND_INITIALIZER;

bool yesno = false;
pthread_mutex_t yesno_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t yesno_condvar= PTHREAD_COND_INITIALIZER;

pthread_mutex_t stdin_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t stdin_condvar = PTHREAD_COND_INITIALIZER;
