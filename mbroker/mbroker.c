#include "common.h"
#include "producer-consumer.h"
#include "protocol.h"

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include "operations.h"

typedef struct{
    u8 id;
    void* packet_data;
} unknown_packet;

pthread_mutex_t messages_lock = PTHREAD_MUTEX_INITIALIZER;

// Built in simple linked list
typedef struct message_box{
    char name[MAX_BOX_NAME_LEN];
    u64 publishers, subscribers, size;
    int fd_internal_w;
    struct message_box* next;
    pthread_cond_t write_wait;
    pthread_mutex_t wait_mutex;
} message_box;

void add_msg_box(const char* name);
void remove_msg_box(const char* name);
message_box* get_msg_box(const char* name);


message_box* msg_boxes = NULL;
char* pipe_name = NULL;


void print_usage();
void process_packet(unknown_packet packet);
void* worker_main(void* queue_void);


void sig_pipe_handler(int sig){
    if(sig==SIGINT){
        if(pipe_name!=NULL)
            unlink(pipe_name);
        pipe_name=NULL;
    }
    exit(0);
}

int main(int argc, char **argv) {
    int num_sessions=0;

    if(argc != 3 || sscanf(argv[2], "%i", &num_sessions)!=1 || strlen(argv[1])==0){
        print_usage();
        return -1;
    }

    ALWAYS_ASSERT(signal(SIGINT, sig_pipe_handler)!=SIG_ERR, "FAILED TO REGISTER SIGNAL HANDLER!");

    pipe_name = argv[1];
    pc_queue_t workqueue __attribute__((cleanup(pcq_dequeue)));

    ALWAYS_ASSERT(pcq_create(&workqueue, (size_t)num_sessions)==0, "Failed to create pcq_queue!");

    if(mkfifo(pipe_name, 0666)!=0) { PANIC("FAILED TO CREATE FIFO! %i", errno); }

    OPEN_FILE_FD(fifo,          pipe_name, O_RDONLY);
    // So the rdonly can always read
    OPEN_FILE_FD(fifo_internal, pipe_name, O_WRONLY);

    pthread_t* worker_threads = (pthread_t*)malloc(sizeof(pthread_t)*(size_t)num_sessions);
    ALWAYS_ASSERT(worker_threads!=NULL, "NO MEMORY!");

    for(int i=0;i<num_sessions;i++){
        ALWAYS_ASSERT(
            pthread_create(worker_threads + i, NULL, worker_main, (void*)&workqueue)==0,
            "FAILED TO SPAWN THREAD!"
        );
    }

    while(1){
        u8 packet_id;

        // Read next packet id
        ssize_t fifo_read = read(fifo, &packet_id, sizeof(packet_id));
        if(fifo_read<=0) PANIC("READ ERROR!\n");

        // Get the packet size based on the id
        ssize_t packet_size = id_size_lookup(packet_id);
        if(packet_size==-1) PANIC("ILLEGAL PACKET ID: %i\n", (int)packet_id);

        // allocate packet
        void* data = malloc((size_t)packet_size);
        ALWAYS_ASSERT(data!=NULL, "NO MEMORY!");
        *((u8*)data) = packet_id;

        // Read packet body
        fifo_read = read(fifo, data+1,(size_t)packet_size-1);
        if(fifo_read!=(size_t)packet_size-1) PANIC("CORRUPTED PIPE!");

        pcq_enqueue(&workqueue, data);
    }

    MTX_DESTORY(messages_lock);

    return 0;
}

// main function for worker threads
void* worker_main(void* queue_void){
    pc_queue_t* queue = (pc_queue_t*) queue_void;

    // Setup signal handling
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    while(1){
        void* data = pcq_dequeue(queue);

        if(data == NULL) pthread_exit(NULL);
        
        unknown_packet packet;
        
        packet.packet_data = data;
        packet.id = *((u8*)packet.packet_data);

        process_packet(packet);

        free(data);
    }
    pthread_exit(NULL);
}

void handle_packet_register_pub(unknown_packet upacket);
void handle_packet_register_sub(unknown_packet upacket);
void handle_packet_create_msg_box(unknown_packet upacket);
void handle_packet_remove_msg_box(unknown_packet upacket);
void handle_packet_list_msg_box(unknown_packet upacket);

