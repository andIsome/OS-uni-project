#include "logging.h"
#include "protocol.h"
#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <memory.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "debug.h"

void print_usage(){
    fprintf(stderr, "usage: sub <register_pipe_name> <pipe_name> <box_name>\n");
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
    size_t messages_received = 0;

    if(argc != 4 || 
        strnlen(argv[2], MAX_PIPE_NAME_LEN)==MAX_PIPE_NAME_LEN ||
        strnlen(argv[3], MAX_BOX_NAME_LEN) ==MAX_BOX_NAME_LEN){

        print_usage();
        return -1;
    }

    // Relabel the individual argv
    const char* register_pipe_name = argv[1];
    const char* pipe_name = argv[2];
    const char* box_name = argv[3];

    ALWAYS_ASSERT(signal(SIGINT, sig_int_handler)!=SIG_ERR, "FAILED TO REGISTER SIG HANDLER");

    { // Register self at broker
        register_subscriber_packet packet;
        write_packet_register_sub(&packet, pipe_name, box_name);

        OPEN_FILE_FD(register_fifo, register_pipe_name, O_WRONLY);

        ssize_t wrote = write(register_fifo, &packet, sizeof(packet));

        if(wrote < sizeof(packet)){
            PANIC("FAILED TO REGISTER AT BROKER");
        }
    }

    ALWAYS_ASSERT(mkfifo(pipe_name, 0666)==0, "FAILED TO CREATE OWN FIFO! REASON: %i", errno);
    int msg_channel_fifo AUTO_CLOSE_FD = open(pipe_name, O_RDONLY);

    // Remove pipe from fs (not delete)
    unlink(pipe_name); //  (return value is ignored)

    if(msg_channel_fifo == -1){
        if(errno == EINTR){
            fprintf(stdout, "Received 0 messages!\n");
            exit(0);
        }
        PANIC("FAILED TO OPEN OWN FIFO!\n");
    }

    print_debug("CONNECTED!\n");

    // Setup for receiving messages
    message_packet packet;
    ssize_t read_from_fifo;

    while(1){
        // Read packet
        read_from_fifo = read(msg_channel_fifo, &packet, sizeof(packet));

        if(read_from_fifo!=sizeof(packet)){
            if(errno == 0){
                print_debug("SERVER DISCONNECTED!\n");
                break;
            }else if(errno == EINTR){
                // CTRL+C occoured
                break;
            }
            PANIC("UNKNOWN ERROR!");
        }

        if(packet.code!=(u8)ID_SEND_MSG_SUBSCRIBER){
            PANIC("GOT INVALID PACKET ID!");
        }

        messages_received++;
        fprintf(stdout, "%s\n", packet.message);
    }

    fprintf(stdout, "Received %zu messages!\n", messages_received);

    return 0;
}