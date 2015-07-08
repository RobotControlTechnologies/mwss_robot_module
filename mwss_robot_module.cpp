/*
* File: mwssrobot_module.cpp
* Author: m79lol, iskinmike
*
*/
#ifdef _WIN32
	#define _CRT_SECURE_NO_WARNINGS 
#endif

#include <string>
#include <vector>

#include <boost\chrono.hpp>
#include <boost\asio.hpp>
#include <boost\thread.hpp>
#include <boost\thread\mutex.hpp>

#ifdef _WIN32
	#include <windows.h>
	#include <stdlib.h> 
#else
	#include <fcntl.h>
	#include <dlfcn.h>
	#include <pthread.h>
#endif

#include "SimpleIni.h"
#include "module.h"
#include "robot_module.h"
#include "mwss_robot_module.h"

#ifdef _WIN32
	EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif

///////// Global Variables
const unsigned int COUNT_mwssRobot_FUNCTIONS = 3;
const unsigned int COUNT_AXIS = 8;
boost::system::error_code ec;

//// Threads
void mwssRobot::robotSleeperThread(request *arg){
	robot_motors_state_mtx.lock();
	if (arg->next_request != NULL){
		arg->next_request->motor->now_state = arg->next_request->new_speed;
	}
	arg->motor->now_state = arg->new_speed;
	// Отправляем сообщение мотору работать
	sendMessage(createMessage());
	robot_motors_state_mtx.unlock();
	// Sleep specified time
	boost::this_thread::sleep_for(boost::chrono::milliseconds(arg->time));
	robot_motors_state_mtx.lock();
	if (arg->motor->req == arg) {
		//Меняем информацию в массиве Motors_state_vector
		arg->motor->now_state = 0;
		if (arg->next_request != NULL){
			arg->next_request->motor->now_state = 0;
		}
		// Должны отправит сообщение о том что наш мотор должен остановиться
		sendMessage(createMessage());
	}
	robot_motors_state_mtx.unlock();
	if (arg->next_request != NULL){
		delete arg->next_request;
	}
	delete arg;
}

//// MACROS
#define ADD_ROBOT_AXIS(AXIS_NAME, UPPER_VALUE, LOWER_VALUE) \
robot_axis[axis_id] = new AxisData; \
robot_axis[axis_id]->axis_index = axis_id + 1; \
robot_axis[axis_id]->upper_value = UPPER_VALUE; \
robot_axis[axis_id]->lower_value = LOWER_VALUE; \
robot_axis[axis_id]->name = AXIS_NAME; \
axis_id++; 

#define DEFINE_ALL_AXIS \
ADD_ROBOT_AXIS("locked", 1, 0)\
ADD_ROBOT_AXIS("MoveChassie", 100, -100)\
ADD_ROBOT_AXIS("RotateChassie", 100, -100)\
ADD_ROBOT_AXIS("RotateTurrel", 100, -100)\
ADD_ROBOT_AXIS("RotateLeftWeapone", 100, -100)\
ADD_ROBOT_AXIS("RotateRightWeapone", 100, -100)\
ADD_ROBOT_AXIS("FireLeftWeapone", 1, 0)\
ADD_ROBOT_AXIS("FireRightWeapone", 1, 0);

mwssRobotModule::mwssRobotModule() {
	mwssrobot_functions = new FunctionData*[COUNT_mwssRobot_FUNCTIONS];
	system_value function_id = 0;

	FunctionData::ParamTypes *Params = new FunctionData::ParamTypes[4];
	Params[0] = FunctionData::ParamTypes::FLOAT;
	Params[1] = FunctionData::ParamTypes::FLOAT;
	Params[2] = FunctionData::ParamTypes::FLOAT;
	Params[3] = FunctionData::ParamTypes::FLOAT;

	mwssrobot_functions[function_id] = new FunctionData(function_id + 1, 4, Params, "moveChassie");
	function_id++;

	Params = new FunctionData::ParamTypes[4];
	Params[0] = FunctionData::ParamTypes::STRING;
	Params[1] = FunctionData::ParamTypes::FLOAT;
	Params[2] = FunctionData::ParamTypes::FLOAT;
	Params[3] = FunctionData::ParamTypes::FLOAT;

	mwssrobot_functions[function_id] = new FunctionData(function_id + 1, 4, Params, "moveTurrel");
	function_id++;

	Params = new FunctionData::ParamTypes[4];
	Params[0] = FunctionData::ParamTypes::STRING;
	Params[1] = FunctionData::ParamTypes::FLOAT;
	Params[2] = FunctionData::ParamTypes::FLOAT;
	Params[3] = FunctionData::ParamTypes::FLOAT;

	mwssrobot_functions[function_id] = new FunctionData(function_id + 1, 4, Params, "fireWeapon");

	robot_axis = new AxisData*[COUNT_AXIS];
	system_value axis_id = 0;
	DEFINE_ALL_AXIS
};

