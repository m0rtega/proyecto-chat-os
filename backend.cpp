/*
** Codigo de servidor. Parte del codigo esta basado en lo que leimos en la guia del maestro Beej:
** https://beej.us/guide/bgnet/html/
*/


#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <iostream>
#include "payload.pb.h"
#include <fstream>
#include <string>
#include <netinet/in.h>
#include <string.h>
#include <map>
#include <pthread.h>
#include <string.h>
#include <unordered_map>
#include <arpa/inet.h>

#define BUFFER_SIZE 8192
#define OK 200
#define SENDER "Server"
#define BAD_RESPONSE 500
#define BACKLOG 5

using namespace google::protobuf;
using namespace std;

// Estadisticas

long rxByteCount, txByteCount;

// Para cada send o recv

int bytesReceived, bytesSent;
int messagesReceived, messagesSent;

typedef struct
{
    
    int socket;
    string username;
    char ipAddr[INET_ADDRSTRLEN];
    string status;

} client_struct;
unordered_map<string, client_struct *> clients;

void print_server_stats(){

    cout << endl
              << "Server stats"
              << endl
              << "* Usuarios conectados: " << clients.size() << endl
              << "* Bytes Recibidos: " << rxByteCount << endl
              << "* Bytes Enviados: " << txByteCount << endl
              << "* Mensajes Recibidos: " << messagesReceived << endl
              << "* Mensajes Enviados: " << messagesSent << endl
              << "------------------------------------------------------"
              << endl
              << endl;

}

void exit_with_error(const char *errorMsg){
    perror(errorMsg);
    exit(1);
}

void error_response(int socket, string msg)
{
    string msg_to_serialize;
    Payload *error_to_payload = new Payload();
    error_to_payload->set_sender(SENDER);
    error_to_payload->set_message(msg);
    error_to_payload->set_code(BAD_RESPONSE);
    error_to_payload->SerializeToString(&msg_to_serialize);

    char buffer[msg_to_serialize.size() + 1];
    strcpy(buffer, msg_to_serialize.c_str());
    bytesSent = send(socket, buffer, sizeof buffer, 0);
    txByteCount += bytesSent;
    messagesSent++;
}


