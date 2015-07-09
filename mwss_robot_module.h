/*
* File: mwssrobot_module.h
* Author: m79lol, iskinmike
*
*/
#ifndef VIRTUAL_ROBOT_MODULE_H
#define	VIRTUAL_ROBOT_MODULE_H

//////////// Service FUNCTIONS and STRUCTURES
struct MotorState;
struct request;

struct request
{
	int new_speed;  // Новая скорость
	int time;	    // Время сколько будет спать поток
	MotorState *motor; // Указатель на request *req в структуре MOtorState.. Нет, это указательна структуру Motor state.
	request *next_request;

	request(int new_speed, int time, MotorState *motor, request *next_request):
		new_speed(new_speed), time(time), motor(motor), next_request(next_request) {};
	request() :new_speed(0), time(0), motor(NULL), next_request(NULL){};
};

struct	MotorState {
	int now_state; // Скорость  speed
	request *req;  // указатель на структуру запроса, изначально NULL
	MotorState():now_state(0),req(NULL) {};
};

class mwssRobot : public Robot {
	unsigned char *createMessage();
	void sendMessage(unsigned char *params);

	void robotSleeperThread(request *arg);
	void createSleeperThread();

	char *uniq_name;
	colorPrintfRobotVA_t *colorPrintf_p;

	bool is_locked;
	bool is_aviable;

	boost::mutex robot_motors_state_mtx;
	boost::asio::io_service robot_io_service_;
	boost::asio::ip::tcp::socket robot_socket;
	boost::asio::ip::tcp::endpoint robot_endpoint;
	boost::function<void(mwssRobot*, request*)> robot_sleep_thread_function;

	std::vector<MotorState *> Motors_state_vector;

	std::vector<variable_value> axis_state; // Позиции осей запоминаем
	unsigned char command_for_robot[19]; // массив текущих значений

public:

	bool require();
	void free();

	// Constructor
	mwssRobot(boost::asio::ip::tcp::endpoint robot_endpoint);
	void prepare(colorPrintfRobot_t *colorPrintf_p, colorPrintfRobotVA_t *colorPrintfVA_p);
	FunctionResult* executeFunction(system_value command_index, void **args);
	void axisControl(system_value axis_index, variable_value value);
	~mwssRobot() { robot_motors_state_mtx.destroy(); };

	void colorPrintf(ConsoleColor colors, const char *mask, ...);
};

typedef std::vector<mwssRobot*> m_connections;

class mwssRobotModule : public RobotModule{
	boost::mutex mwssRM_mtx;
	m_connections aviable_connections;
	FunctionData **mwssrobot_functions;
	AxisData **robot_axis;
	colorPrintfModule_t *colorPrintf_p;

public:
	mwssRobotModule();

	//init
	const char *getUID();
	void prepare(colorPrintfModule_t *colorPrintf_p, colorPrintfModuleVA_t *colorPrintfVA_p);

	//compiler only
	FunctionData** getFunctions(unsigned int *count_functions);
	AxisData** getAxis(unsigned int *count_axis);
	void *writePC(unsigned int *buffer_length);

	//intepreter - devices
	int init();
	Robot* robotRequire();
	void robotFree(Robot *robot);
	void final();

	//intepreter - program & lib
	void readPC(void *buffer, unsigned int buffer_length);

	//intepreter - program
	int startProgram(int uniq_index);
	int endProgram(int uniq_index);

	//destructor
	void destroy();
	~mwssRobotModule(){};

	void colorPrintf(ConsoleColor colors, const char *mask, ...);
};
#endif	/* VIRTUAL_ROBOT_MODULE_H */