void mwssRobotModule::prepare(colorPrintfModule_t *colorPrintf_p, colorPrintfModuleVA_t *colorPrintfVA_p) {
	this->colorPrintf_p = colorPrintf_p;
}

const char* mwssRobotModule::getUID() {
	return "mwssRobot_functions_dll";
};

FunctionData** mwssRobotModule::getFunctions(unsigned int *count_functions) {
	*count_functions = COUNT_mwssRobot_FUNCTIONS;
	return mwssrobot_functions;
}

int mwssRobotModule::init(){
	mwssRM_mtx.initialize();

	CSimpleIniA ini;
#ifdef _WIN32
	ini.SetMultiKey(true);

	WCHAR DllPath[MAX_PATH] = { 0 };

	GetModuleFileNameW((HINSTANCE)&__ImageBase, DllPath, (DWORD)MAX_PATH);

	WCHAR *tmp = wcsrchr(DllPath, L'\\');
	WCHAR wConfigPath[MAX_PATH] = { 0 };
	size_t path_len = tmp - DllPath;
	wcsncpy(wConfigPath, DllPath, path_len);
	wcscat(wConfigPath, L"\\config.ini");

	char ConfigPath[MAX_PATH] = { 0 };
	wcstombs(ConfigPath, wConfigPath, sizeof(ConfigPath));
#else
	Dl_info PathToSharedObject;
	void * pointer = reinterpret_cast<void*> (getRobotModuleObject);
	dladdr(pointer, &PathToSharedObject);
	std::string dltemp(PathToSharedObject.dli_fname);

	int dlfound = dltemp.find_last_of("/");

	dltemp = dltemp.substr(0, dlfound);
	dltemp += "/config.ini";

	const char* ConfigPath = dltemp.c_str();
#endif
	if (ini.LoadFile(ConfigPath) < 0) {
		colorPrintf(ConsoleColor(ConsoleColor::red), "Can't load '%s' file!\n", ConfigPath);
		return 1;
	}

	CSimpleIniA::TNamesDepend values;
	CSimpleIniA::TNamesDepend IP_ini;
	ini.GetAllValues("connection", "port", values);
	ini.GetAllValues("connection", "ip", IP_ini);

	CSimpleIniA::TNamesDepend::const_iterator ini_value;

	for (ini_value = values.begin(); ini_value != values.end(); ++ini_value) {
		colorPrintf(ConsoleColor(ConsoleColor::white), "Attemp to connect: %s\n", ini_value->pItem);
		port = atoi(ini_value->pItem);
		IP = IP_ini.begin()->pItem;
	}
	
	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(IP.c_str()), port);
	mwssRobot *mwss_robot = new mwssRobot(endpoint);
	aviable_connections.push_back(mwss_robot);

	return 0;
};

Robot* mwssRobotModule::robotRequire(){
	mwssRM_mtx.lock();
	for (auto i = aviable_connections.begin(); i != aviable_connections.end(); ++i) {
		if ((*i)->require()) {
			return (*i);
		}
	}
	mwssRM_mtx.unlock();
	return NULL;
};

bool mwssRobot::require(){
	if (!is_aviable) {
		return false;
	}
	// socket connect
	
	robot_socket.connect(robot_endpoint, ec);
	if (!robot_socket.is_open()){
		return false;
	}
	// set flag busy
	is_aviable = false;
	return true;
};

void mwssRobotModule::robotFree(Robot *robot){
	mwssRobot *mwss_robot = reinterpret_cast<mwssRobot*>(robot);
	mwssRM_mtx.lock();
	for (auto i = aviable_connections.begin(); i != aviable_connections.end(); ++i) {
		if ((*i) == mwss_robot) {
			mwss_robot->free();
			return;
		}
	}
	mwssRM_mtx.unlock();
};

