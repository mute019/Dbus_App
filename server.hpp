#include <iostream>
#include <dbus/dbus.h>
#include <signal.h>
#include <pthread.h>

#ifndef SERVER_H
#define SERVER_H

#define PIPE_SIZE 2 

typedef struct comm_info {

    const char *message_interface_name = "org.new.Methods";
    const char *diagonistic_interface_name = "org.app.diagonstics";
    const char *server_bus_name = "com.test.app_bus";
    const char *object_path_name = "/org/new/pack";
    const char *message_method = "execute";
    const char *diagonstic_method = "ping";

    const char *client_bus_name = "com.fix.test";
    
    bool conn_flag = false;
    bool resolved = false;
    DBusConnection *conn = nullptr;
    DBusMessage *message = nullptr;
    DBusError err;

    int app_pipe[PIPE_SIZE];
    pthread_mutex_t lock;

} server_info;

volatile sig_atomic_t break_flag;

// void clean_up(int , void* );
void message_handler(server_info *, DBusMessage* );
void message_thread_handler(server_info* servr);
void ping_thread_handler(server_info* servr); 

#endif