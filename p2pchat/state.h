//
//  state.h
//  p2pchat
//
//  Created by ZLY on 15/1/26.
//  Copyright (c) 2015年 ZLY. All rights reserved.
//

#ifndef p2pchat_state_h
#define p2pchat_state_h
#include <stdio.h>
#include <pthread.h>
#include <string>
#include "state.h"
#include <map>

extern std::map<std::string,std::string> live_user_list;
extern std::string username;
typedef enum input_state_{
    WAIT_BRAODCAST_INPUT,
    WAIT_SECRET_YES_NO,
    WAIT_SECRET_MESSAGE
}input_state;
extern bool yesno;
extern pthread_mutex_t yesno_mutex;
extern pthread_cond_t yesno_condvar;
extern input_state app_state;
extern pthread_mutex_t input_state_mutex;
extern pthread_cond_t input_state_convar;

extern pthread_mutex_t stdin_mutex;
extern pthread_cond_t stdin_condvar;

#endif