void process_packet(unknown_packet packet) {
    switch (packet.id) {
        case ID_REGISTER_PUBLISHER:
            handle_packet_register_pub(packet);
            return;
        case ID_REGISTER_SUBSCRIBER:
            handle_packet_register_sub(packet);
            return;
        case ID_CREATE_MSG_BOX:
            handle_packet_create_msg_box(packet);
            return;
        case ID_REMOVE_MSG_BOX:
            handle_packet_remove_msg_box(packet);
            return;
        case ID_LIST_MSG_BOX:
            handle_packet_list_msg_box(packet);
            return;
        default:
            PANIC("ILLEGAL PACKET ID (%i): FIFO CORRUPTED?\n", packet.id);
    }
}

void handle_packet_register_pub(unknown_packet upacket){
    register_publisher_packet* register_packet = upacket.packet_data;

    int connection AUTO_CLOSE_FD = open(register_packet->client_named_pipe, 00);
    if(connection==-1) return;

    message_box* msg;
    int fd = -1;
    {
        SCOPED_LOCK(messages_lock);
        msg = get_msg_box(register_packet->box_name);
        if(msg==NULL || msg->publishers>0) return;
        msg->publishers = 1;
        fd = msg->fd_internal_w;
    }

    message_packet msg_packet;

    while(1){
        ssize_t fifo_read = read(connection, &msg_packet, sizeof(message_packet));
        if(fifo_read==0){
            break;
        }
        if(fifo_read!=sizeof(message_packet) || msg_packet.code!=ID_SEND_MSG_SERVER){
            fprintf(stderr, "UNKNOWN ERROR! (%i)\nCLOSING CONNECTION!", errno);
            break;
        }
        
        if(write(fd, &msg_packet, sizeof(msg_packet))!=sizeof(msg_packet)) break;
        SCOPED_LOCK(msg->wait_mutex);
        msg->size += sizeof(msg_packet);
        pthread_cond_broadcast(&msg->write_wait);
    }
    SCOPED_LOCK(msg->wait_mutex);
    msg->publishers--;
}

void handle_packet_register_sub(unknown_packet upacket){
    register_subscriber_packet* register_packet = upacket.packet_data;
    ALWAYS_ASSERT(register_packet->code == ID_REGISTER_SUBSCRIBER, "FATAL ERROR");


    int communication AUTO_CLOSE_FD = open(register_packet->client_named_pipe, O_WRONLY);
    if(communication == -1) return;
    message_box* msg;
    int fd AUTO_CLOSE_FD = -1;
    {
        SCOPED_LOCK(messages_lock);
        msg = get_msg_box(register_packet->box_name);
        if(msg==NULL) return;
        fd = open(msg->name, O_RDONLY);
        if(fd==-1) return;
        msg->subscribers++;
        
    }

    message_packet packet; packet.code = ID_SEND_MSG_SUBSCRIBER;

    while(1){
        {
            SCOPED_LOCK(msg->wait_mutex);
            while(msg->size==lseek(fd, 0, SEEK_CUR)){
                pthread_cond_wait(&msg->write_wait, &msg->wait_mutex);
            }
        }
        ssize_t rread = read(fd, &packet, sizeof(packet));
        if(rread == 0) continue;

        if(rread!=sizeof(packet)) break;

        packet.code = ID_SEND_MSG_SUBSCRIBER;
        ssize_t wwrote = write(communication, &packet, sizeof(packet));
        if(wwrote!=sizeof(packet)){
            if(errno!=SIGPIPE && errno!=0){
                fprintf(stderr, "unknown error occured! sub disconnected!\n");
            }
            break;
        }

    }

    SCOPED_LOCK(msg->wait_mutex);
    msg->subscribers--;
}

void handle_packet_create_msg_box(unknown_packet upacket){
    create_msg_box_packet* packet = upacket.packet_data;
    
    int connection AUTO_CLOSE_FD = open(packet->client_named_pipe, O_WRONLY);
    if(connection==-1) return;

    message_box* box;
    {
        SCOPED_LOCK(messages_lock);
        box = get_msg_box(packet->box_name);
        if(box==NULL){
            add_msg_box(packet->box_name);
        }
    }
    response_create_msg_box_packet response_packet;
    response_packet.code = (u8)ID_RESPONSE_CREATE_MSG_BOX;
    memset(response_packet.error_message, 0, ERROR_MSG_LEN);

    if(box==NULL){
        response_packet.error_code = 0;
    }else{
        response_packet.error_code = -1;
        strcpy(response_packet.error_message, "YOU CANT ADD A MSGBOX THAT ALREADY EXIST!");
    }

    ssize_t _temp_ = write(connection, &response_packet, sizeof(response_packet));
    (void) _temp_;
}

