/*
* File: messages.cpp
* Author: iskinmike
*
*/

#include <stdlib.h>
#include <string>
#include <time.h>
#include <map>
#include <vector>
#include <thread>

#include "messages.h"
#include "define_section.h"
#ifndef _MSC_VER
	#include "stringC11.h"
#endif

#include <Windows.h>

// GLOBAL VARIABLES
SOCKET SaR;
DEFINE_ATOM(G_CS_MES);
bool Dispatcher_Thread_Exist = true;
THREAD_HANDLE Dispatcher;

struct request
{
	int new_speed;  // ����� ��������
	int time;	    // ����� ������� ����� ����� �����
	bool flag;      // ���� ����������� ����������
	MotorState *motor; // ��������� �� request *req � ��������� MOtorState.. ���, ��� ����������� ��������� Motor state.
	request *next_request;
};

//����� ���� ��������� ��� ������ :
struct	MotorState {
	int now_state; // ��������  speed
	request *req;  // ��������� �� ��������� �������, ���������� NULL
};

std::vector<request> BoxOfMessages;
std::vector<MotorState *> Motors_state_vector;

//������ ���������� ��� ������������ �������
unsigned char   LeftMotorTurrelRelay,  //0 ���� ��� �.�.�
				RightMotorTurrelRelay,  //1 ���� ��� �.�.�
				DownMotorTurrelRelay,  //2 ���� ��� �.�.�
				LeftMotorChassisRelay,  //3 ���� ��� �.�.�
				RightMotorChassisRelay,  //4 ���� ��� �.�.�
				LeftMotorTurrelPWM, //5 ���� �� �.�.�
				RightMotorTurrelPWM, //6 ���� �� �.�.�
				DownMotorTurrelPWM, //7 ���� �� �.�.�
				LeftMotorChassisPWM, //8 ���� �� �.�.�
				RightMotorChassisPWM, //9 ���� �� �.�.�
				LeftWeaponRelay, //10 ���� ���. ������ ������
				RightWeaponRelay, //11 ���� ���. ������� ������
				LeftMotorTurrelPWM_scaler = 5, //12 ���� �� �.�.�
				RightMotorTurrelPWM_scaler = 5, //13 ���� �� �.�.�
				DownMotorTurrelPWM_scaler = 9, //14 ���� �� �.�.�
				LeftMotorChassisPWM_scaler = 15, //15 ���� �� �.�.�
				RightMotorChassisPWM_scaler = 15; //16 ���� �� �.�.�
//����� ���������� ��� ������������ �������

/*
LeftMotorTurrelRelay = 0;  // 0- ������, 1 - �����
RightMotorTurrelRelay = 0;  // 0- ������, 1 - �����
DownMotorTurrelRelay = 0;  // 0- ������, 1 - �����
LeftMotorChassisRelay = 0;  // 0- ������, 1 - �����
RightMotorChassisRelay = 0;  // 0- ������, 1 - �����
LeftMotorTurrelPWM = 0;  // ��� ��������� ������ ��������
RightMotorTurrelPWM = 0;  // ��� ��������� ������ ��������
DownMotorTurrelPWM = 0;  // ��� ��������� ������ ��������
LeftMotorChassisPWM = 0;  // ��� ��������� ������ ��������
RightMotorChassisPWM = 0;  // ��� ��������� ������ ��������
LeftWeaponRelay = 0;  // 0- ������, 1 - �����
RightWeaponRelay = 0;  // 0- ������, 1 - �����
LeftMotorTurrelPWM_scaler = 5;
RightMotorTurrelPWM_scaler = 5;
DownMotorTurrelPWM_scaler = 9;
LeftMotorChassisPWM_scaler = 15;
RightMotorChassisPWM_scaler = 15;
*/

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
	a.append(std::to_string(LeftMotorTurrelRelay)); //0 ���� ��� �.�.�
	a.append(std::to_string(RightMotorTurrelRelay)); //1 ���� ��� �.�.�
	a.append(std::to_string(DownMotorTurrelRelay)); //2 ���� ��� �.�.�
	a.append(std::to_string(LeftMotorChassisRelay)); //3 ���� ��� �.�.�
	a.append(std::to_string(RightMotorChassisRelay)); //4 ���� ��� �.�.�
	a.append(std::to_string(LeftMotorTurrelPWM)); //5 ���� �� �.�.�
	a.append(std::to_string(RightMotorTurrelPWM)); //6 ���� �� �.�.�
	a.append(std::to_string(DownMotorTurrelPWM)); //7 ���� �� �.�.�
	a.append(std::to_string(LeftMotorChassisPWM)); //8 ���� �� �.�.�
	a.append(std::to_string(RightMotorChassisPWM)); //9 ���� �� �.�.�
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
/// ������� �������� ��������� � �����
void sendMWSSMessage(std::string params){

	send(SaR, params.c_str(), params.length(), 0);
	//return ;
};

/*
DEFINE_THREAD_PROCEDURE(SleeperThread){
	request *temp = ((request *) arg);
	Sleep(temp->time);

	if (temp->motor->req == temp) {

		//������ ���������� � ������� Motors_state_vector
		temp->motor->now_state = 0;
		// ������ �������� ��������� � ��� ��� ��� ����� ������ ������������
		//std::string mes = createMessage();
		ATOM_LOCK(G_CS_MES);
		sendMWSSMessage( createMessage() );
		ATOM_UNLOCK(G_CS_MES);
	}

	delete temp;
	return 0;
}
*/

