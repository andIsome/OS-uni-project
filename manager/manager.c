#include "logging.h"
#include "protocol.h"
#include "common.h"
#include "vector.h"

#include <string.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "debug.h"
#include <signal.h>

static void print_usage();

char* commands_str[] = {
    "create",
    "remove",
    "list"
};

enum command {
    cmd_create,
    cmd_remove,
    cmd_list
};

//#define DEBUG_MSG

void print_debug(const char* what){
#ifdef DEBUG_MSG
    fprintf(stdout, "%s", what);
#else
    (void) what;
#endif
}

int msg_cmp_func(const void* m1, const void* m2){
    list_msg_box_response_packet* msg1 = *(list_msg_box_response_packet**)m1;
    list_msg_box_response_packet* msg2 = *(list_msg_box_response_packet**)m2;
    return strcmp(msg1->box_name, msg2->box_name);
}

// The sig handler has to be registered
// so the read exists with errno EINTR (ERROR Interupt)
void sig_int_handler(int sig){
    (void) sig;
    return;
}

void execute_command_create(int* register_fifo, const char* pipe_name, const char* msg_box);
void execute_command_remove(int* register_fifo, const char* pipe_name, const char* msg_box);
void execute_command_list  (int* register_fifo, const char* pipe_name);

int main(int argc, char **argv) {
    if(!(argc==4 || argc==5)){
        print_usage();
        return -1;
    }

    const char* register_pipe_name = argv[1];
    const char* pipe_name = argv[2];
    const char* cmd = argv[3];
    int command = -1;

    // Check pipe_name doesn't exceed the predetermined size
    if(strnlen(pipe_name, MAX_PIPE_NAME_LEN)==MAX_PIPE_NAME_LEN){
        print_usage();
        return -1;
    }

    // Get enum value of string command
    for(int i=0;i<sizeof(commands_str)/sizeof(char*);i++){
        if(strcmp(commands_str[i], cmd)==0){
            command = i;
            break;
        }
    }

    // Verify that the correct number of argc is present for each command
    if(command == -1 || ((command==cmd_create || command==cmd_remove) && argc!=5) || (command == cmd_list && argc!=4)){
        print_usage();
        return -1;
    }

    ALWAYS_ASSERT(signal(SIGINT, sig_int_handler)!=SIG_ERR, "FAILED TO REGISTER SIG HANDLER");

    OPEN_FILE_FD(register_pipe, register_pipe_name, O_WRONLY);

    ALWAYS_ASSERT(mkfifo(pipe_name, 0666)==0, "FAILED TO CREATE FIFO!");


    switch (command){
    case cmd_create:
        execute_command_create(&register_pipe, pipe_name, argv[4]);
        break;
    case cmd_remove:
        execute_command_remove(&register_pipe, pipe_name, argv[4]);
        break;
    case cmd_list:
        execute_command_list(&register_pipe, pipe_name);
        break;
    default:
        fprintf(stdout, "Unkown command: %s\n", cmd);
        print_usage();
        break;
    }
    
    return 0;
}



void print_usage() {
    fprintf(stderr, "usage: \n"
                    "   manager <register_pipe_name> <pipe_name> create <box_name>\n"
                    "   manager <register_pipe_name> <pipe_name> remove <box_name>\n"
                    "   manager <register_pipe_name> <pipe_name> list\n");
}


void execute_command_create(int* register_fifo, const char* pipe_name, const char* msg_box){
    fprintf(stdout, "run create box command!\n");
    create_msg_box_packet packet;
    write_packet_create(&packet, pipe_name, msg_box);

    if(write(*register_fifo, &packet, sizeof(packet))!=sizeof(packet)){
        PANIC("ERROR WRITING PACKET TO BROKER!");
    }

    int own_fifo AUTO_CLOSE_FD = open(pipe_name, O_RDONLY);

    unlink(pipe_name);

    if(own_fifo == -1){
        if(errno == EINTR){
            exit(0);
        }
        PANIC("FAILED TO OPEN OWN FIFO!\n");
    }
    
    response_create_msg_box_packet response_packet;
    
    ssize_t fifo_read = read(own_fifo, &response_packet, sizeof(response_packet));

    if(fifo_read == 0){
        print_debug("SERVER DISCONNECTED!\n");
        return;
    }else if(fifo_read==-1 || fifo_read!=sizeof(response_packet)){
        if(errno == EINTR){
            print_debug("DISCONNECTED!\n");
            exit(0);
        }
        PANIC("UNKNOWN ERROR OCURRED!\n");
        return;
    }

    if(response_packet.error_code==0){
        fprintf(stdout, "OK\n");
    }else{
        fprintf(stdout, "ERROR %s\n", response_packet.error_message);
    }
}

