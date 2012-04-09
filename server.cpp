#include "server.h"

static int callbackPerson(void *NotUsed, int argc, char **argv, char **azColName)
{
  int i;
  for(i=0; i<argc; i++)
  {
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return 0;
}
void insertPeople(char* first_name, char* last_name, char* location)
{
	bool validInput =true;
	//check for proper function input
	if(strlen(first_name)>15 || strlen(first_name)<2)
  {
		//invalid first_name
		POST_ERR("APPLICATION_LAYER: Invalid First Name: name needs to be between 2 and 15 characters long");
		validInput=false;
	}
	if(strlen(last_name)>20 || strlen(last_name)<2)
  {
		//invalid last_name
		POST_ERR("APPLICATION_LAYER: Invalid Last Name: name needs to be between 2 and 20 characters long");
		validInput=false;
	}
	if(strlen(location)>36 || strlen(location)<2)
  {
		//invalid location
		POST_ERR("APPLICATION_LAYER: Invalid Location: name needs to be between 2 and 36 characters long");
		validInput = false;
	}
	//input is good
	if(validInput)
  {
		string strSqlMsg = "INSERT INTO people VALUES(1,'";
		strSqlMsg += first_name;
		strSqlMsg +="', '";
		strSqlMsg += last_name;
		strSqlMsg +="', '";
		strSqlMsg += location;
		strSqlMsg +="')";		
		char* sqlMsg= (char*)strSqlMsg.c_str();
		sqlite3_exec(database, sqlMsg, callbackPerson, 0, &ErrMsg);
		//person has been updated need to add login information
	}
}

static int callbackInsertPhoto(void *NotUsed, int argc, char **argv, char **azColName)
{
	int i;
  for(i=0; i<argc; i++)
  {
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return 0;
}

void insertPhoto(int thisPersonID, int type, unsigned char* BLOB)
{
	//check for proper function input
	if(thisPersonID <0 || thisPersonID >999999999)
  {
		//invalid id
		printf("Invalid Person ID: not a proper id");
		validInput=false;
	}
	
	//input is good
	if(validInput)
  {
		string strSqlMsg = "INSERT INTO photos VALUES(";
		strSqlMsg += type;
		strSqlMsg +=", ";
		strSqlMsg += thisPersonID;
		strSqlMsg +=", ";
		strSqlMsg += BLOB;
		strSqlMsg +=")";		
		char* sqlMsg= (char*)strSqlMsg.c_str();
		sqlite3_exec(database, sqlMsg, callbacInsertPhoto, 0, &ErrMsg);
		
		// select * from person where person.first_name = blah and person.last_name = blah limit 1;
    // insert into photos (blahblahblah)
	}
}



static int callbackRemovePerson(void *NotUsed, int argc, char **argv, char **azColName)
{
	int i;
  for(i=0; i<argc; i++)
  {
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
  return 0;
}

void removePeople(char* first_name, char* last_name)
{
	bool validInput =true;
	//check for proper function input
	if(strlen(first_name)>15 || strlen(first_name)<2)
  {
		//invalid first_name
		POST_ERR("APPLICATION_LAYER: Invalid First Name: name needs to be between 2 and 15 characters long");
		validInput = false;
	}

	if(strlen(last_name)>20 || strlen(last_name)<2)
  {
		//invalid last_name
		POST_ERR("APPLICATION_LAYER: Invalid Last Name: name needs to be between 2 and 20 characters long");
		validInput = false;
	}

	//input is good
	if(validInput)
  {
		string strSqlMsg = "INSERT INTO people VALUES(1,'";
		strSqlMsg += first_name;
		strSqlMsg +="', '";
		strSqlMsg += last_name;
		strSqlMsg +="', '";
		strSqlMsg += location;
		strSqlMsg +="')";		
		char* sqlMsg = (char*)strSqlMsg.c_str();
		sqlite3_exec(database, sqlMsg, callbackPerson, 0, &ErrMsg);
	}
}

// select * from person join photo on photo.person_id = person.id where person.first_name = blah1 and person.last_name = blah2;


void selectPeople()
{
	sqlite3_exec(database, "Select * FROM people", callbackPerson, 0, &ErrMsg);
}

static int callbackLoginAttempt(void *NotUsed, int argc, char **argv, char **azColName)
{
	int i;
  for(i=0; i<argc; i++)
  {
    successfulLoginAttempt = true;
  }
  return 0;
}

void loginAttempt(char* username, char* password)
{
	bool validInput =true;
	//check for proper function input
	if(strlen(username)>20 || strlen(username)<2)
  {
		//invalid first_name
		POST_ERR("APPLICATION_LAYER: Invalid username: name needs to be between 2 and 20 characters long");
		validInput=false;
	}
	if(strlen(password)>20 || strlen(password)<2)
  {
		//invalid password
		POST_ERR("APPLICATION_LAYER: Invalid password: name needs to be between 2 and 20 characters long");
		validInput=false;
	}
	if(validInput)
  {
		string strSqlMsg = "SELECT * FROM login WHERE username='";
		strSqlMsg += username;
		strSqlMsg +="' AND password= '";
		strSqlMsg += password;
		strSqlMsg +="'";	
		char* sqlMsg= (char*)strSqlMsg.c_str();
		myUsername = username;
		sqlite3_exec(database, sqlMsg, callbackLoginAttempt, 0, &ErrMsg);
	}	
}

static int callbackChangePassword(void *NotUsed, int argc, char **argv, char **azColName)
{
  int i;
  for(i=0; i<argc; i++)
  {
    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  printf("\n");
	return 0;
}

void changePassword(char* newPassword)
{
	bool validInput =true;
	//check for proper function input
	if(strlen(newPassword)>20 || strlen(username)<2)
  {
		//invalid first_name
		POST_ERR("APPLICATION_LAYER: Invalid username: name needs to be between 2 and 20 characters long");
		validInput=false;
	}
	if(validInput && successfulLogin)
  {
		string strSqlMsg = "UPDATE login SET password = '";
		strSqlMsg += newpassword;
		strSqlMsg +="' WEHRE username ='";
		strSqlMsg += myUsername;
		strSqlMsg +="'";	
		char* sqlMsg= (char*)strSqlMsg.c_str();
		sqlite3_exec(database, sqlMsg, callbackChangePassword, 0, &ErrMsg);
	}	
}