void mwssRobot::free(){
	if (is_aviable) {
		return;
	}
	is_aviable = true;
	robot_socket.close();
};

void mwssRobotModule::final(){
	mwssRM_mtx.lock();
	for (auto i = aviable_connections.begin(); i != aviable_connections.end(); ++i) {
		delete (*i);
	}
	aviable_connections.clear();
	mwssRM_mtx.unlock();
	mwssRM_mtx.destroy();
};

void mwssRobotModule::destroy() {
	for (unsigned int j = 0; j < COUNT_mwssRobot_FUNCTIONS; ++j) {
		if (mwssrobot_functions[j]->count_params) {
			delete[] mwssrobot_functions[j]->params;
		}
		delete mwssrobot_functions[j];
	}
	for (unsigned int j = 0; j < COUNT_AXIS; ++j) {
		delete robot_axis[j];
	}
	delete[] robot_axis;
	delete[] mwssrobot_functions;
	delete this;
};

AxisData **mwssRobotModule::getAxis(unsigned int *count_axis){
	(*count_axis) = COUNT_AXIS;
	return robot_axis;
};

void mwssRobot::axisControl(system_value axis_index, variable_value value){
	switch (axis_index)
	{
	case 1:{ // Locked
		is_locked = !!value;
		break;
	}
	case 2:{ // MoveChassie
		if (!is_locked){
			if (value >= 0) {
				command_for_robot[4] = 0;
				command_for_robot[5] = 0;
			}
			else {
				command_for_robot[4] = 1;
				command_for_robot[5] = 1;
			}
			command_for_robot[9] = (int)abs(value);
			command_for_robot[10] = (int)abs(value);
		}
		break;
	}
	case 3:{ // RotateChassie
		if (!is_locked){
			if (value > 0) {
				command_for_robot[4] = 1;
				command_for_robot[5] = 0;
			}
			else if (value = 0){
				command_for_robot[4] = 0;
				command_for_robot[5] = 0;
			}
			else {
				command_for_robot[4] = 0;
				command_for_robot[5] = 1;
			}
			command_for_robot[9] = (int)abs(value);
			command_for_robot[10] = (int)abs(value);
		}
		break;
	}
	case 4:{ // RotateTurrel
		if (!is_locked){
			if (value >= 0) {
				command_for_robot[3] = 0;
			}
			else {
				command_for_robot[3] = 1;
			}
			command_for_robot[3] = (int)abs(value);
		}
		break;
	}
	case 5:{ // RotateLeftWeapone
		if (!is_locked){
			if (value >= 0) {
				command_for_robot[1] = 0;
			}
			else {
				command_for_robot[1] = 1;
			}
			command_for_robot[1] = (int)abs(value);
		}
		break;
	}
	case 6:{ // RotateRightWeapone
		if (!is_locked){
			if (value >= 0) {
				command_for_robot[2] = 0;
			}
			else {
				command_for_robot[2] = 1;
			}
			command_for_robot[2] = (int)abs(value);
		}
		break;
	}
	case 7:{ // FireLeftWeapone
		if (!is_locked){
			command_for_robot[11] = (value) ? 1 : 0;
		}
		break;
	}
	case 8:{ // FireRightWeapone
		if (!is_locked){
			command_for_robot[12] = (value) ? 1 : 0;
		}
		break;
	}
	default:
		break;
	}

	bool is_similar_flag = true;

	for (int i = 1; i<13; i++) {
		if (!(command_for_robot[i] == command_for_robot_etalon[i])){
			is_similar_flag = false;
			break;
		}
	}

	if (!is_similar_flag) {
		// Делаем отправку сообщений
		sendMessage(createMessage());
		//Записываем Эталон
		for (int i = 1; i<13; i++) {
			command_for_robot_etalon[i] = command_for_robot[i];
		}
	}
};

void *mwssRobotModule::writePC(unsigned int *buffer_length) {
	*buffer_length = 0;
	return NULL;
}

