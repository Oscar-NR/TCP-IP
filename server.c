

#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<errno.h>


#define PORT 5200 		//Default port of the server
#define MAX_CLIENTS 10	//Max number of client
#define BUFFER_SIZE 512	//Size of the buffer for recieve and send messages


//Function to obtain the biggest number of descriptors
int get_max_fd(int actual, int new)
{
	if(actual>= new)
	{
		return actual; 
	}else
	{
		return new;
	}	
}


//Funcion to copy the set of descriptors, because the select will modify the same
void copy_fdset(fd_set* destiny, fd_set* origen, int biggest_sd_check)
{
	int i;
	FD_ZERO(destiny);
	for(i=0; i<biggest_sd_check;i++)
	{
		if( FD_ISSET(i,origen) )
		{
			FD_SET(i, destiny);
		}	
	}
}


//Write can fail and write less character than should, this custom function take care of all the characters are writed
ssize_t custom_write(int fd, char* message, size_t message_lenght)
{
	ssize_t to_write = message_lenght;
	ssize_t total_writed = 0;
	ssize_t writed;
	do
	{
		errno= 0;
		writed = write(fd, message+ total_writed, to_write);
		if(writed>= 0)
		{
			total_writed+= writed;
			to_write -= writed;		
		}
		
	}while(
		( (writed>0) && (total_writed< message_lenght)) ||
		(errno == EINTR));
	
	if(total_writed>0)
	{
		return total_writed;	
	}else
	{
		//Maybe an error happened, we can return that with this
		return writed;
	}
}





int main(int argc, char* argv[])
{
	
	char buffer[BUFFER_SIZE];

	int listen_sd;				//socket descriptor which will listen	
	int client_sd[MAX_CLIENTS];	//array for the clients, this socket will be created by request of a client
	
	int biggest_sd;				//to check up to the biggest sd intead of check every one
	int received, sended;		//to made the code easier to read
	int select_result;			//to made the select function more readable
	int assigned;				//will be used to check if the descriptors of client_sd are assigned or can be used
		
	struct sockaddr_in client_address;
	struct sockaddr_in server_address;
	socklen_t address_length;		
	
	fd_set original_set, modified_set;
	
	
	
	
	//set as free all the sd in the array of clients
	for(int i=0; i< MAX_CLIENTS; i++)
	{
		client_sd[i]=-1;	//if there is a place for the new client the accept function will return a non negative value
	}
	
	
	//now create the socket which will listen for new clients	
	listen_sd = socket(PF_INET, SOCK_STREAM,0);
	if(listen_sd < 0)
	{
		perror("error creating the listen socket");
		exit(1);
	}
	
	
	//Set the struct of the server address
	memset(&server_address, 0, sizeof(server_address));	//Cleaning the memory
	server_address.sin_family = AF_INET;			//select the IPv4 of the protocols
	server_address.sin_addr.s_addr = INADDR_ANY;	//bind the socket to all local interfaces
	server_address.sin_port = htons(PORT);			//the number of the port, doing htons to avoid problems with the big/little endian
	
	//Bind the listen socket to the server adress & check for error
	if(  bind( listen_sd, (struct sockaddr* ) &server_address, sizeof(server_address) )  <0  )
	{
		perror("error creating the socket");
		exit(1);
	}
	
	//set the socket as passive to be able to listen & check for error
	if( listen(listen_sd,5) <0 )
	{
		perror("error in listen");
		exit(1);
	}
	
	//Now the configuration of the set of  sockets descriptors	
	FD_ZERO(&original_set);
	//add the listen socket to wake up select() when any request came
	FD_SET(listen_sd, &original_set);
	
	//the listen socket is the only one, so for now, is the biggest descrpitor
	biggest_sd = listen_sd;
	
	
	printf("starting loop of the server\n");
	//server loop
	while(1)
	{
		copy_fdset(&modified_set, &original_set, biggest_sd+1);
	
		select_result = select( biggest_sd+1, &modified_set, NULL, NULL, NULL);		
		//if we don't use select_result read the select function and the check for error would be harder to read
		if(select_result < 0)
		{
			perror("error in the select");
			exit(1);
		}
		
		//Check if one client has sended a message
		for(int i =0; i<MAX_CLIENTS; i++)
		{
			if(client_sd[i]>0 && FD_ISSET(client_sd[i], &modified_set))
			{
				//recieve data
				received = recv(client_sd[i], buffer, BUFFER_SIZE, 0);
				if(received>0)
				{
					//send this message to the rest of clients of the chat
					for(int j=0; j<MAX_CLIENTS;j++)
					{
						//check if the client_sd[i] has a client and the client that will receive the message is different that the one who sended
						if( (client_sd[i]!= -1) && (i!=j) )
						{
							sended = custom_write(client_sd[j], buffer, received); 
							if(sended != received)
							{						
								//This means that the client is disconected or an error happened
								//	this will be solved in the next lines with FD_CLR(client_sd[i], &original_set)
							}					
						}					
					}
			
				}else
				{
					//if the socket didn't received anything means that the client has disconected, so close their socket
					if(received<=0)
					{
						FD_CLR(client_sd[i], &original_set);
						close(client_sd[i]);
						client_sd[i] =-1;				
					}			
				}		
			}
		
		}	
		int iterator = 0;
		//Check if the listen_sd has recieved a request from a new client
		if( FD_ISSET(listen_sd, &modified_set) )
		{
			assigned = 0;
			
			while(iterator<MAX_CLIENTS && assigned ==0)
			{
				if(client_sd[iterator]==-1)
				{
					assigned =1;				
				}else
				{
					iterator++;
				}			
			}
			
			//if client_sd has a space for the new client we open the conexion
			if(assigned==1)
			{
				address_length = sizeof(client_address);
				client_sd[iterator] = accept(listen_sd, (struct sockaddr*) &client_address, &address_length);
				if(client_sd[iterator] <0)
				{
					perror("error in the accept");
					exit(1);
				}
				
				//add the new client to the set of descriptor
				FD_SET(client_sd[iterator], &original_set);
				
				//update the biggest number of descriptor
				biggest_sd = get_max_fd(biggest_sd,client_sd[iterator]);					
			}
			
		}
		
	
	}//end of the while loop
	
	//close the socket which listen to end
	close(listen_sd);
		
	return 0;
}



















