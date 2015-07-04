/*
* File: mwssrobot_module.cpp
* Author: m79lol, iskinmike
*
*/
#ifdef _WIN32
	#define _CRT_SECURE_NO_WARNINGS 
	#define _SCL_SECURE_NO_WARNINGS
#endif

#include <string>
#include <vector>

#ifdef _WIN32
	#include <windows.h>
	#include <stdlib.h> 
#else
	#include <fcntl.h>
	#include <dlfcn.h>
	#include <pthread.h>
#endif

#include "messages.h"
#include "SimpleIni.h"
#include "module.h"
#include "robot_module.h"
#include "mwss_robot_module.h"


#ifdef _WIN32
	EXTERN_C IMAGE_DOS_HEADER __ImageBase;
	// Critical Atom_section
	#define DEFINE_ATOM(ATOM_NAME) CRITICAL_SECTION ATOM_NAME;
	#define ATOM_LOCK(ATOM_NAME) EnterCriticalSection( &ATOM_NAME );
	#define ATOM_UNLOCK(ATOM_NAME) LeaveCriticalSection( &ATOM_NAME );
#else
	// Critical Atom_section
	#define DEFINE_ATOM(ATOM_NAME) pthread_mutex_t ATOM_NAME = PTHREAD_MUTEX_INITIALIZER;
	#define ATOM_LOCK(ATOM_NAME) pthread_mutex_lock( &ATOM_NAME );
	#define ATOM_UNLOCK(ATOM_NAME) pthread_mutex_unlock( &ATOM_NAME );
#endif

/////////
const unsigned int COUNT_mwssRobot_FUNCTIONS = 3;
const unsigned int COUNT_AXIS = 8;

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
	is_aviable = true;
	return 0;
};

//////////  / / / // / / // / / / // // / // / / / Нужно переделать под динамическое подключение
Robot* mwssRobotModule::robotRequire(){
	ATOM_LOCK(mwssRM_cs);
	if (is_aviable && aviable_connections.empty()){
		robot_module_socket = initConnection(port, IP);
		mwssRobot *mwss_robot = new mwssRobot(robot_module_socket);
		mwss_robot->Motors_state_vector = returnMotorData();
		mwss_robot->Motors_state_etalon = returnEtalonData();
		aviable_connections.push_back(mwss_robot);
		Robot *robot = mwss_robot;
		ATOM_UNLOCK(mwssRM_cs);
		is_aviable = false;
		return robot;
	}
	else if (is_aviable && !aviable_connections.empty()) {
		Robot *robot = aviable_connections[0];
		ATOM_UNLOCK(mwssRM_cs);
		robot_module_socket = initConnection(port, IP);
		is_aviable = false;
		return robot;
	}
	ATOM_UNLOCK(mwssRM_cs);
	return NULL;
};

void mwssRobotModule::robotFree(Robot *robot){
	ATOM_LOCK(mwssRM_cs);
	mwssRobot *mwss_robot = reinterpret_cast<mwssRobot*>(robot);
	for (m_connections::iterator i = aviable_connections.begin(); i != aviable_connections.end(); ++i) {
		if (mwss_robot == *i){
			delete (*i);
			aviable_connections.erase(i);
			break;
		};
	}
	is_aviable = true;
	ATOM_UNLOCK(mwssRM_cs);
	closeSocketConnection();
};

void mwssRobotModule::final(){
	aviable_connections.clear();
	closeSocketConnection();
};

void mwssRobotModule::destroy() {
	for (unsigned int j = 0; j < COUNT_mwssRobot_FUNCTIONS; ++j) {
		if (mwssrobot_functions[j]->count_params) {
			delete[] mwssrobot_functions[j]->params;
		}
		delete mwssrobot_functions[j];
	}
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

	// Блок проверки изменилась ли по сравнению с предыдущим состоянием и отправлять либо не отправлять

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
		case 1: { // spawn
			variable_value *input1 = (variable_value *)args[0];
			variable_value *input2 = (variable_value *)args[1];
			variable_value *input3 = (variable_value *)args[2];
			variable_value *input4 = (variable_value *)args[3];

			moveChassie((int)*input1, (int)*input2, (int)*input3, (bool)*input4);
			break;
		}
		case 2: { // move 
			std::string input1((const char *)args[0]);
			variable_value *input2 = (variable_value *)args[1];
			variable_value *input3 = (variable_value *)args[2];
			variable_value *input4 = (variable_value *)args[3];
			moveTurrel(input1, (int)*input2, (int)*input3, (bool)*input4);
			break;
		}
		case 3: { // change Color
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
