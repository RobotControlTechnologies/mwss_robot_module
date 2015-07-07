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

#include <iostream>

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

/////// Блок объявления сервисных функций и структур
struct MotorState;
struct request;

//////////// Service FUNCTIONS and STRUCTURES
struct request
{
	int new_speed;  // Новая скорость
	int time;	    // Время сколько будет спать поток
	bool flag;      // Флаг возвращения управления
	MotorState *motor; // Указатель на request *req в структуре MOtorState.. Нет, это указательна структуру Motor state.
	request *next_request;
};

//Пусть есть структура для мотора :
struct	MotorState {
	int now_state; // Скорость  speed
	request *req;  // указатель на структуру запроса, изначально NULL
};

unsigned char *createMessage();
void sendMWSSMessage(unsigned char *params);

void moveChassie(int speed_L, int speed_R, int time, bool flag);
void moveTurrel(std::string motor, int speed, int time, bool flag);
void fireWeapon(std::string motor, bool enabled, int time, bool flag);

///////// Global Variables
const unsigned int COUNT_mwssRobot_FUNCTIONS = 3;
const unsigned int COUNT_AXIS = 8;
boost::system::error_code ec;
std::vector<MotorState *> Motors_state_vector;
boost::mutex g_mtx_;

unsigned char   LeftMotorTurrelRelay,
				RightMotorTurrelRelay,
				DownMotorTurrelRelay,
				LeftMotorChassisRelay,
				RightMotorChassisRelay,
				LeftMotorTurrelPWM,
				RightMotorTurrelPWM,
				DownMotorTurrelPWM,
				LeftMotorChassisPWM,
				RightMotorChassisPWM,
				LeftWeaponRelay,
				RightWeaponRelay,
				LeftMotorTurrelPWM_scaler = 5,
				RightMotorTurrelPWM_scaler = 5,
				DownMotorTurrelPWM_scaler = 9,
				LeftMotorChassisPWM_scaler = 15,
				RightMotorChassisPWM_scaler = 15;


//// Threads
int SleeperThread(request *arg){
	g_mtx_.lock();
	if (arg->next_request != NULL){
		arg->next_request->motor->now_state = arg->next_request->new_speed;
	}
	arg->motor->now_state = arg->new_speed;
	g_mtx_.unlock();
	// Sleep specified time
	boost::this_thread::sleep_for(boost::chrono::milliseconds(arg->time));

	if (arg->motor->req == arg) {
		g_mtx_.lock();
		//Меняем информацию в массиве Motors_state_vector
		arg->motor->now_state = 0;
		if (arg->next_request != NULL){
			arg->next_request->motor->now_state = 0;
		}
		// Должны отправит сообщение о том что наш мотор должен остановиться
		sendMWSSMessage(createMessage());
		g_mtx_.unlock();
	}

	delete arg;
	return 0;
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

	system_value axis_id = 0;
	DEFINE_ALL_AXIS
};

void mwssRobotModule::prepare(colorPrintf_t *colorPrintf_p, colorPrintfVA_t *colorPrintfVA_p) {
	colorPrintf = colorPrintf_p;
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

	for (int i = 0; i < 7; i++){
		Motors_state_vector[i] = new MotorState;
		Motors_state_vector[i]->now_state = 0;
		Motors_state_vector[i]->req = NULL;
	}

	CSimpleIniA ini;
#ifdef _WIN32
	InitializeCriticalSection(&mwssRM_cs);
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
	pthread_mutex_init(&mwssRM_cs, NULL);

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
		colorPrintf(this, ConsoleColor(ConsoleColor::red), "Can't load '%s' file!\n", ConfigPath);
		return 1;
	}

	CSimpleIniA::TNamesDepend values;
	CSimpleIniA::TNamesDepend IP_ini;
	ini.GetAllValues("connection", "port", values);
	ini.GetAllValues("connection", "ip", IP_ini);

	CSimpleIniA::TNamesDepend::const_iterator ini_value;

	for (ini_value = values.begin(); ini_value != values.end(); ++ini_value) {
		colorPrintf(this, ConsoleColor(ConsoleColor::white), "Attemp to connect: %s\n", ini_value->pItem);
		port = atoi(ini_value->pItem);
		IP = IP_ini.begin()->pItem;
	}
	
	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(IP.c_str()), port);
	mwssRobot *mwss_robot = new mwssRobot(endpoint);
	aviable_connections.push_back(mwss_robot);

	return 0;
};

