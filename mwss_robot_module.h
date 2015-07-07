/*
* File: mwssrobot_module.h
* Author: m79lol, iskinmike
*
*/
#ifndef VIRTUAL_ROBOT_MODULE_H
#define	VIRTUAL_ROBOT_MODULE_H
class mwssRobot : public Robot {
public:
	bool is_locked;
	bool is_aviable;

	boost::mutex socket_mtx;
	boost::asio::io_service robot_io_service_;
	boost::asio::ip::tcp::socket robot_socket;
	boost::asio::ip::tcp::endpoint robot_endpoint;

	//MotorState *temp;
	std::vector<unsigned char*> *Motors_state_vector;
	std::vector<unsigned char> Motors_state_etalon;

	bool require();
	void free();
	void sendMessage(unsigned char *params);

	std::vector<variable_value> axis_state;
	mwssRobot(boost::asio::ip::tcp::endpoint robot_endpoint) : is_locked(false), is_aviable(true), robot_socket(robot_io_service_){};
	FunctionResult* executeFunction(system_value command_index, void **args);
	void axisControl(system_value axis_index, variable_value value);
	~mwssRobot() {};
};

typedef std::vector<mwssRobot*> m_connections;

class mwssRobotModule : public RobotModule{
#ifdef _WIN32
	CRITICAL_SECTION mwssRM_cs;
#else
	pthread_mutex_t mwssRM_cs;
#endif
	boost::mutex mwssRM_mtx;
	m_connections aviable_connections;
	FunctionData **mwssrobot_functions;
	AxisData **robot_axis;
	colorPrintf_t *colorPrintf;

	int port;
	std::string IP;

	//bool is_aviable;

	boost::asio::ip::tcp::socket *robot_module_socket;

public:
	mwssRobotModule();

	//init
	const char *getUID();
	void prepare(colorPrintf_t *colorPrintf_p, colorPrintfVA_t *colorPrintfVA_p);

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
};
#endif	/* VIRTUAL_ROBOT_MODULE_H */