void execute_command_remove(int* register_fifo, const char* pipe_name, const char* msg_box){
    remove_msg_box_packet packet;
    write_packet_remove(&packet, pipe_name, msg_box);

    if(write(*register_fifo, &packet, sizeof(packet))!=sizeof(packet)){
        PANIC("ERROR WRITING PACKET TO BROKER!");
    }

    int own_fifo AUTO_CLOSE_FD = open(pipe_name, O_RDONLY);
    
    unlink(pipe_name);

    if(own_fifo == -1){
        if(errno == EINTR){
            exit(0);
        }
        PANIC("FAILED TO OPEN OWN FIFO!\n");
    }
    
    response_create_msg_box_packet response_packet;
    
    ssize_t fifo_read = read(own_fifo, &response_packet, sizeof(response_packet));

    if(fifo_read == 0){
        print_debug("SERVER DISCONNECTED!\n");
        return;
    }else if(fifo_read==-1 || fifo_read!=sizeof(response_packet)){
        if(errno == EINTR){
            print_debug("DISCONNECTED!\n");
            exit(0);
        }
        PANIC("UNKNOWN ERROR OCURRED!\n");
        return;
    }

    if(response_packet.error_code==0){
        fprintf(stdout, "OK\n");
    }else{
        fprintf(stdout, "ERROR %s\n", response_packet.error_message);
    }
}

void execute_command_list(int* register_fifo, const char* pipe_name){
    list_msg_box_packet packet;
    memset(&packet, 0, sizeof(packet));

    packet.code = (u8)ID_LIST_MSG_BOX;

    strcpy(packet.client_named_pipe, pipe_name);


    if(write(*register_fifo, &packet, sizeof(packet))!=sizeof(packet)){
        PANIC("ERROR WRITING PACKET TO BROKER!");
    }

    int own_fifo AUTO_CLOSE_FD = open(pipe_name, O_RDONLY);
    
    unlink(pipe_name);

    if(own_fifo == -1){
        if(errno == EINTR){
            exit(0);
        }
        PANIC("FAILED TO OPEN OWN FIFO!\n");
    }

    list_msg_box_response_packet* response_packet = NULL;
    
    vector v __attribute__((cleanup(vector_destory)));
    vector_create(&v, 5);

    do{
        response_packet = malloc(sizeof(list_msg_box_response_packet));
        ALWAYS_ASSERT(response_packet!=NULL, "NO MEMORY!");
        
        vector_push(&v, response_packet);

        ssize_t fifo_read = read(own_fifo, response_packet, sizeof(list_msg_box_response_packet));

        if(fifo_read == 0){
            print_debug("SERVER DISCONNECTED!\n");
            return;
        }else if(fifo_read==-1 || fifo_read!=sizeof(list_msg_box_response_packet)){
            if(errno == EINTR){
                print_debug("DISCONNECTED!\n");
                exit(0);
            }
            PANIC("UNKNOWN ERROR OCURRED!\n");
            return;
        }

        if(response_packet->box_name[0]=='\0' && response_packet->is_last){
            fprintf(stdout, "NO BOXES FOUND\n");
            return;
        }
        
    }while(!response_packet->is_last);

    vector_sort(&v, msg_cmp_func);

    for(int i=0;i<v.size;i++){
        list_msg_box_response_packet* curr = (list_msg_box_response_packet*)v.buff[i];
        fprintf(stdout, "%s %zu %zu %zu\n",
            curr->box_name,
            curr->box_size,
            curr->n_publishers,
            curr->n_subscribers);
    }
}