//////////////
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
	for (unsigned int j = 0; j < 7; ++j) {
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
				*((*Motors_state_vector)[3]) = 0;
				*((*Motors_state_vector)[4]) = 0;
			}
			else {
				*((*Motors_state_vector)[3]) = 1;
				*((*Motors_state_vector)[4]) = 1;
			}
			*((*Motors_state_vector)[8]) = (int)abs(value);
			*((*Motors_state_vector)[9]) = (int)abs(value);
		}
		break;
	}
	case 3:{ // RotateChassie

		if (!is_locked){
			if (value > 0) {
				*((*Motors_state_vector)[3]) = 1;
				*((*Motors_state_vector)[4]) = 0;
			}
			else if (value = 0){
				*((*Motors_state_vector)[3]) = 0;
				*((*Motors_state_vector)[4]) = 0;
			}
			else {
				*((*Motors_state_vector)[3]) = 0;
				*((*Motors_state_vector)[4]) = 1;
			}
			*((*Motors_state_vector)[8]) = (int)abs(value);
			*((*Motors_state_vector)[9]) = (int)abs(value);
		}
		break;
	}
	case 4:{ // RotateTurrel
		if (!is_locked){

			if (value >= 0) {
				*((*Motors_state_vector)[0]) = 0;
			}
			else {
				*((*Motors_state_vector)[0]) = 1;
			}
			*((*Motors_state_vector)[5]) = (int)abs(value);
		}
		break;
	}
	case 5:{ // RotateLeftWeapone
		if (!is_locked){

			if (value >= 0) {
				*((*Motors_state_vector)[1]) = 0;
			}
			else {
				*((*Motors_state_vector)[1]) = 1;
			}
			*((*Motors_state_vector)[6]) = (int)abs(value);
		}
		break;
	}
	case 6:{ // RotateRightWeapone
		if (!is_locked){

			if (value >= 0) {
				*((*Motors_state_vector)[2]) = 0;
			}
			else {
				*((*Motors_state_vector)[2]) = 1;
			}
			*((*Motors_state_vector)[7]) =(int) abs(value);
		}
		break;
	}
	case 7:{ // FireLeftWeapone
		if (!is_locked){
			*((*Motors_state_vector)[10]) = (value) ? 1 : 0;
		}
		break;
	}
	case 8:{ // FireRightWeapone
		if (!is_locked){
			*((*Motors_state_vector)[11]) = (value) ? 1 : 0;
		}
		break;
	}
	default:
		break;
	}

	bool is_similar_flag = true;

	for (int i = 0; i<12; i++) {
		if (!(*((*Motors_state_vector)[i]) == Motors_state_etalon[i])){
			is_similar_flag = false;
		}
	}

	if (!is_similar_flag) {
		// Делаем отправку сообщений
		sendMWSSMessage(createMessage());
		//Записываем Эталон
		for (int i = 0; i<12; i++) {
			Motors_state_etalon[i] = *((*Motors_state_vector)[i]);
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

			moveChassie((int)*input1, (int)*input2, (int)*input3, (bool)*input4);
			break;
		}
		case 2: { // moveTurrel
			std::string input1((const char *)args[0]);
			variable_value *input2 = (variable_value *)args[1];
			variable_value *input3 = (variable_value *)args[2];
			variable_value *input4 = (variable_value *)args[3];
			moveTurrel(input1, (int)*input2, (int)*input3, (bool)*input4);
			break;
		}
		case 3: { // fireWeapon
			std::string input1((const char *)args[0]);
			variable_value *input2 = (variable_value *)args[1];
			variable_value *input3 = (variable_value *)args[2];
			variable_value *input4 = (variable_value *)args[3];
			fireWeapon( input1, (int)*input2, (int)*input3, (bool)*input4);
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
void moveChassie(int speed_L, int speed_R, int time, bool flag){

	request *left_motor, *right_motor;
	left_motor = new request;
	right_motor = new request;

	left_motor->flag = flag;
	left_motor->new_speed = speed_L;
	left_motor->time = time;
	left_motor->motor = Motors_state_vector[3];
	left_motor->next_request = right_motor;

	right_motor->flag = flag;
	right_motor->new_speed = speed_R;
	right_motor->time = time;
	right_motor->motor = Motors_state_vector[4];
	right_motor->next_request = NULL;

	Motors_state_vector[3]->req = left_motor;
	Motors_state_vector[4]->req = right_motor;

	boost::thread Sleep_Thread(SleeperThread, left_motor);
	if (flag){ // if flag == true we wait before Sleep thread done work
		Sleep_Thread.join();
	}

};

void moveTurrel(std::string motor, int speed, int time, bool flag){
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

	turrel_motor->flag = flag;
	turrel_motor->new_speed = speed;
	turrel_motor->time = time;
	turrel_motor->motor = Motors_state_vector[num_motor];
	turrel_motor->next_request = NULL;

	Motors_state_vector[num_motor]->req = turrel_motor;

	boost::thread Sleep_Thread(SleeperThread, turrel_motor);
	if (flag){ // if flag == true we wait before Sleep thread done work
		Sleep_Thread.join();
	}
};

void fireWeapon(std::string motor, bool enabled, int time, bool flag){
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

	weapon_motor->flag = flag;
	weapon_motor->new_speed = enabled;
	weapon_motor->time = time;
	weapon_motor->motor = Motors_state_vector[num_motor];
	weapon_motor->next_request = NULL;

	Motors_state_vector[num_motor]->req = weapon_motor;

	boost::thread Sleep_Thread(SleeperThread, weapon_motor);
	if (flag){ // if flag == true we wait before Sleep thread done work
		Sleep_Thread.join();
	}
};

unsigned char *createMessage()
{
	for (int i = 0; i < 7; i++){
		switch (i)
		{
		case 0:{ // LEFT TURREL
			int speed = Motors_state_vector[i]->now_state;
			LeftMotorTurrelRelay = (speed >= 0) ? 0 : 1;
			LeftMotorTurrelPWM = abs(speed);
			break;
		}
		case 1:{ // RIGHT TURREL
			int speed = Motors_state_vector[i]->now_state;
			RightMotorTurrelRelay = (speed >= 0) ? 0 : 1;
			RightMotorTurrelPWM = abs(speed);
			break;
		}
		case 2:{ // DOWN TURREL
			int speed = Motors_state_vector[i]->now_state;
			DownMotorTurrelRelay = (speed >= 0) ? 0 : 1;
			DownMotorTurrelPWM = abs(speed);
			break;
		}
		case 3:{ // LEFT CHASSIE
			int speed = Motors_state_vector[i]->now_state;
			LeftMotorChassisRelay = (speed >= 0) ? 0 : 1;
			LeftMotorChassisPWM = abs(speed);
			break;
		}
		case 4:{ // RIGHT CHASSIE
			int speed = Motors_state_vector[i]->now_state;
			RightMotorChassisRelay = (speed >= 0) ? 0 : 1;
			RightMotorChassisPWM = abs(speed);
			break;
		}
		case 5:{ // LEFT WEAPON
			int speed = Motors_state_vector[i]->now_state;
			LeftWeaponRelay = (speed != 0) ? 1 : 0;
			break;
		}
		case 6:{ // LEFT WEAPON
			int speed = Motors_state_vector[i]->now_state;
			RightWeaponRelay = (speed != 0) ? 1 : 0;
			break;
		}
		}
	}

	unsigned char a[19];

	a[0] = 0x7E;
	a[1] = LeftMotorTurrelRelay;
	a[2] = RightMotorTurrelRelay;
	a[3] = DownMotorTurrelRelay;
	a[4] = LeftMotorChassisRelay;
	a[5] = RightMotorChassisRelay;
	a[6] = LeftMotorTurrelPWM;
	a[7] = RightMotorTurrelPWM;
	a[8] = DownMotorTurrelPWM;
	a[9] = LeftMotorChassisPWM;
	a[10] = RightMotorChassisPWM;
	a[11] = LeftWeaponRelay;
	a[12] = RightWeaponRelay;
	a[13] = LeftMotorTurrelPWM_scaler;
	a[14] = RightMotorTurrelPWM_scaler;
	a[15] = DownMotorTurrelPWM_scaler;
	a[16] = LeftMotorChassisPWM_scaler;
	a[17] = RightMotorChassisPWM_scaler;
	a[18] = 0x7F;

	return a;
}

void sendMWSSMessage(unsigned char *params){
	//socket_.send(boost::asio::buffer(params,19));
};

void mwssRobot::sendMessage(unsigned char *params){

	robot_socket.send(boost::asio::buffer(params, 19));

};