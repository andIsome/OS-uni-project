#pragma once

#include "common.h"

enum PacketId {
    ID_REGISTER_PUBLISHER=1,
    ID_REGISTER_SUBSCRIBER,
    ID_CREATE_MSG_BOX,
    ID_RESPONSE_CREATE_MSG_BOX,
    ID_REMOVE_MSG_BOX,
    ID_RESPONSE_REMOVE_MSG_BOX,
    ID_LIST_MSG_BOX,
    ID_RESPONSE_LIST_MSG_BOX,
    ID_SEND_MSG_SERVER,
    ID_SEND_MSG_SUBSCRIBER
};

#define ERROR_MSG_LEN     1024
#define MSG_LEN           1024
#define MAX_PIPE_NAME_LEN  256
#define MAX_BOX_NAME_LEN    32

#pragma pack(push, 1)
typedef struct {
    u8 code;
    char client_named_pipe[MAX_PIPE_NAME_LEN];
    char box_name[MAX_BOX_NAME_LEN];
} register_publisher_packet;
#pragma pack(pop)

// Same packet layout
typedef register_publisher_packet register_subscriber_packet;
typedef register_publisher_packet create_msg_box_packet;

#pragma pack(push, 1)
typedef struct{
    u8 code;
    i32 error_code;
    char error_message[ERROR_MSG_LEN];
} response_create_msg_box_packet;
#pragma pack(pop)

// Same packet layout
typedef register_publisher_packet remove_msg_box_packet;
typedef response_create_msg_box_packet response_remove_msg_box_packet;

#pragma pack(push, 1)
typedef struct{
    u8 code;
    char client_named_pipe[MAX_PIPE_NAME_LEN];
} list_msg_box_packet;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct{
    u8 code, is_last;
    char box_name[32];
    u64 box_size, n_publishers, n_subscribers;
} list_msg_box_response_packet;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct{
    u8 code;
    char message[MSG_LEN];
} message_packet;
#pragma pack(pop)

ssize_t id_size_lookup(enum PacketId id);

void write_packet_create(create_msg_box_packet* packet, const char* client_named_pipe, const char* msg_box);

void write_packet_remove(create_msg_box_packet* packet, const char* client_named_pipe, const char* msg_box);

void write_packet_register_sub(register_subscriber_packet* packet, const char* client_named_pipe, const char* box_name);

void write_packet_register_pub(register_publisher_packet* packet, const char* client_named_pipe, const char* box_name);