/*
* File: messages.cpp
* Author: iskinmike
*
*/


#include <string>
#include <time.h>
#include <map>
#include <vector>


#include <boost\asio.hpp>
#include <boost\thread.hpp>
#include <boost\thread\mutex.hpp>

#include "messages.h"

#ifndef _MSC_VER
	#include "stringC11.h"
#endif


// GLOBAL VARIABLES
boost::asio::io_service io_service_;
boost::asio::ip::tcp::socket socket_(io_service_);
boost::system::error_code ec;

boost::mutex g_mtx_;

bool Dispatcher_Thread_Exist = true;


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

std::vector<request> BoxOfMessages;
std::vector<MotorState *> Motors_state_vector;

//Начало переменные для формирования команды
unsigned char   LeftMotorTurrelRelay,  //0 байт инв л.д.б
				RightMotorTurrelRelay,  //1 байт инв п.д.б
				DownMotorTurrelRelay,  //2 байт инв н.д.б
				LeftMotorChassisRelay,  //3 байт инв л.д.ш
				RightMotorChassisRelay,  //4 байт инв п.д.ш
				LeftMotorTurrelPWM, //5 байт шм л.д.б
				RightMotorTurrelPWM, //6 байт шм п.д.б
				DownMotorTurrelPWM, //7 байт шм н.д.б
				LeftMotorChassisPWM, //8 байт шм л.д.ш
				RightMotorChassisPWM, //9 байт шм п.д.ш
				LeftWeaponRelay, //10 байт инв. левого оружия
				RightWeaponRelay, //11 байт инв. правого оружия
				LeftMotorTurrelPWM_scaler = 5, //12 байт шм л.д.б
				RightMotorTurrelPWM_scaler = 5, //13 байт шм п.д.б
				DownMotorTurrelPWM_scaler = 9, //14 байт шм н.д.б
				LeftMotorChassisPWM_scaler = 15, //15 байт шм л.д.ш
				RightMotorChassisPWM_scaler = 15; //16 байт шм п.д.ш

std::vector<MotorState *> *returnMotorState(){
	return &Motors_state_vector;
};

std::vector<unsigned char*> Motor_data_vector;
std::vector<unsigned char> Etalon_data_vector;

std::vector<unsigned char*> *returnMotorData(){
	Motor_data_vector.push_back(&LeftMotorTurrelRelay);
	Motor_data_vector.push_back(&RightMotorTurrelRelay);
	Motor_data_vector.push_back(&DownMotorTurrelRelay);
	Motor_data_vector.push_back(&LeftMotorChassisRelay);
	Motor_data_vector.push_back(&RightMotorChassisRelay);
	Motor_data_vector.push_back(&LeftMotorTurrelPWM);
	Motor_data_vector.push_back(&RightMotorTurrelPWM);
	Motor_data_vector.push_back(&DownMotorTurrelPWM);
	Motor_data_vector.push_back(&LeftMotorChassisPWM);
	Motor_data_vector.push_back(&RightMotorChassisPWM);
	Motor_data_vector.push_back(&LeftWeaponRelay);
	Motor_data_vector.push_back(&RightWeaponRelay);

	return &Motor_data_vector;
};
std::vector<unsigned char> returnEtalonData(){
	Etalon_data_vector.push_back(LeftMotorTurrelRelay);
	Etalon_data_vector.push_back(RightMotorTurrelRelay);
	Etalon_data_vector.push_back(DownMotorTurrelRelay);
	Etalon_data_vector.push_back(LeftMotorChassisRelay);
	Etalon_data_vector.push_back(RightMotorChassisRelay);
	Etalon_data_vector.push_back(LeftMotorTurrelPWM);
	Etalon_data_vector.push_back(RightMotorTurrelPWM);
	Etalon_data_vector.push_back(DownMotorTurrelPWM);
	Etalon_data_vector.push_back(LeftMotorChassisPWM);
	Etalon_data_vector.push_back(RightMotorChassisPWM);
	Etalon_data_vector.push_back(LeftWeaponRelay);
	Etalon_data_vector.push_back(RightWeaponRelay);

	return Etalon_data_vector;
}


