#include <stdio.h>
#include <string.h>
#include <iostream>
#include "server.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;

bool successfulLoginClient = false;
//asking user to login
char* username = new char[20];
char* password = new char[20];

#define PROMPT(p, invar) { cout << p << ": "; cin >> invar; }

void clientInterface()
{
  // login
  while(!successfulLoginClient)
  {
    printf("Please enter your username: ");
    scanf("%s", username);
    printf("Password: ");
    scanf("%s", password);

    string loginInformation;
    loginInformation+= username;
    loginInformation+='\0';
    loginInformation+= password;
    loginInformation+='\0';
    char* loginInfo = (char*)loginInformation.c_str();
    sendPacket(false,loginInfo, strlen(loginInfo), COMMAND_LOGIN);
    waitpid(-1, NULL, 0);
    // TODO: this needs to be a bit more complicated
  }

  string inputCommand;
  cout << "> ";
  cin >> inputCommand;
  while(inputCommand != "exit")
  {
    if(inputCommand == "set password")
    {
      //changing password
      string pass;;
      cout << "Please enter the new password: ";
      cin >> pass;
      sendPacket(false,pass.c_str(), pass.length(), COMMAND_SETPASSWORD);
    }
    else if(inputCommand == "upload photo")
    {
      //adding photo
      string personID, type, photoFileName;
      char* photo;
      
      PROMPT("Please enter the person's id", personID);
      PROMPT("Please enter the photo type (1-4)", type);
      PROMPT("Please enter the photo file path", photoFileName);
      
      //getting the photo
      FILE *fp;
      long len;
      fp = fopen(photoFileName.c_str(), "rb");
      if(fp==NULL)
      {
        printf("Invalid file path.");
        continue;
      }
      fseek(fp, 0, SEEK_END);
      len = ftell(fp);
      fseek(fp,0,SEEK_SET);
      photo = new char[len];
      fread(photo, len, 1, fp);
      fclose(fp);
      
      string strMsg;
      strMsg+=personID;
      strMsg+='\0';
      strMsg+=type;
      strMsg+='\0';
      strMsg+=photo;
      strMsg+='\0';
      
      sendPacket(false, strMsg.c_str(), strMsg.length(), COMMAND_UPLOADPHOTO);

      delete[] photo;
    }
    else if(inputCommand == "download photo")
    {
      //getting photo
      string photoID;
      PROMPT("Please enter the photo's id", photoID);
      
      sendPacket(false,photoID.c_str(), photoID.length(), COMMAND_UPLOADPHOTO);
    }
    else if(inputCommand == "list photos by person")
    {
      //checking photos
      string personID;
      PROMPT("Please enter the person's id", personID);

      sendPacket(false,personID.c_str(), personID.length(), COMMAND_QUERYPHOTOS);
    }
    else if(inputCommand == "list people")
    {
      //checking people
      sendPacket(false,"this will never be read", 23, COMMAND_QUERYPHOTOS);	
    }
    else if(inputCommand == "add person")
    {
      //adding a person
      string firstName, lastName, location;
      PROMPT("Please enter the person's first name", firstName);
      PROMPT("Please enter the person's last name", lastName);
      PROMPT("Please enter the person's location", location);

      string strMsg;
      strMsg+=firstName;
      strMsg+='\0';
      strMsg+=lastName;
      strMsg+='\0';
      strMsg+=location;
      strMsg+='\0';
      sendPacket(false, strMsg.c_str(), strMsg.length(), COMMAND_ADDPERSON);
    }
    else if(inputCommand == "remove person")
    {
      //removing a person
      string personID;
      PROMPT("Please enter the person's id", personID);
      sendPacket(false,personID.c_str(), personID.length(), COMMAND_REMOVEPERSON);
    }
    else if(inputCommand == "remove photo")
    {
      //removing a photo
      string photoID;
      PROMPT("Please enter the photo's id", photoID);
      sendPacket(false, photoID.c_str(), photoID.length(), COMMAND_REMOVEPHOTO);
    }
    else
    {
      //not a valid command
      printf("The proper commands are : 'login', 'set password', 'upload photo', 'download photo', 'query photos', 'list people', 'add person', 'remove person', and 'remove photo'.");
    }

    cout << "> ";
    cin >> inputCommand;
  }
}
