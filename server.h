void sendPacket(bool isErr, char* payload, int payload_len, unsigned int command)
static int callbackPerson(void *NotUsed, int argc, char **argv, char **azColName);
void insertPeople(char* first_name, char* last_name, char* location);
static int callbackInsertPhoto(void *NotUsed, int argc, char **argv, char **azColName);
void insertPhoto(int thisPersonID, int type, unsigned char* BLOB);
static int callbackRemovePerson(void *NotUsed, int argc, char **argv, char **azColName);
void removePeople(char* first_name, char* last_name);
void selectPeople();
static int callbackLoginAttempt(void *NotUsed, int argc, char **argv, char **azColName);
void loginAttempt(char* username, char* password);
static int callbackChangePassword(void *NotUsed, int argc, char **argv, char **azColName);
void changePassword(char* newPassword);
