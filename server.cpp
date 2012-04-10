#include "server.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include "err_macros.h"
#include <sqlite3.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

using namespace std;

sqlite3 *database;
int rc;
bool successfulLogin = false;
char* myUsername;
int personID;

int callbackPerson(void *isListResp, int argc, char **argv, char **azColName)
{
  ostringstream pb;
  pb << "|" << setw(9)  << argv[0] ? argv[0] : "";
  pb << "|" << setw(15) << argv[1] ? argv[1] : "";
  pb << "|" << setw(20) << argv[2] ? argv[2] : "";
  pb << "|" << setw(36) << argv[3] ? argv[3] : "";
  pb << "|";

  sendPacket(false, pb.str().c_str(), pb.str().length(), COMMAND_LISTPEOPLE);
  return 0;
}

void selectPeople()
{
	sqlite3_exec(database, "Select * FROM people", callbackPerson, (void*)1, NULL);
}

int callbackSelectPhotos(void *NotUsed, int argc, char **argv, char **azColName)
{
  ostringstream pb;
  pb << "|" << setw(9)  << argv[0] ? argv[0] : "";
  pb << "|" << setw(40) << argv[1] ? argv[1] : "";
  pb << "|" << setw(20) << argv[2] ? argv[2] : "";
  pb << "|";

  sendPacket(false, pb.str().c_str(), pb.str().length(), COMMAND_QUERYPHOTOS);
  return 0;
}
void selectPhotos()
{
	sqlite3_exec(database, "Select id, type, person_id FROM photos", callbackSelectPhotos, 0, NULL);
}

void insertPeople(char* first_name, char* last_name, char* location)
{
	//check for proper function input
	if(strlen(first_name)>15 || strlen(first_name)<2)
  {
		//invalid first_name
		sendPacket(true,"Invalid First Name: name needs to be between 2 and 15 characters long", 69, COMMAND_ADDPERSON);
		return;
	}
	if(strlen(last_name)>20 || strlen(last_name)<2)
  {
		//invalid last_name
		sendPacket(true,"Invalid Last Name: name needs to be between 2 and 20 characters long", 68, COMMAND_ADDPERSON);
		return;
	}
	if(strlen(location)>36 || strlen(location)<2)
  {
		//invalid location
		sendPacket(true,"Invalid Location: name needs to be between 2 and 36 characters long", 67, COMMAND_ADDPERSON);
		return;
	}
	//input is good
	string strSqlMsg = "INSERT INTO people VALUES(1,'";
	strSqlMsg += first_name;
	strSqlMsg +="', '";
	strSqlMsg += last_name;
	strSqlMsg +="', '";
	strSqlMsg += location;
	strSqlMsg +="')";		
	char* sqlMsg= (char*)strSqlMsg.c_str();
	sqlite3_exec(database, sqlMsg, NULL, 0, NULL);
	sendPacket(false,"Person Successfully Added!", 26, COMMAND_ADDPERSON);
}

void insertPhoto(int thisPersonID, int type, const char* BLOB)
{
  bool validInput = false;

	//check for proper function input
	if(thisPersonID < 0 || thisPersonID > 999999999)
  {
		//invalid id
		sendPacket(true, "Invalid Person ID: not a proper id", 34, COMMAND_UPLOADPHOTO );
    return;
	}
	
  string strSqlMsg = "INSERT INTO photos VALUES(";
  strSqlMsg += type;
  strSqlMsg +=", ";
  strSqlMsg += thisPersonID;
  strSqlMsg +=", ";
  strSqlMsg += string(BLOB);
  strSqlMsg +=")";		
   
  // TODO: check on blob code
  sqlite3_exec(database, strSqlMsg.c_str(), NULL, 0, NULL);
  sendPacket(false, "Photo inserted", strlen("Photo inserted"), COMMAND_UPLOADPHOTO);
}

void removePhoto(int currentPhotoID)
{
	//check for proper function input
	if(currentPhotoID <0 || currentPhotoID >999999999)
  {
		//invalid id
		sendPacket(true,"Invalid Photo ID: not a proper id", 33, COMMAND_REMOVEPHOTO);
		return;
	}
	
	
	//input is good
  string strSqlMsg = "DELETE FROM photos WHERE id =";
  strSqlMsg += currentPhotoID;	
  char* sqlMsg= (char*)strSqlMsg.c_str();
  sqlite3_exec(database, sqlMsg, NULL, 0, NULL);
  sendPacket(false,"Photo has been removed", 22, COMMAND_REMOVEPHOTO);
}

