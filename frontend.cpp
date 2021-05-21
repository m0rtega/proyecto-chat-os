/*
** Codigo de cliente. Parte del codigo esta basado en lo que leimos en la guia del maestro Beej:
** https://beej.us/guide/bgnet/html/
*/
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <string>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <queue>
#include <netdb.h>
#include <pthread.h>
#include <ifaddrs.h>
#include "payload.pb.h"

#define default_ip "127.0.0.1"
#define BUFFER 8192
#define BAD_RESPONSE 500
#define OK 200

using namespace google::protobuf;
using namespace std;

void menu(char *username)
{
	cout << "\nUsername: " << username << "\n" 
	<< "MENU\n" 
 	<< "1. Send a general message. \n"
	<< "2. Send a direct message. \n"
	<< "3. Change status.\n"
	<< "4. User list. \n"
	<< "5. Get a user's info. \n"
	<< "6. Quit. \n"
	<< "-------------------------- \n"
	<< "Input an option: " << endl;
}

int connected, serverListen, inputListen;
string statuses[3] = {"ACTIVO", "OCUPADO", "INACTIVO"};

void *get_messages(void *args)
{
	while (1)
	{
		char buff[BUFFER];
		int *socket_fd = (int *)args;
		Payload servComms;
		int bytesReceived = recv(*socket_fd, buff, BUFFER, 0);
		servComms.ParseFromString(buff);
		if (servComms.code() == BAD_RESPONSE)
		{
			cout << servComms.message() << endl;
		}
		else if (servComms.code() == OK || servComms.flag() == Payload_PayloadFlag_general_chat)
		{
			cout << servComms.message() << endl;
		}
		else
		{
			break;
		}
		serverListen = 0;
		if (connected == 0){
			pthread_exit(0);
		}
	}
}

// get_socket_address
void *get_socket_address(struct sockaddr *socket_address)
{
	if (socket_address->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)socket_address)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)socket_address)->sin6_addr);
}