void *client_handling(void *params)
{
    // Utilizados por los distintos mensajes
    client_struct client;
    client_struct *nuevo_client = (client_struct *)params;
    // int socket = *(int *)params;
    char buffer[BUFFER_SIZE];
    int socket = nuevo_client->socket;
    

    string serial;
    Payload pay_LOAD;

    // ttest
    cout << "Servidor: Hola! soy el thread que se encarga del client asociado al socket "
              << socket
              << endl;


    while(1)
    {
        if ((bytesReceived = recv(socket, buffer, BUFFER_SIZE, 0)) < 1)
        {
            if (bytesReceived != 0)
            {
                exit(1);
            }
            break;
        }
        
        pay_LOAD.ParseFromString(buffer);
        rxByteCount += bytesReceived;
        messagesReceived++;

        // send a general message
        switch (pay_LOAD.flag()){
            case Payload_PayloadFlag_general_chat:
            {
                cout << "General message: (" << pay_LOAD.message() << "), from " << client.username << endl;
                
                Payload *res = new Payload();
                res->set_sender(SENDER);
                res->set_message("General message sent successfully.");
                res->set_code(OK);
                res->SerializeToString(&serial);
                strcpy(buffer, serial.c_str());
                send(socket, buffer, serial.size() + 1, 0);

                Payload *general = new Payload();
                general->set_sender(SENDER);
                general->set_message("(GENERAL) " + pay_LOAD.sender() + " said: " + pay_LOAD.message() + "\n");
                general->set_code(OK);
                general->set_flag(pay_LOAD.flag());
                general->SerializeToString(&serial);
                strcpy(buffer, serial.c_str());
                for (auto item = clients.begin(); item != clients.end(); ++item)
                {
                    if (item->first != client.username)
                    {
                        send(item->second->socket, buffer, serial.size() + 1, 0);
                    }
                }

            break;
            }
            // send PM
            case Payload_PayloadFlag_private_chat:
            {
                if (clients.count(pay_LOAD.extra()) > 0)
                {
                cout << "Private message: (" << pay_LOAD.message() << ") from " << client.username << ". To " << pay_LOAD.extra() << endl;

                Payload *res = new Payload();
                res->set_sender(SENDER);
                res->set_message("Private message sent successfully.");
                res->set_code(OK);
                res->SerializeToString(&serial);
                strcpy(buffer, serial.c_str());
                send(socket, buffer, serial.size() + 1, 0);

                Payload *private_message = new Payload();
                private_message->set_sender(client.username);
                private_message->set_message("(PRIVATE) " + pay_LOAD.sender() + " said: " + pay_LOAD.message() + "\n");
                private_message->set_code(OK);
                private_message->set_flag(pay_LOAD.flag());
                private_message->SerializeToString(&serial);
                int destSocket = clients[pay_LOAD.extra()]->socket;
                strcpy(buffer, serial.c_str());
                send(destSocket, buffer, serial.size() + 1, 0);
                }

            break;
            }
            // user_list
            case Payload_PayloadFlag_user_list:
            {
                cout << "List of all users for " << client.username << endl;
                string user_list = "";
                for (auto item = clients.begin(); item != clients.end(); ++item)
                {
                    user_list = user_list + "username: " + (item->first) + "ip: " + (item->second->ipAddr) + " status: " + (item->second->status) + "\n";
                }

                Payload *listing = new Payload();
                listing->set_sender(SENDER);
                listing->set_message(user_list);
                listing->set_code(OK);
                listing->set_flag(pay_LOAD.flag());
                listing->SerializeToString(&serial);
                strcpy(buffer, serial.c_str());
                send(socket, buffer, serial.size() + 1, 0);
            
            break;
            }
            // user_info
            case Payload_PayloadFlag_user_info:
            {
                if (clients.count(pay_LOAD.extra()) > 0)
                {
                    cout << "User info of " << pay_LOAD.extra() << " requested by " << client.username << endl;

                    Payload *user_info = new Payload();
                    client_struct *informer = clients[pay_LOAD.extra()];
                    string info = "";
                    info = "username: " + (informer->username) + " ip: " + (informer->ipAddr) + " status: " + (informer->status) + "\n";

                    user_info->set_sender(SENDER);
                    user_info->set_message(info);
                    user_info->set_code(OK);
                    user_info->set_flag(pay_LOAD.flag());
                    user_info->SerializeToString(&serial);
                    strcpy(buffer, serial.c_str());
                    send(socket, buffer, serial.size() + 1, 0);
                }
            
            break;
            }
            // update status
            case Payload_PayloadFlag_update_status:
            {
                cout << "Changing status of " << client.username << " to " << pay_LOAD.extra() << endl;
                client.status = pay_LOAD.extra();

                Payload *change_status = new Payload();
                change_status->set_sender(SENDER);
                change_status->set_message(pay_LOAD.extra());
                change_status->set_code(OK);
                change_status->set_flag(pay_LOAD.flag());
                change_status->SerializeToString(&serial);
                strcpy(buffer, serial.c_str());
                send(socket, buffer, serial.size() + 1, 0);
            
            break;
            }
            // Servicio que manda un mensaje general
            case Payload_PayloadFlag_register_:
            {
                cout << "Registering " << pay_LOAD.sender() << endl;

                if (clients.count(pay_LOAD.sender()) == 0){

                Payload *register_user = new Payload();
                register_user->set_sender(SENDER);
                register_user->set_message("Registered.");
                register_user->set_flag(pay_LOAD.flag());
                register_user->set_code(OK);
                register_user->SerializeToString(&serial);
                strcpy(buffer, serial.c_str());
                send(socket, buffer, serial.size() + 1, 0);

                cout << "User with id: " << socket << " has entered the chat." << endl;

                // Guardar informacion de nuevo cliente
                client.username = pay_LOAD.sender();
                client.socket = socket;
                client.status = "ACTIVO";
                strcpy(client.ipAddr, nuevo_client->ipAddr);
                clients[client.username] = &client;
                
                }

            break;
            }
            default:
            {
                string error_msg = "Option not currently supported.";
                error_response(socket, error_msg);
            }
        }

        print_server_stats();
    }


    cout << "Disconnecting... " << endl;
    clients.erase(client.username);
    close(socket);
    pthread_exit(0);
}

int main(int argc, char *argv[])
{

    if (argc != 2){
        return 1;
    }
    int port = strtol(argv[1], NULL, 10);

    sockaddr_in server_connection;
    sockaddr_in client_connection;
    socklen_t client_connection_size;
    int sockett = 0;
    int new_socket = 0;
    char client_connection_addr[INET_ADDRSTRLEN];

    server_connection.sin_family = AF_INET;
    server_connection.sin_addr.s_addr = INADDR_ANY;
    server_connection.sin_port = htons(port);
    memset(server_connection.sin_zero, 0, sizeof server_connection.sin_zero);

    if ((sockett = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Error creating socket.\n");
        return 1;
    }

    // bindear address y puerto a socket
    if (bind(sockett, (struct sockaddr *)&server_connection, sizeof(server_connection)) == -1)
    {
        close(sockett);
        fprintf(stderr, "Error binding IP.\n");
        return 2;
    }

    // escuchar conexiones, cola maxima de d
    if (listen(sockett, BACKLOG) == -1)
    {
        close(sockett);
        fprintf(stderr, "Error on listen().\n");
        return 3;
    }
    printf("Listening on port: %d\n", port);

    // loop para aceptar conexiones
    while (1)
    {
        client_connection_size = sizeof client_connection;
        new_socket = accept(sockett, (struct sockaddr *)&client_connection, &client_connection_size);

        client_struct nuevo_client;
        nuevo_client.socket = new_socket;
        inet_ntop(AF_INET, &(client_connection.sin_addr), nuevo_client.ipAddr, INET_ADDRSTRLEN);

        // despachar thread de nuevo client
        pthread_t thread_id;
        pthread_attr_t attrs;
        pthread_attr_init(&attrs);
        // pthread_create(&thread_id, &attrs, client_handling, (void *)&new_socket);
        pthread_create(&thread_id, &attrs, client_handling, (void *)&nuevo_client);
    }

    return 0;
}
