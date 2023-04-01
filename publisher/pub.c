#include "logging.h"
#include "protocol.h"
#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <signal.h>
#include <poll.h>

void print_usage(){
    fprintf(stderr, "usage:pub <register_pipe_name> <pipe_name> <box_name>\n");
}

// The sig handler has to be registered
// so the read exists with errno EINTR (ERROR Interupt)
void sig_int_handler(int sig){
    (void) sig;
    return;
}

//#define DEBUG_MSG

void print_debug(const char* what){
#ifdef DEBUG_MSG
    fprintf(stdout, "%s", what);
#else
    (void) what;
#endif
}

int main(int argc, char **argv) {
    if(argc != 4 || 
        strnlen(argv[2], MAX_PIPE_NAME_LEN)==MAX_PIPE_NAME_LEN ||
        strnlen(argv[3], MAX_BOX_NAME_LEN) ==MAX_BOX_NAME_LEN){

        print_usage();
        return -1;
    }

    ALWAYS_ASSERT(signal(SIGINT, sig_int_handler)!=SIG_ERR, "FAILED TO REGISTER SIG HANDLER");

    const char* register_pipe_name = argv[1];
    const char* pipe_name = argv[2];
    const char* box_name = argv[3];
    
    {
        register_publisher_packet packet;
        write_packet_register_pub(&packet, pipe_name, box_name);

        OPEN_FILE_FD(register_fifo, register_pipe_name, O_WRONLY);

        ssize_t wrote = write(register_fifo, &packet, sizeof(packet));

        if(wrote!=sizeof(packet)){
            PANIC("FAILED TO REGISTER AT BROKER");
        }
        
    }

    ALWAYS_ASSERT(mkfifo(pipe_name, 0666)==0, "FAILED TO CREATE OWN FIFO! REASON: %i", errno);
    int msg_channel_fifo AUTO_CLOSE_FD = open(pipe_name, O_WRONLY);

    // Remove pipe from fs (not delete)
    unlink(pipe_name); //  (return value is ignored)

    if(msg_channel_fifo == -1){
        if(errno == EINTR){
            exit(0);
        }
        PANIC("FAILED TO OPEN OWN FIFO!\n");
    }

    print_debug("CONNECTED!\n");

    message_packet packet;

    packet.code = (u8)ID_SEND_MSG_SERVER;

    while(1){
        char* ret_val = fgets(packet.message, 1024, stdin);

        // Exit on CTRL+D
        if(feof(stdin)){
            print_debug("Hit eof!\n");
            break;
        }
        if(errno == EINTR){
            print_debug("DISCONNECTED!\n");
            break;
        }
        if(ret_val==NULL) PANIC("UNKOWN STDIN ERROR!\n");

        ssize_t wrote = write(msg_channel_fifo, &packet, sizeof(packet));
        
        if(errno == EINTR){
            print_debug("DISCONNECTED!\n");
            break;
        }

        if(wrote != sizeof(packet)){
            fprintf(stderr, "Failed to write to pipe! (%i)\n", (i32)wrote);
            break;
        }
    }
    
    return 0;
}