std::string createMessage()
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

	std::string a("");
	a.append(std::to_string(0x7E)); // 126
	a.append(std::to_string(LeftMotorTurrelRelay)); //0 байт инв л.д.б
	a.append(std::to_string(RightMotorTurrelRelay)); //1 байт инв п.д.б
	a.append(std::to_string(DownMotorTurrelRelay)); //2 байт инв н.д.б
	a.append(std::to_string(LeftMotorChassisRelay)); //3 байт инв л.д.ш
	a.append(std::to_string(RightMotorChassisRelay)); //4 байт инв п.д.ш
	a.append(std::to_string(LeftMotorTurrelPWM)); //5 байт шм л.д.б
	a.append(std::to_string(RightMotorTurrelPWM)); //6 байт шм п.д.б
	a.append(std::to_string(DownMotorTurrelPWM)); //7 байт шм н.д.б
	a.append(std::to_string(LeftMotorChassisPWM)); //8 байт шм л.д.ш
	a.append(std::to_string(RightMotorChassisPWM)); //9 байт шм п.д.ш
	a.append(std::to_string(LeftWeaponRelay)); //10
	a.append(std::to_string(RightWeaponRelay)); //11
	a.append(std::to_string(LeftMotorTurrelPWM_scaler)); //12
	a.append(std::to_string(RightMotorTurrelPWM_scaler)); //13
	a.append(std::to_string(DownMotorTurrelPWM_scaler)); //14
	a.append(std::to_string(LeftMotorChassisPWM_scaler)); //15
	a.append(std::to_string(RightMotorChassisPWM_scaler)); //16
	a.append(std::to_string(0x7F)); //127

	return a;
}
/// Функция отправки сообщения в сокет
void sendMWSSMessage(std::string params){
	socket_.send(boost::asio::buffer(params));
};


int SleeperThread(request *arg){
	Sleep(arg->time);

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

// Thread Function
int DispatcherOfMotors(){
	std::vector<request> DispatchersBox;

	while (true)
	{
		g_mtx_.lock();
		if (!Dispatcher_Thread_Exist) {
			g_mtx_.unlock();
			return 0;
		} // Close Thread
		for (auto i = BoxOfMessages.begin(); i != BoxOfMessages.end(); i++){
			DispatchersBox.push_back(*i);
		}
		BoxOfMessages.clear();
		g_mtx_.unlock();

		request *temp;
		for (auto i = DispatchersBox.begin(); i != DispatchersBox.end(); i++){
			temp = &(*i);
			g_mtx_.lock();
			//Меняем информацию в массиве Motors_state_vector
			if (temp->next_request != NULL){
				temp->next_request->motor->now_state = temp->next_request->new_speed;
			}
			temp->motor->now_state = temp->new_speed;
			// Нужно отправить сообщение чтобы запускал шарманку.
			sendMWSSMessage(createMessage());
			g_mtx_.unlock();

			//Sleep_Thread = START_THREAD_DEMON(SleeperThread, temp, Sleep_Thread_ID);
			boost::thread Sleep_Th(SleeperThread, temp);
			if (temp->flag){ // if flag == true we wait before Sleep thread done work
				Sleep_Th.join();
			}
		}
		DispatchersBox.clear();
	}// EndWhile
};

// Инициализация нашего сокета
boost::asio::ip::tcp::socket *initConnection(int Port, std::string IP){

	g_mtx_.initialize();

	for (int i = 0; i < 7; i++){
		Motors_state_vector[i] = new MotorState;
		Motors_state_vector[i]->now_state = 0;
		Motors_state_vector[i]->req = NULL;
	}

	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(IP.c_str()), Port);
	socket_.connect(endpoint, ec);
	if (!ec)
	{
		std::cerr << "connected" << std::endl;
	}
	else
	{
		std::cerr << ec.message() << std::endl;
	}

	// Start Thread
	boost::thread DispatcherThread(DispatcherOfMotors);

	return &socket_;
};

// Close Connection
void closeSocketConnection(){

	g_mtx_.lock();
	Dispatcher_Thread_Exist = false;
	g_mtx_.unlock();

	socket_.close();
	g_mtx_.destroy();

	/// Нужна очистка всех выделенных участков памяти
	for (int i = 0; i < 7; i++){
		delete Motors_state_vector[i];
	}

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

	g_mtx_.lock();
	BoxOfMessages.push_back(*left_motor);
	g_mtx_.unlock();

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

	g_mtx_.lock();
	BoxOfMessages.push_back(*turrel_motor);
	g_mtx_.unlock();
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

	g_mtx_.lock();
	BoxOfMessages.push_back(*weapon_motor);
	g_mtx_.unlock();
};