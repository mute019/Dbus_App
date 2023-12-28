#include <iostream>
#include <dbus/dbus.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>

#include "server.hpp"



int main() {
    
    server_info *servr = new server_info;
    int ret {};

    dbus_error_init(&servr->err);

    if (dbus_error_is_set(&servr->err)) {
        fprintf(stderr, "Error: %s \n", servr->err.message);
        dbus_error_free(&servr->err);
    }

    servr->conn = dbus_bus_get(DBUS_BUS_SESSION, &servr->err);

    if (servr->conn == NULL) {
        EXIT_FAILURE;
    }


    ret = dbus_bus_request_name(servr->conn, 
        "com.test.app_bus",
        DBUS_NAME_FLAG_REPLACE_EXISTING,
        &servr->err);



    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        exit(1);
    }

    const char *unique_name = dbus_bus_get_unique_name(servr->conn);

    fprintf(stdout, "Unique Name: %s\n", unique_name);

    while(!break_flag) {

        if (!dbus_connection_get_is_connected(servr->conn)) {
            fprintf(stderr, "No connection. \n");
        }
        
        if (!dbus_connection_read_write(servr->conn, 0)) {
            fprintf(stderr, "Waiting ..\n");
            continue;
        }

        pthread_mutex_init(&servr->lock, nullptr);

        if (!servr->resolved) {

            servr->message = dbus_connection_pop_message(servr->conn);
        }

        if (servr->message != nullptr) {
            const char* interface = dbus_message_get_interface(servr->message);

            fprintf(stderr, "%s \n", interface);
            
            if (strcmp(interface, servr->message_interface_name) == 0) {
                pthread_t message_thread;
                
                int message_thrd = pthread_create(&message_thread, NULL, (void* (*)(void *))message_thread_handler, servr);
                
                if (message_thrd == -1) {
                    fprintf(stderr, "pthread_create failed \n");
                }

                pthread_join(message_thread, nullptr);
                dbus_message_unref(servr->message); 
                servr->message = nullptr;
                continue;   

            }

            if (strcmp(interface, servr->diagonistic_interface_name) == 0) {
                
                pthread_t ping_thread;
                int ping_thrd = pthread_create(&ping_thread, NULL, (void* (*)(void *))ping_thread_handler, servr);
                
                if (ping_thrd == -1) {
                    fprintf(stderr, "pthread_create failed \n");
                }

                pthread_join(ping_thread, nullptr);  
                dbus_message_unref(servr->message);
                servr->message = nullptr;
                continue;
            }

            if (!servr->resolved && servr->message != nullptr) {

                dbus_message_unref(servr->message);
                servr->message = nullptr;

            }

            if (!servr->resolved && servr->message != nullptr) {

                dbus_message_unref(servr->message);
                servr->message = nullptr;
            }


        } else {
            sleep(1);
            continue;
        }


    }

    dbus_connection_unref(servr->conn);

    return 0;
}


void message_thread_handler(server_info* servr) {
        
        

        if (dbus_message_is_method_call(servr->message, servr->message_interface_name, servr->message_method)) {
            int child = fork();

            if (child == -1) {
                fprintf(stderr, "Error: Failed to create child process\n");
                return;

            } else if (child == 0) {
                message_handler(servr, servr->message);

            } else {
                int wpid {};
                while (wpid = wait(nullptr) > 0) {}
                return;
            }
        }

}


