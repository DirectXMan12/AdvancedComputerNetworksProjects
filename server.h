void sendPacket(bool isErr, const char* payload, int payload_len, unsigned int command);
int callbackPerson(void *NotUsed, int argc, char **argv, char **azColName);
void insertPeople(char* first_name, char* last_name, char* location);
int callbackInsertPhoto(void *NotUsed, int argc, char **argv, char **azColName);
void insertPhoto(int thisPersonID, int type, unsigned char* BLOB);
int callbackRemovePerson(void *NotUsed, int argc, char **argv, char **azColName);
void removePeople(char* first_name, char* last_name);
void selectPeople();
int callbackLoginAttempt(void *NotUsed, int argc, char **argv, char **azColName);
void loginAttempt(char* username, char* password);
int callbackChangePassword(void *NotUsed, int argc, char **argv, char **azColName);
void changePassword(char* newPassword);

// constants

#define COMMAND_LOGIN 0
#define COMMAND_UPLOADPHOTO 1
#define COMMAND_DOWNLOADPHOTO 2
#define COMMAND_QUERYPHOTOS 3
#define COMMAND_LISTPEOPLE 4
#define COMMAND_ADDPERSON 5
#define COMMAND_REMOVEPERSON 6
#define COMMAND_REMOVEPHOTO 7
#define COMMAND_SETPASSWORD 8
#define COMMAND_UPLOADPART 9