int main(int argc, char *argv[])
{

	GOOGLE_PROTOBUF_VERIFY_VERSION;

	char buffer[BUFFER];
	char ip_server[INET6_ADDRSTRLEN];
	struct addrinfo hints, *servinfo, *addr;

	if (argc != 4){
		return 1;
	}

	// Connection to the socket
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	int socket_fd, numbytes, er;

	er = getaddrinfo(argv[2], argv[3], &hints, &servinfo);
	if (er != 0){
		return 1;
	}

	for (addr = servinfo; addr != NULL; addr = addr->ai_next)
	{
		if ((socket_fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol)) == -1)
		{
			continue;
		}
		if (connect(socket_fd, addr->ai_addr, addr->ai_addrlen) == -1)
		{
			perror("There has been a socket error.");
			close(socket_fd);
			continue;
		}

		break;
	}
	if (addr == NULL){
		fprintf(stderr, "Connection failed.");
		return 2;
	}
	inet_ntop(addr->ai_family, get_socket_address((struct sockaddr *)addr->ai_addr),
			  ip_server, sizeof ip_server);
	printf("Handshake server %s\n", ip_server);
	freeaddrinfo(servinfo);

	string serial;

	Payload *handshake = new Payload;
	handshake->set_sender(argv[1]);
	handshake->set_flag(Payload_PayloadFlag_register_);
	handshake->set_ip(ip_server);
	handshake->SerializeToString(&serial);

	strcpy(buffer, serial.c_str());
	send(socket_fd, buffer, serial.size() + 1, 0);

	recv(socket_fd, buffer, BUFFER, 0);

	Payload server_response;
	server_response.ParseFromString(buffer);

	if(server_response.code() == BAD_RESPONSE){
			return 1;
	}

	cout << "Connected to server." << endl;	
	connected = 1;

	pthread_t thread_id;
	pthread_attr_t attrs;
	pthread_attr_init(&attrs);
	//threads that listens to messages
	pthread_create(&thread_id, &attrs, get_messages, (void *)&socket_fd);

	int op;

	while(1)
	{
		while (serverListen == 1){}
		
		menu(argv[1]);
		cin >> op;

		string message_to_serialize;

		int bytesReceived, bytesSent;
		switch (op){
			case 1:{
				inputListen = 1;
				cout << "Write a message: " << endl;
				cin.ignore();
				string message;
				getline(cin, message);


				Payload *client_message= new Payload();
				client_message->set_sender(argv[1]);
				client_message->set_message(message);
				client_message->set_flag(Payload_PayloadFlag_general_chat);
				client_message->set_ip(ip_server);
				client_message->SerializeToString(&message_to_serialize);

				strcpy(buffer, message_to_serialize.c_str());
				bytesSent = send(socket_fd, buffer, message_to_serialize.size() + 1, 0);
				serverListen = 0;
				inputListen = 0;
				break;
			}
			case 2:
			{
				cout << "User to send message? " << endl;
				cin.ignore();
				string name;
				getline(cin, name);

				cout << "\nWrite a message: " << endl;
				string message;
				getline(cin, message);

				Payload *client_message= new Payload();
				client_message->set_sender(argv[1]);
				client_message->set_message(message);
				client_message->set_flag(Payload_PayloadFlag_private_chat);
				client_message->set_extra(name);
				client_message->set_ip(ip_server);
				client_message->SerializeToString(&message_to_serialize);

				strcpy(buffer, message_to_serialize.c_str());
				bytesSent = send(socket_fd, buffer, message_to_serialize.size() + 1, 0);
				serverListen = 0;
				break;
			}
			case 3:{

				cout << "New status?: \n"
					<< "1. ACTIVE\n" 
					<< "2. BUSY\n" 
					<< "3. INACTIVE\n" << endl;
				
				int stat;
				cin >> stat;
				string new_status;
				if (stat <= 3){
					new_status = statuses[stat-1];
				}
				else
				{
					continue;
				}
				Payload *user_info = new Payload();
				user_info->set_sender(argv[1]);
				user_info->set_flag(Payload_PayloadFlag_update_status);
				user_info->set_extra(new_status);
				user_info->set_ip(ip_server);
				user_info->SerializeToString(&message_to_serialize);

				strcpy(buffer, message_to_serialize.c_str());
				bytesSent = send(socket_fd, buffer, message_to_serialize.size() + 1, 0);
				serverListen = 1;
				break;
			}
			case 4:
			{
				Payload *users_list = new Payload();
				users_list->set_sender(argv[1]);
				users_list->set_ip(ip_server);
				users_list->set_flag(Payload_PayloadFlag_user_list);
				users_list->SerializeToString(&message_to_serialize);

				strcpy(buffer, message_to_serialize.c_str());
				bytesSent = send(socket_fd, buffer, message_to_serialize.size() + 1, 0);
				serverListen = 1;
				break;
			}
			case 5:
			{
				string name;
				printf("Enter the user's username: \n");
				cin.ignore();
				getline(cin, name);

				Payload *user_info = new Payload();
				user_info->set_sender(argv[1]);
				user_info->set_flag(Payload_PayloadFlag_user_info);
				user_info->set_extra(name);
				user_info->set_ip(ip_server);
				user_info->SerializeToString(&message_to_serialize);

				strcpy(buffer, message_to_serialize.c_str());
				bytesSent = send(socket_fd, buffer, message_to_serialize.size() + 1, 0);
				serverListen = 1;
				break;
			}
			case 6:
			{
				int o;
				cout << "1. End connection." << endl;
				cout << "2. Keep on running. \n" << endl;
				cout << "Enter an option: " << endl;
				cin.ignore();
				cin >> o;
				if (o == 1){
					exit(1);
				} else {
					break;
				}
			}
			default:
			{
				cout << "That option doesn't exist." << endl;
			}
		}
	}
	pthread_cancel(thread_id);
	connected = 0;
	close(socket_fd);
	return 0;
}