static int callbackDownloadPhoto(void *NotUsed, int argc, char **argv, char **azColName)
{
  // TODO: split across packets
  // TODO: look into blob code
	sendPacket(false, argv[3], sizeof(argv[3]), COMMAND_DOWNLOADPHOTO);
  return 0;
}
void downloadPhoto(int currentPhotoID)
{
	//check for proper function input
	if(currentPhotoID <0 || currentPhotoID >999999999)
  {
		//invalid id
		sendPacket(true, "Invalid Photo ID: not a proper id", 33, COMMAND_DOWNLOADPHOTO);
		return;
	}	
	//input is good
  string strSqlMsg = "SELECT * FROM photos WHERE id =";
  strSqlMsg += currentPhotoID;	

  sqlite3_exec(database, strSqlMsg.c_str(), NULL, 0, NULL);
}

void removePeople(int currentPersonID)
{
  {    
    string strSqlMsg = "DELETE FROM photos WHERE person_id =";
    strSqlMsg += currentPersonID;
    char* sqlMsg= (char*)strSqlMsg.c_str();
    sqlite3_exec(database, sqlMsg, NULL, 0, NULL);
  }
  {
    //remove the person
    string strSqlMsg = "DELETE FROM people WHERE id = ";
    strSqlMsg += currentPersonID;
    char* sqlMsg= (char*)strSqlMsg.c_str();
    sqlite3_exec(database, sqlMsg, callbackPerson, 0, NULL);
  }

  sendPacket(false,"Person removed successfully.",28 , COMMAND_REMOVEPERSON);

}

// select * from person join photo on photo.person_id = person.id where person.first_name = blah1 and person.last_name = blah2;


int callbackLoginAttempt(void *NotUsed, int argc, char **argv, char **azColName)
{
  bool successfulLoginA = false;
	int i;
  for(i=0; i<argc; i++)
  {
    successfulLoginA = true;
  }
  if (successfulLoginA && !successfulLogin)
  {
    sendPacket(false, "Login Successful", 16, COMMAND_LOGIN);
    successfulLogin = true;
  }
  return 0;
}

void loginAttempt(char* username, char* password)
{
	//check for proper function input
	if(strlen(username)>20 || strlen(username)<2)
  {
		sendPacket(true, "Invalid username: name needs to be between 2 and 20 characters long", 67, COMMAND_LOGIN);
    return;
	}
	if(strlen(password)>20 || strlen(password)<2)
  {
		sendPacket(true, "Invalid password: name needs to be between 2 and 20 characters long",67 , COMMAND_LOGIN);
    return;
	}

  string strSqlMsg = "SELECT * FROM login WHERE username='";
  strSqlMsg += username;
  strSqlMsg +="' AND password= '";
  strSqlMsg += password;
  strSqlMsg +="'";	
  char* sqlMsg= (char*)strSqlMsg.c_str();
  myUsername = username;
  sqlite3_exec(database, sqlMsg, callbackLoginAttempt, 0, NULL);
}

int callbackChangePassword(void *NotUsed, int argc, char **argv, char **azColName)
{
  sendPacket(false, "Password Changed", 16, COMMAND_SETPASSWORD);
	return 0;
}

void changePassword(char* newPassword)
{
	//check for proper function input
	if(strlen(newPassword)>20)
  {
		sendPacket(true, "Invalid username: name needs to be between 2 and 20 characters long", 67, COMMAND_SETPASSWORD);
    return;
	}
	if(successfulLogin)
  {
		string strSqlMsg = "UPDATE login SET password = '";
		strSqlMsg += newPassword;
		strSqlMsg +="' WEHRE username ='";
		strSqlMsg += myUsername;
		strSqlMsg +="'";	
		char* sqlMsg= (char*)strSqlMsg.c_str();
		sqlite3_exec(database, sqlMsg, callbackChangePassword, 0, NULL);
	}	
}