void handle_packet_remove_msg_box(unknown_packet upacket){
    remove_msg_box_packet* packet = upacket.packet_data;

    int connection AUTO_CLOSE_FD = open(packet->client_named_pipe, O_WRONLY);
    if(connection==-1) return;

    message_box* box;
    u64 pub=0, sub=0;
    bool removed = false;
    {
        SCOPED_LOCK(messages_lock);
        box = get_msg_box(packet->box_name);
        if(box!=NULL && box->publishers==0 && box->subscribers==0){
            removed = true;
            sub = box->subscribers;
            pub = box->publishers;
            remove_msg_box(packet->box_name);
        }
    }

    response_remove_msg_box_packet response_packet;
    response_packet.code = ID_RESPONSE_REMOVE_MSG_BOX;
    memset(response_packet.error_message, 0, ERROR_MSG_LEN);

    if(removed){
        response_packet.error_code = 0;
    }else{
        response_packet.error_code = -1;
        if(box==NULL){
            strcpy(response_packet.error_message, "YOU CANT REMOVE A MSGBOX THAT DOESNT EXIST!");
        }else if(sub!=0 && pub!=0){
            strcpy(response_packet.error_message, "YOU CANT REMOVE A MSGBOX THAT HAS SUB AND PUB");
        }else if(sub!=0){
            strcpy(response_packet.error_message, "YOU CANT REMOVE A MSGBOX THAT HAS SUB");
        }else{
            strcpy(response_packet.error_message, "YOU CANT REMOVE A MSGBOX THAT HAS PUB");
        }
    }

    ssize_t _temp_ = write(connection, &response_packet, sizeof(response_packet));
    (void) _temp_;
}

void handle_packet_list_msg_box(unknown_packet upacket){
    list_msg_box_packet* packet = upacket.packet_data;

    int connection AUTO_CLOSE_FD = open(packet->client_named_pipe, O_WRONLY);
    if(connection==-1) return;

    list_msg_box_response_packet response_packet;
    response_packet.code = ID_RESPONSE_LIST_MSG_BOX;

    SCOPED_LOCK(messages_lock);
    if(msg_boxes==NULL){
        memset(response_packet.box_name, 0, MAX_BOX_NAME_LEN);
        response_packet.is_last = true;
        ssize_t _temp_ = write(connection, &response_packet, sizeof(response_packet));
        (void) _temp_;
        return;
    }

    for(message_box* it=msg_boxes;it!=NULL;it=it->next){
        strcpy(response_packet.box_name, it->name);
        response_packet.is_last = it->next==NULL;
        response_packet.box_size = it->size;
        response_packet.n_publishers = it->publishers;
        response_packet.n_subscribers = it->subscribers;
        ssize_t _temp_ = write(connection, &response_packet, sizeof(response_packet));
        (void) _temp_;
    }
}

void print_usage(){
    fprintf(stderr, "usage: mbroker <register_pipe_name> <max_sessions>\n");
}

void add_msg_box(const char* name) {
    message_box* new_box = (message_box*) malloc(sizeof(message_box));
    if(new_box == NULL) {
        PANIC("Failed to allocate memory for new message box.");
    }
    new_box->next = NULL;
    strcpy(new_box->name, name);
    new_box->publishers=0;
    new_box->subscribers=0;
    new_box->size=0;

    MTX_INIT(new_box->wait_mutex);
    COND_INIT(new_box->write_wait);

    new_box->fd_internal_w = open(new_box->name, O_WRONLY | O_CREAT, 0644);

    if(msg_boxes == NULL) {
        msg_boxes = new_box;
    } else {
        message_box* current = msg_boxes;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_box;
    }
}

void remove_msg_box(const char* name) {
    message_box* current = msg_boxes;
    message_box* previous = NULL;

    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            if (previous == NULL) {
                msg_boxes = current->next;
            } else {
                previous->next = current->next;
            }
            close(current->fd_internal_w);
            MTX_DESTORY(current->wait_mutex);
            COND_DESTROY(current->write_wait);
            unlink(current->name);
            free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

message_box* get_msg_box(const char* name) {
    message_box* current = msg_boxes;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}