void message_handler(server_info *servr, DBusMessage* message) {

    pthread_mutex_lock(&servr->lock);
    DBusMessageIter args;

    char* message_str;

    if (!dbus_message_iter_init(message, &args)) {
        printf("error in dbus_message_iter_ini\n");
    } 

    while (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INVALID) {
        dbus_message_iter_get_basic(&args, &message_str);
        dbus_message_iter_next(&args);
    }

    const char *sender = dbus_message_get_sender(servr->message);

    // fprintf(stdout, "%s: %s\n", sender, message_str);

    if (pipe(servr->app_pipe) == -1) {
        fprintf(stderr, "Error: pipe() failed\n");
        return;
    } 

   

    char command_copy[100];

    int len = strlen(message_str);

    if (message_str[len - 1] == '\n') {
        message_str[len - 1] = '\0';
    } 

    int child = fork(); 
    
    if (child == -1) {
        fprintf(stderr, "Error: fork failed\n");
        return;
    }

    else if (child == 0) {

        strcpy(command_copy, message_str);
        // for storing the command name and it's params
        char **args = nullptr; 

        // tokenizing the string 
        // suppose we have "ls -la"


        // now it will split it to "ls, -la"
        char *token = strtok(message_str, " ");

        // we are also tracking the size
        int size = 0;
        while (token) {
            ++size;
            token = strtok(nullptr, " ");
            // fprintf(stdout, "%s ,", token);
        }
        
        
        args = new char*[size + 1]; // extra space for storing the null ptr

        //0,1,2

        token = strtok(command_copy, " "); //ls -la

        for (int iter = 0; iter < size; iter++) { 
            args[iter] = new char(strlen(token)); //args[1] = new char(strlen("-la"))
            args[iter] = token; //args[1] = "-la"
            token = strtok(nullptr, " "); 
        }

        // at the end the split array must contain a nullptr/NULL at the end such as "{ ls, -la, nullptr }"
        args[size] = nullptr; 

        // args: to point char pointers in turn points to the char arrays

        close(servr->app_pipe[0]);
        // close(servr->app_pipe[1]);

        dup2(servr->app_pipe[1], STDOUT_FILENO);

        int ret = execvp(args[0], args); //execvp("ls", NULL) 

        // // fprintf(stdout, "Hello\n");


        if (ret == -1) {
            std::cout << "execvp error: unable to run the command! \n";
        }

        // we are again resetting the "args" variable for the new tokens
        
        delete(args);
        args = nullptr;

        return;

    } else {
        
        int wpid {};
        
        close(servr->app_pipe[1]);  
        std::string prev = "";
        char *reply_str;
        size_t init_size = 0;
        
        FILE* reply_fd = fdopen(servr->app_pipe[0], "r");
        
        while ((init_size = getline(&reply_str, &init_size, reply_fd)) != -1) {
            prev = prev + reply_str;
        }

        const char* str_buffer = prev.c_str();

        std::cout << str_buffer;

        while (wpid = wait(nullptr) > 0) {}


        // // DBUS REPLY PART
        DBusMessage *reply;
        if ((reply = dbus_message_new_method_return(servr->message)) == nullptr) {
            fprintf(stderr, "Error in dbus_message_new_method_return");
            return;
        }

        DBusMessageIter iter;
      

        dbus_message_iter_init_append(reply, &iter);


        if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &reply_str)) {
            fprintf(stderr, "error in dbus_message_iter_append_basic \n");
            return;
        }

        if (!dbus_connection_send(servr->conn, reply, nullptr)) {
            fprintf(stderr, "error in dbus_connection_send \n");
            return;
        }

        dbus_message_unref(reply);

        servr->message = nullptr;
        servr->resolved = true;
        dbus_connection_flush(servr->conn);

        pthread_mutex_unlock(&servr->lock);

    }

}


void ping_thread_handler(server_info* servr) {
    if (dbus_message_is_method_call(servr->message, servr->diagonistic_interface_name, servr->diagonstic_method)) {
        DBusMessage *reply = dbus_message_new_method_return(servr->message);
        DBusMessageIter args;
        bool status = true;

        dbus_message_iter_init_append(reply, &args);

        if (dbus_message_iter_append_basic(&args, DBUS_TYPE_BOOLEAN, &status)) {
            fprintf(stderr, "error in dbus_message_iter_append_basic \n");
            return;
        }

        if (!dbus_connection_send(servr->conn, servr->message, nullptr)) {
            fprintf(stderr, "error in dbus_connection_send \n");
            return;
        }
    }
}
