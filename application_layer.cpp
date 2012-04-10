#include <sys/wait.h>
#include <sys/prctl.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <fstream>
#include "err_macros.h"
#include "physical_layer.h"
#include "data_link_layer.h"
#include "server.h"
#include "client.h"

using namespace std;

pid_t dll_pid;
bool already_sent = false;
bool is_server = false;

int rc;
sqlite3* db;

void handle_app_signals(int, siginfo_t*, void*);

void sendPacket(bool isErr, const char* payload, int payload_len, int command)
{
  SHM_GRAB_NEW(struct packet, p, packetid);
  if (!isErr)
  {
    memcpy(p->payload, payload, payload_len);
    p->pl_data_len = payload_len;
  }
  else
  {
    p->payload[0] = 'E'; 
    p->payload[1] = 'R';
    p->payload[2] = 'R';

    p->pl_data_len = 3;
  }
  p->command_type = command;
  p->seq_num = 0;

  sigval v;
  v.sival_int = packetid;
  POST_INFO("APPLICATION_LAYER: Sending packet with command " << command);
  sigqueue(dll_pid, SIG_NEW_PACKET, v);
  SHM_RELEASE(struct packet, p);
}

void fork_to_new_client(int comm_sock)
{
  if (fork() == 0)
  {
    pid_t my_pid = getpid();
    
    // TODO: see if we actually have an argument
    is_server = true;

    dll_pid = fork();
    if(dll_pid < 0)
    {
      //failed to fork
      POST_ERR("APPLICATION_LAYER: Failed to fork off data link layer...");
    }
    else if (dll_pid == 0) // we are the child
    {
      if(is_server) prctl(PR_SET_NAME, (unsigned long) "prog1s_dll", 0, 0, 0);
      else prctl(PR_SET_NAME, (unsigned long) "prog1c_dll", 0, 0, 0);
      init_data_link_layer(true, my_pid, true, comm_sock);
    }
    else
    {
      srandom(getpid());
      struct sigaction act;
      act.sa_sigaction = &handle_app_signals;
      sigemptyset(&act.sa_mask);
      act.sa_flags = SA_SIGINFO | SA_RESTART;

      sigaction(SIG_NEW_PACKET, &act, 0);
      sigaction(SIG_FLOW_ON, &act, 0);
      sigaction(SIG_NEW_CLIENT, &act, 0);
    }

    while(1) waitpid(-1, 0, 0);
  }
}

void send_a_test_packet()
{
  SHM_GRAB_NEW(struct packet, p, packetid);
  p->payload[0] = 'h';
  p->payload[1] = 'i';
  p->payload[2] = '!';
  p->command_type = 0;
  p->pl_data_len = 3;
  p->seq_num = 0;

  sigval v;
  v.sival_int = packetid;
  POST_INFO("APP_LAYER: sending new test packet");
  sigqueue(dll_pid, SIG_NEW_PACKET, v);
  SHM_RELEASE(struct packet, p);
}