int SleeperThread(request *arg){
	Sleep(arg->time);

	if (arg->motor->req == arg) {
		ATOM_LOCK(G_CS_MES);
		//������ ���������� � ������� Motors_state_vector
		arg->motor->now_state = 0;
		if (arg->next_request != NULL){
			arg->next_request->motor->now_state = 0;
		}
		// ������ �������� ��������� � ��� ��� ��� ����� ������ ������������
		sendMWSSMessage(createMessage());
		ATOM_UNLOCK(G_CS_MES);
	}

	delete arg;
	return 0;
}

// Thread Function
DEFINE_THREAD_PROCEDURE(DispatcherOfMotors){

	std::vector<request> DispatchersBox;

	while (true)
	{
		ATOM_LOCK(G_CS_MES);
		if (!Dispatcher_Thread_Exist) {
			ATOM_UNLOCK(G_CS_MES);
			return 0;
		} // Close Thread
		for (auto i = BoxOfMessages.begin(); i != BoxOfMessages.end(); i++){
			DispatchersBox.push_back(*i);
		}
		BoxOfMessages.clear();
		ATOM_UNLOCK(G_CS_MES);
/*
		//����� � ������ ������� ��� �����.
		THREAD_HANDLE Sleep_Thread;
		// Start Thread
#ifdef _WIN32
		unsigned int Sleep_Thread_ID;
#else
		pthread_t Sleep_Thread_ID;
#endif	
*/
		request *temp;
		for (auto i = DispatchersBox.begin(); i != DispatchersBox.end(); i++){
			temp = &(*i);
			ATOM_LOCK(G_CS_MES);
			//������ ���������� � ������� Motors_state_vector
			if (temp->next_request != NULL){
				temp->next_request->motor->now_state = temp->next_request->new_speed;
			}
			temp->motor->now_state = temp->new_speed;
			// ����� ��������� ��������� ����� �������� ��������.
			sendMWSSMessage(createMessage());
			ATOM_UNLOCK(G_CS_MES);

			//Sleep_Thread = START_THREAD_DEMON(SleeperThread, temp, Sleep_Thread_ID);
			std::thread Sleep_Th(SleeperThread, temp);
			if (temp->flag){ // if flag == true we wait before Sleep thread done work
				Sleep_Th.join();
			}
		}
		DispatchersBox.clear();

	}// EndWhile
};

// ������������� ������ ������
int initConnection(int Port, std::string IP){

	//MotorState *mot_st = new MotorState;
	//Motors_state_vector.push_back(mot_st);

	for (int i = 0; i < 7; i++){
		Motors_state_vector[i] = new MotorState;
		Motors_state_vector[i]->now_state = 0;
		Motors_state_vector[i]->req = NULL;
	}

#ifdef _WIN32
	InitializeCriticalSection(&G_CS_MES);
	WSADATA w;
	int error = WSAStartup(0x0202, &w);

	if (error) { printf("ERROR WSAStartup: %lu", GetLastError()); };

	if (w.wVersion != 0x0202)
	{
		WSACleanup();
		printf("ERROR Wrong Version of WSADATA: %lu", GetLastError());
	}

	sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(Port);
	addr.sin_addr.S_un.S_addr = inet_addr(IP.c_str());
#else
	sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(Port);
	addr.sin_addr.s_addr = inet_addr(IP.c_str());
#endif

	SaR = socket(PF_INET, SOCK_STREAM, 0);

	SOCKET_NON_BLOCK(SaR, "Try to make socket nonblocking: %d");

	if (connect(SaR, (SOCKADDR *)&addr, sizeof(addr)) != 0) {
		//printf("ERROR can't connect: %d", GetLastError());
	};

	// Start Thread
#ifdef _WIN32
	unsigned int unThreadID;
#else
	pthread_t unThreadID;
#endif	
	Dispatcher = START_THREAD_DEMON(DispatcherOfMotors, NULL, unThreadID);
	return SaR;
};

// Close Connection
void closeSocketConnection(){

	ATOM_LOCK(G_CS_MES);
	Dispatcher_Thread_Exist = false;
	ATOM_UNLOCK(G_CS_MES);

	SOCKET_CLOSE(SaR, "Can't close socket: %d");
	DESTROY_ATOM(G_CS_MES);

	/// ����� ������� ���� ���������� �������� ������
	for (int i = 0; i < 7; i++){
		delete Motors_state_vector[i];
	}

#ifdef _WIN32
	WSACleanup();
#endif
};

/// ������� ������

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

	ATOM_LOCK(G_CS_MES);
	BoxOfMessages.push_back(*left_motor);
	//BoxOfMessages.push_back(*right_motor);
	ATOM_UNLOCK(G_CS_MES);

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

	ATOM_LOCK(G_CS_MES);
	BoxOfMessages.push_back(*turrel_motor);
	ATOM_UNLOCK(G_CS_MES);
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

	ATOM_LOCK(G_CS_MES);
	BoxOfMessages.push_back(*weapon_motor);
	ATOM_UNLOCK(G_CS_MES);
};