FunctionResult* mwssRobot::executeFunction(system_value functionId, void **args) {
	if ((functionId < 1) || (functionId > COUNT_mwssRobot_FUNCTIONS)) {
		return NULL;
	}
	variable_value rez = 0;
	try {
		switch (functionId) {
		case 1: { // moveChassie
			variable_value *input1 = (variable_value *)args[0];
			variable_value *input2 = (variable_value *)args[1];
			variable_value *input3 = (variable_value *)args[2];
			variable_value *input4 = (variable_value *)args[3];

			request *temp;
			temp = moveChassie((int)*input1, (int)*input2, (int)*input3);
			boost::thread th_1(boost::bind(robot_sleep_thread_function, this, temp));
			if ((bool)*input4){
				th_1.join();
			}
			
			break;
		}
		case 2: { // moveTurrel
			std::string input1((const char *)args[0]);
			variable_value *input2 = (variable_value *)args[1];
			variable_value *input3 = (variable_value *)args[2];
			variable_value *input4 = (variable_value *)args[3];

			request *temp;
			temp = moveTurrel(input1, (int)*input2, (int)*input3);
			boost::thread th_1(boost::bind(robot_sleep_thread_function, this, temp));
			if ((bool)*input4){
				th_1.join();
			}
			break;
		}
		case 3: { // fireWeapon
			std::string input1((const char *)args[0]);
			variable_value *input2 = (variable_value *)args[1];
			variable_value *input3 = (variable_value *)args[2];
			variable_value *input4 = (variable_value *)args[3];

			request *temp;
			temp = fireWeapon(input1, (int)*input2, (int)*input3);
			boost::thread th_1(boost::bind(robot_sleep_thread_function, this, temp));
			if ((bool)*input4){
				th_1.join();
			}
			break;
		}
		};
		return new FunctionResult(1, rez);
	}
	catch (...){
		return new FunctionResult(0);
	};
};

int mwssRobotModule::startProgram(int uniq_index) {
	return 0;
}

void mwssRobotModule::readPC(void *buffer, unsigned int buffer_length) {
}

int mwssRobotModule::endProgram(int uniq_index) {
	return 0;
}

PREFIX_FUNC_DLL RobotModule* getRobotModuleObject() {
	return new mwssRobotModule();
};

/// Функции робота
request* mwssRobot::moveChassie(int speed_L, int speed_R, int time){

	request *left_motor, *right_motor;
	left_motor = new request;
	right_motor = new request;

	left_motor->new_speed = speed_L;
	left_motor->time = time;
	left_motor->motor = Motors_state_vector[3];
	left_motor->next_request = right_motor;

	right_motor->new_speed = speed_R;
	right_motor->motor = Motors_state_vector[4];
	right_motor->next_request = NULL;

	robot_motors_state_mtx.lock();
	Motors_state_vector[3]->req = left_motor;
	robot_motors_state_mtx.unlock();

	return left_motor;
};

request* mwssRobot::moveTurrel(std::string motor, int speed, int time){
	int num_motor;

	switch (*(motor.c_str()))
	{
	case '1':{
		num_motor = 0;
		break;
	}
	case '2':{
		num_motor = 1;
		break;
	}
	case '3':{
		num_motor = 2;
		break;
	}
	default:
		break;
	}

	request *turrel_motor;
	turrel_motor = new request;

	turrel_motor->new_speed = speed;
	turrel_motor->time = time;
	turrel_motor->motor = Motors_state_vector[num_motor];
	turrel_motor->next_request = NULL;

	robot_motors_state_mtx.lock();
	Motors_state_vector[num_motor]->req = turrel_motor;
	robot_motors_state_mtx.unlock();

	return turrel_motor;
};

request* mwssRobot::fireWeapon(std::string motor, bool enabled, int time){
	int num_motor;

	switch (*(motor.c_str()))
	{
	case '1':{
		num_motor = 5;
		break;
	}
	case '2':{
		num_motor = 6;
		break;
	}
	default:
		break;
	}

	request *weapon_motor;
	weapon_motor = new request;

	weapon_motor->new_speed = enabled;
	weapon_motor->time = time;
	weapon_motor->motor = Motors_state_vector[num_motor];
	weapon_motor->next_request = NULL;

	robot_motors_state_mtx.lock();
	Motors_state_vector[num_motor]->req = weapon_motor;
	robot_motors_state_mtx.unlock();

	return weapon_motor;
};