void handle_app_signals(int signum, siginfo_t* info, void* context)
{
  POST_INFO("APPLICATION_LAYER: got signal " << signum);
  /*if (signum == SIG_FLOW_ON && !already_sent && !is_server)
  {
    already_sent = true;
    send_a_test_packet(); // NOTE: shouldn't be calling an external function from here unless we make it async-safe
  }*/
  if (signum == SIGSEGV)
  {
    POST_ERR("APPLICATION_LAYER: Segfault!");
    exit(2);
  }
  else if (signum == SIG_NEW_CLIENT)
  {
    POST_INFO("APPLICATION_LAYER: New client on socket " << info->si_value.sival_int << ", forking new stack...");
    fork_to_new_client(info->si_value.sival_int);
  }
  else if (signum == SIG_NEW_PACKET)
  {
    POST_INFO("APPLICATION_LAYER: New packet incoming!");
    //POST_INFO("DATA_LINK_LAYER: Memory at path '" << (char *)info->si_value.sival_int << "'...");
    SHM_GRAB(struct packet, pack, (info->si_value.sival_int));
    // do some stuff, memcpy
    POST_INFO("APPLICATION_LAYER: Packet has command type of " << pack->command_type);
    if(!is_server)
    {
  		if(pack->command_type == COMMAND_DOWNLOADPHOTO){
  		//need to save the photo
  			string fileName;
  			printf("Please enter the desired file name: ");
        cin >> fileName;
  			ofstream outputFile;
  			outputFile.open(fileName.c_str());
  			outputFile<<pack->payload;
  			printf("Photo downloaded.");
  		}
  		else
      {
  			//just getting a confirmation/denial
  			char* buffer = new char[pack->pl_data_len];
  			memcpy(buffer, pack->payload, pack->pl_data_len);
        POST_INFO("APPLICATION_LAYER: " << buffer);
        if (pack->command_type == COMMAND_LOGIN)
        {
          if (buffer[0] == 'L') setClientLoggedIn(true);
          else setClientLoggedIn(false);
        }
        delete[] buffer;
  		}
  	}
  	else
    {
  		if(pack->command_type == COMMAND_LOGIN)
      {
        POST_INFO("APPLICATION_LAYER: login attempt");
  			int offset = 0;
  			int prevOffset = 0;
  			char* buffer = new char[pack->pl_data_len];
        char* username = new char[20];
        char* password = new char[20];

  			memcpy(buffer, pack->payload, pack->pl_data_len);
  			offset = strlen(buffer);
  			memcpy(username, buffer, offset);
  			prevOffset = offset;
  			offset = strlen(buffer+prevOffset);
  			memcpy(password, buffer+prevOffset, offset);
  			prevOffset = offset;
        loginAttempt(username, password);

        delete[] buffer, username, password;
        
  		}
  		else if(pack->command_type == COMMAND_UPLOADPHOTO)
      {
  			//adding in a new person
  			int offset =0;
  			int prevOffset = 0;
  			char* buffer = new char[pack->pl_data_len];
        char* personID = new char[20];
        char* type = new char[1];
        char* BLOB = new char[16000]; // photo at most 16k

  			memcpy(buffer, pack->payload, pack->pl_data_len);
  			offset = strlen(buffer);
  			memcpy(personID, buffer, offset);
  			prevOffset = offset;
  			offset = strlen(buffer+prevOffset);
  			memcpy(type, buffer+prevOffset, offset);
  			prevOffset = offset;
  			offset = strlen(buffer+prevOffset);
  			memcpy(BLOB, buffer+prevOffset, offset);
  			insertPhoto(personID, type, BLOB);

        delete[] buffer, personID, type, BLOB;
  		}
  		else if(pack->command_type == COMMAND_DOWNLOADPHOTO){
  			//adding new photo
  			char* buffer = new char[pack->pl_data_len];
  			memcpy(buffer, pack->payload, pack->pl_data_len);
  			downloadPhoto(buffer);
        delete[] buffer;
  		}
  		else if(pack->command_type == COMMAND_QUERYPHOTOS){
  			//getting a person's photo information
  			char* buffer = new char[pack->pl_data_len];
  			memcpy(buffer, pack->payload, pack->pl_data_len);
  			selectPhotos(buffer);
        delete[] buffer;
  		}
  		else if(pack->command_type == COMMAND_LISTPEOPLE){
  			//getting the current list of people
  			selectPeople();
  		}
  		else if(pack->command_type == COMMAND_ADDPERSON){
  			//adding in a new person
  			int offset =0;
  			int prevOffset = 0;
  			char* buffer = new char[pack->pl_data_len];
        char* firstName = new char[15];
        char* lastName = new char[20];
        char* location = new char[36]; // photo at most 16k
  			memcpy(buffer, pack->payload, pack->pl_data_len);
  			offset = strlen(buffer);
  			memcpy(firstName, buffer, offset);
  			prevOffset = offset;
  			offset = strlen(buffer+prevOffset);
  			memcpy(lastName, buffer+prevOffset, offset);
  			prevOffset = offset;
  			offset = strlen(buffer+prevOffset);
  			memcpy(location, buffer+prevOffset, offset);
  			insertPeople(firstName, lastName, location);
        delete[] buffer, firstName, lastName, location;
  		}
  		else if(pack->command_type == COMMAND_REMOVEPERSON){
  			//getting rid of a person
  			char* buffer = new char[pack->pl_data_len];
  			memcpy(buffer, pack->payload, pack->pl_data_len);
  			removePeople(buffer);
        delete[] buffer;
  		}
  		else if(pack->command_type == COMMAND_REMOVEPHOTO){
  			//getting rid of a photo
  			char* buffer = new char[pack->pl_data_len];
  			memcpy(buffer, pack->payload, pack->pl_data_len);
  			removePhoto(buffer);
        delete[] buffer;
  		}
  		else if(pack->command_type == COMMAND_SETPASSWORD){
  			//chaning password
  			char* buffer = new char[pack->pl_data_len];
  			memcpy(buffer, pack->payload, pack->pl_data_len);
  			changePassword(buffer);
        delete[] buffer;
  		}
  	}
    SHM_RELEASE(struct packet, pack); 
    SHM_DESTROY((info->si_value.sival_int));
  }
}

int main(int argc, char* argv[])
{
  pid_t my_pid = getpid();
  
  // TODO: see if we actually have an argument
  if (argv[1][0] == '1')
  {
    POST_INFO("APPLICATION_LAYER: We are a server");
    is_server = true;
  }
  else POST_INFO("APPLICATION_LAYER: We are a client");

  dll_pid = fork();
  if(dll_pid < 0)
  {
    //failed to fork
    POST_ERR("APPLICATION_LAYER: Failed to fork off data link layer...");
  }
  else if (dll_pid == 0) // we are the child
  {
    if(is_server) prctl(PR_SET_NAME, (unsigned long) "prog1s_dll", 0, 0, 0);
    else prctl(PR_SET_NAME, (unsigned long) "prog1c_dll", 0, 0, 0);
    init_data_link_layer(argv[1][0] == '1', my_pid, false, 0);
  }
  else
  {
    srandom(getpid());
    struct sigaction act;
    act.sa_sigaction = &handle_app_signals;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO | SA_RESTART;

    sigaction(SIG_NEW_PACKET, &act, 0);
    sigaction(SIG_FLOW_ON, &act, 0);
    sigaction(SIG_NEW_CLIENT, &act, 0);
    sigaction(SIGSEGV, &act, 0);

    if (is_server)
    {
      rc = sqlite3_open("test.db", &db);
      if(rc)
      {
        POST_ERR("APPLICATION_LAYER: Database could not be accessed");
        sqlite3_close(db);
      }
      else
      {
        POST_INFO("APPLICATION_LAYER: Database opened successfully");
        initDB(db);
      }
    }
    else
    {
      clientInterface();
    }
  }

  while(1) waitpid(-1, 0, 0);
}
