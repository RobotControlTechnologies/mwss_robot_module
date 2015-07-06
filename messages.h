struct MotorState;
struct request;

std::vector<MotorState *> *returnMotorState();
std::vector<unsigned char*> *returnMotorData();
std::vector<unsigned char> returnEtalonData();

std::string createMessage();
void sendMWSSMessage(std::string params);

boost::asio::ip::tcp::socket *initConnection(int Port, std::string IP);
void closeSocketConnection();

void moveChassie(int speed_L, int speed_R, int time, bool flag);
void moveTurrel(std::string motor, int speed, int time, bool flag);
void fireWeapon(std::string motor, bool enabled, int time, bool flag);