unsigned char* mwssRobot::createMessage()
{
	// Пробегает по Motors_state_vector и в соответствии с ним собирает сообщение
	for (int i = 0; i < 7; i++){
		switch (i)
		{
		case 0:{ // LEFT TURREL
			int speed = Motors_state_vector[i]->now_state;
			command_for_robot[1] = (speed >= 0) ? 0 : 1;
			command_for_robot[6] = abs(speed);
			break;
		}
		case 1:{ // RIGHT TURREL
			int speed = Motors_state_vector[i]->now_state;
			command_for_robot[2] = (speed >= 0) ? 0 : 1;
			command_for_robot[7] = abs(speed);
			break;
		}
		case 2:{ // DOWN TURREL
			int speed = Motors_state_vector[i]->now_state;
			command_for_robot[3] = (speed >= 0) ? 0 : 1;
			command_for_robot[8] = abs(speed);
			break;
		}
		case 3:{ // LEFT CHASSIE
			int speed = Motors_state_vector[i]->now_state;
			command_for_robot[4] = (speed >= 0) ? 0 : 1;
			command_for_robot[9] = abs(speed);
			break;
		}
		case 4:{ // RIGHT CHASSIE
			int speed = Motors_state_vector[i]->now_state;
			command_for_robot[5] = (speed >= 0) ? 0 : 1;
			command_for_robot[10] = abs(speed);
			break;
		}
		case 5:{ // LEFT WEAPON
			int speed = Motors_state_vector[i]->now_state;
			command_for_robot[11] = (speed != 0) ? 1 : 0;
			break;
		}
		case 6:{ // LEFT WEAPON
			int speed = Motors_state_vector[i]->now_state;
			command_for_robot[12] = (speed != 0) ? 1 : 0;
			break;
		}
		}
	}

	return command_for_robot;
}

void mwssRobot::sendMessage(unsigned char *params){
	robot_socket.send(boost::asio::buffer(params, 19));
};

void mwssRobot::prepare(colorPrintfRobot_t *colorPrintf_p, colorPrintfRobotVA_t *colorPrintfVA_p) {
	this->colorPrintf_p = colorPrintfVA_p;
}

void mwssRobot::colorPrintf(ConsoleColor colors, const char *mask, ...) {
	va_list args;
	va_start(args, mask);
	(*colorPrintf_p)(this, uniq_name, colors, mask, args);
	va_end(args);
}

void mwssRobotModule::colorPrintf(ConsoleColor colors, const char *mask, ...) {
	va_list args;
	va_start(args, mask);
	(*colorPrintf_p)(this, colors, mask, args);
	va_end(args);
}

mwssRobot::mwssRobot(boost::asio::ip::tcp::endpoint robot_endpoint) :
is_locked(false),
is_aviable(true),
robot_socket(robot_io_service_),
robot_sleep_thread_function(&mwssRobot::robotSleeperThread)
{
	robot_motors_state_mtx.initialize();
	uniq_name = new char[40];
	sprintf(uniq_name, "robot-%u", 1);

	for (int i = 0; i < 7; i++){ // We have 7 "motors"
		Motors_state_vector[i] = new MotorState;
		Motors_state_vector[i]->now_state = 0;
		Motors_state_vector[i]->req = NULL;
	}

	/// Command for robot massive 
	/// 0 - first byte
	/// 1 - 3 turrel motors Relay   Left Right Down
	/// 4 - 5 chassie Relay         L R 
	/// 6 - 8 turrel motors power   L R D
	/// 9 - 10 chassie motors power L R
	/// 11 - 12 weapone relay       L R
	/// 13 - 15 turrel scalers		L R D  
	/// 16 - 17 chassie scalers		L R
	/// 18 - last byte
	for (int i = 1; i<13; i++){
		command_for_robot[i] = 0;
		command_for_robot_etalon[i] = 0;
	}

	command_for_robot[0] = 0x7E;
	command_for_robot[13] = 5;
	command_for_robot[14] = 5;
	command_for_robot[15] = 9;
	command_for_robot[16] = 15;
	command_for_robot[17] = 15;
	command_for_robot[18] = 0x7F;

	command_for_robot_etalon[0] = 0x7E;
	command_for_robot_etalon[13] = 5;
	command_for_robot_etalon[14] = 5;
	command_for_robot_etalon[15] = 9;
	command_for_robot_etalon[16] = 15;
	command_for_robot_etalon[17] = 15;
	command_for_robot_etalon[18] = 0x7F;
};