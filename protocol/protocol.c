#include "protocol.h"
#include <string.h>

static const size_t packet_size[] = {
    sizeof(register_publisher_packet),
    sizeof(register_subscriber_packet),
    sizeof(create_msg_box_packet),
    sizeof(response_create_msg_box_packet),
    sizeof(remove_msg_box_packet),
    sizeof(response_remove_msg_box_packet),
    sizeof(list_msg_box_packet),
    sizeof(list_msg_box_response_packet),
    sizeof(message_packet),
    sizeof(message_packet)
};

ssize_t id_size_lookup(enum PacketId id){
    if(!(ID_REGISTER_PUBLISHER<=id && id<=ID_SEND_MSG_SUBSCRIBER)){
        return -1;
    }
    return (ssize_t)packet_size[id-1];
}


void write_packet_create(create_msg_box_packet* packet, const char* client_named_pipe, const char* msg_box){
    memset(packet, 0, sizeof(create_msg_box_packet));

    packet->code = (u8)ID_CREATE_MSG_BOX;

    strcpy(packet->client_named_pipe, client_named_pipe);
    strcpy(packet->box_name,          msg_box          );
}

void write_packet_remove(create_msg_box_packet* packet, const char* client_named_pipe, const char* msg_box){
    memset(packet, 0, sizeof(create_msg_box_packet));

    packet->code = (u8)ID_REMOVE_MSG_BOX;

    strcpy(packet->client_named_pipe, client_named_pipe);
    strcpy(packet->box_name,          msg_box          );
}

void write_packet_register_sub(register_subscriber_packet* packet, const char* client_named_pipe, const char* box_name){
    memset(packet, 0, sizeof(register_subscriber_packet));

    packet->code = (u8)ID_REGISTER_SUBSCRIBER;

    strcpy(packet->client_named_pipe, client_named_pipe);
    strcpy(packet->box_name,          box_name         );
}

void write_packet_register_pub(register_publisher_packet* packet, const char* client_named_pipe, const char* box_name){
    memset(packet, 0, sizeof(register_publisher_packet));

    packet->code = (u8)ID_REGISTER_PUBLISHER;

    strcpy(packet->client_named_pipe, client_named_pipe);
    strcpy(packet->box_name,          box_name         );
}