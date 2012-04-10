#include <stdio.h>
#include <string.h>
#include <iostream>
#include "server.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "err_macros.h"

using namespace std;

bool successfulLoginClient = false;
bool loginTouched = false;
//asking user to login

#define PROMPT(p, invar) { cout << p << ": "; cin >> invar; }

void setClientLoggedIn(bool v)
{
  successfulLoginClient = v;
  loginTouched = true; 
}

void clientInterface()
{
  // login
  while(!successfulLoginClient)
  {
    string username;
    string password;
    PROMPT("username", username);
    PROMPT("password", password);

    string loginInformation;
    loginInformation+= username;
    loginInformation += '\0';
    loginInformation+= password;
    loginInformation += '\0';

    sendPacket(false, loginInformation.c_str(), username.length()+password.length()+2, COMMAND_LOGIN);
    // TODO: this needs to be a bit more complicated
    while(!loginTouched) {}
  }

  string inputCommand;
  cout << "> ";
  cin >> inputCommand;
  while(inputCommand.find( "exit", 0) == string::npos)
  {
    cout << inputCommand << endl;
    if(inputCommand.find("setpassword", 0) == 0)
    {
      //changing password
      string pass;;
      cout << "Please enter the new password: ";
      cin >> pass;
      sendPacket(false,pass.c_str(), pass.length(), COMMAND_SETPASSWORD);
    }
    else if(inputCommand.find( "uploadphoto",0) == 0)
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

      string photos(photo);
      
      int spaceLeft = 256 - strMsg.length();
      strMsg+= photos.substr(0, spaceLeft-1);
      strMsg+='\0';

      sendPacket(false, strMsg.c_str(), strMsg.length(), COMMAND_UPLOADPHOTO);

      string p2 = photos.substr(spaceLeft, spaceLeft+255);
      p2 += '\0';

      sendPacket(false, p2.c_str(), p2.length(), COMMAND_UPLOADPHOTO);

      delete[] photo;
    }
    else if(inputCommand.find("downloadphoto", 0) == 0)
    {
      //getting photo
      string photoID;
      PROMPT("Please enter the photo's id", photoID);
      
      sendPacket(false,photoID.c_str(), photoID.length(), COMMAND_UPLOADPHOTO);
    }
    else if(inputCommand.find( "listphotosbyperson", 0) == 0)
    {
      //checking photos
      string personID;
      PROMPT("Please enter the person's id", personID);

      sendPacket(false,personID.c_str(), personID.length(), COMMAND_QUERYPHOTOS);
    }
    else if(inputCommand.find( "listpeople", 0) == 0)
    {
      //checking people
      sendPacket(false,"this will never be read", 23, COMMAND_LISTPEOPLE);	
    }
    else if(inputCommand.find("addperson", 0) != string::npos)
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
    else if(inputCommand.find( "removeperson", 0) == 0)
    {
      //removing a person
      string personID;
      PROMPT("Please enter the person's id", personID);
      sendPacket(false,personID.c_str(), personID.length(), COMMAND_REMOVEPERSON);
    }
    else if(inputCommand.find( "removephoto", 0) == 0)
    {
      //removing a photo
      string photoID;
      PROMPT("Please enter the photo's id", photoID);
      sendPacket(false, photoID.c_str(), photoID.length(), COMMAND_REMOVEPHOTO);
    }
    else
    {
      //not a valid command
      cout << "The proper commands are : 'login', 'set password', 'upload photo', 'download photo', 'query photos', 'list people', 'add person', 'remove person', and 'remove photo'." << endl;
    }

    cout << "> " << flush;
    inputCommand = "";
    cin >> inputCommand;
  }
}
