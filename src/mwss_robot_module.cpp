/*
* File: MWSSrobot_module.cpp
* Author: m79lol, iskinmike
*
*/
#include "mwss_robot_module.h"

#include <SimpleIni.h>

#ifdef _WIN32
#ifndef _MSC_VER
#include "stringC11.h"
#endif
#endif

#include <string>
#include <vector>
#include <stdarg.h>

#ifndef _WIN32
#include <fcntl.h>
#include <dlfcn.h>
#endif

extern std::string getConfigPath();

///////// Global Variables
#define IID "RCT.Mwss_robot_module_v100"

const unsigned int COUNT_MWSSROBOT_FUNCTIONS = 3;
const unsigned int COUNT_AXIS = 8;

bool isSpeed(int speed) {
  if (abs(speed) > 255) {
    return false;
  } else {
    return true;
  }
}

bool isTime(int time) {
  if (time < 0) {
    return false;
  } else {
    return true;
  }
}

bool isMotor(char motor) {
  switch (motor) {
    case 'L':
    case 'R':
    case 'D': {
      return true;
    }
    default: { return false; }
  }
}

//// Threads
void MWSSRobot::robotSleeperThread(Request *arg) {
  robot_motors_state_mtx.lock();
  if (arg->next_request != NULL) {
    arg->next_request->motor->now_state = arg->next_request->new_speed;
  }
  arg->motor->now_state = arg->new_speed;

  // Отправляем сообщение мотору работать
  sendCommandForRobotWithChangedMotorsState();
  robot_motors_state_mtx.unlock();
  // Sleep specified time
  boost::this_thread::sleep(boost::posix_time::milliseconds(arg->time));

  robot_motors_state_mtx.lock();
  if (arg->motor->req == arg) {
    //Меняем информацию в массиве motors_state_vector
    arg->motor->now_state = 0;
    if (arg->next_request != NULL) {
      arg->next_request->motor->now_state = 0;
    }
    // Должны отправит сообщение о том что наш мотор должен остановиться
    sendCommandForRobotWithChangedMotorsState();
  }
  robot_motors_state_mtx.unlock();
  if (arg->next_request != NULL) {
    delete arg->next_request;
  }
  delete arg;
}

//// MACROS
#define CREATE_THREAD_MACRO(NUM_MOTOR, MOTOR_STATE)                    \
  robot_motors_state_mtx.lock();                                       \
  motors_state_vector[NUM_MOTOR]->req = MOTOR_STATE;                   \
  boost::thread *command_thread = new boost::thread(                   \
      boost::bind(&MWSSRobot::robotSleeperThread, this, MOTOR_STATE)); \
  motors_state_vector[NUM_MOTOR]->thread_pointer = command_thread;     \
  robot_motors_state_mtx.unlock();                                     \
  if (mode != CommandMode::not_wait) {                                 \
    command_thread->join();                                            \
  }

#define ADD_ROBOT_AXIS(AXIS_NAME, UPPER_VALUE, LOWER_VALUE) \
  \
robot_axis[axis_id] = new AxisData;                         \
  \
robot_axis[axis_id]->axis_index = axis_id + 1;              \
  \
robot_axis[axis_id]->upper_value = UPPER_VALUE;             \
  \
robot_axis[axis_id]->lower_value = LOWER_VALUE;             \
  \
robot_axis[axis_id]->name = AXIS_NAME;                      \
  \
axis_id++;

#define DEFINE_ALL_AXIS \
  \
ADD_ROBOT_AXIS("locked", 1, 0) \
ADD_ROBOT_AXIS("MoveChassie", 255, -255) \
ADD_ROBOT_AXIS("RotateChassie", 255, -255) \
ADD_ROBOT_AXIS("RotateTurrel", 10, -10) \
ADD_ROBOT_AXIS("RotateLeftWeapon", 10, -10) \
ADD_ROBOT_AXIS("RotateRightWeapon", 10, -10) \
ADD_ROBOT_AXIS("FireLeftWeapon", 1, 0) \
ADD_ROBOT_AXIS("FireRightWeapon", 1, 0);

MWSSRobotModule::MWSSRobotModule() {
  mi = new ModuleInfo;
  mi->uid = IID;
  mi->mode = ModuleInfo::Modes::PROD;
  mi->version = BUILD_NUMBER;
  mi->digest = NULL;

  mwssrobot_functions = new FunctionData *[COUNT_MWSSROBOT_FUNCTIONS];
  system_value function_id = 0;

  FunctionData::ParamTypes *Params = new FunctionData::ParamTypes[3];
  Params[0] = FunctionData::ParamTypes::FLOAT;
  Params[1] = FunctionData::ParamTypes::FLOAT;
  Params[2] = FunctionData::ParamTypes::FLOAT;

  mwssrobot_functions[function_id] =
      new FunctionData(function_id + 1, 3, Params, "moveChassie");
  function_id++;

  Params = new FunctionData::ParamTypes[3];
  Params[0] = FunctionData::ParamTypes::STRING;
  Params[1] = FunctionData::ParamTypes::FLOAT;
  Params[2] = FunctionData::ParamTypes::FLOAT;

  mwssrobot_functions[function_id] =
      new FunctionData(function_id + 1, 3, Params, "moveTurrel");
  function_id++;

  Params = new FunctionData::ParamTypes[3];
  Params[0] = FunctionData::ParamTypes::STRING;
  Params[1] = FunctionData::ParamTypes::FLOAT;
  Params[2] = FunctionData::ParamTypes::FLOAT;

  mwssrobot_functions[function_id] =
      new FunctionData(function_id + 1, 3, Params, "fireWeapon");

  robot_axis = new AxisData *[COUNT_AXIS];
  system_value axis_id = 0;
  DEFINE_ALL_AXIS
}

void MWSSRobotModule::prepare(colorPrintfModule_t *colorPrintf_p,
                              colorPrintfModuleVA_t *colorPrintfVA_p) {
  this->colorPrintf_p = colorPrintfVA_p;
}

const struct ModuleInfo &MWSSRobotModule::getModuleInfo() { return *mi; }

FunctionData **MWSSRobotModule::getFunctions(unsigned int *count_functions) {
  *count_functions = COUNT_MWSSROBOT_FUNCTIONS;
  return mwssrobot_functions;
}

int MWSSRobotModule::init() {
  std::string ConfigPath = "";
#ifdef _WIN32
  ConfigPath = getConfigPath();
#else
  Dl_info PathToSharedObject;
  void *pointer = reinterpret_cast<void *>(getRobotModuleObject);
  dladdr(pointer, &PathToSharedObject);
  std::string dltemp(PathToSharedObject.dli_fname);

  int dlfound = dltemp.find_last_of("/");

  dltemp = dltemp.substr(0, dlfound);
  dltemp += "/config.ini";

  ConfigPath.append(dltemp.c_str());
#endif

  CSimpleIniA ini;
  ini.SetMultiKey(true);

  if (ini.LoadFile(ConfigPath.c_str()) < 0) {
    colorPrintf(ConsoleColor(ConsoleColor::red), "Can't load '%s' file!\n",
                ConfigPath.c_str());
    return 1;
  }

  int port;
  std::string IP;
  port = ini.GetLongValue("connection", "port", 0);
  if (!port) {
    colorPrintf(ConsoleColor(ConsoleColor::red), "Port is empty\n");
    return 2;
  }
  IP = ini.GetValue("connection", "ip", "");
  if (IP == "") {
    colorPrintf(ConsoleColor(ConsoleColor::red), "IP is empty\n");
    return 2;
  }

  boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string(IP.c_str()), port);
  MWSSRobot *MWSS_robot = new MWSSRobot(endpoint);
  aviable_connections.push_back(MWSS_robot);

  return 0;
}

Robot *MWSSRobotModule::robotRequire() {
  mwssrm_mtx.lock();
  for (auto i = aviable_connections.begin(); i != aviable_connections.end();
       ++i) {
    boost::system::error_code ec = (*i)->connect();
    if (ec) {  // An error occurred.
      colorPrintf(ConsoleColor(ConsoleColor::red),
                  "Can't connect to socket %i\n", ec.value());
      mwssrm_mtx.unlock();
      return NULL;
    }

    if ((*i)->require()) {
      mwssrm_mtx.unlock();
      return (*i);
    }
  }
  mwssrm_mtx.unlock();
  return NULL;
}

boost::system::error_code MWSSRobot::connect() {
  boost::system::error_code ec;
  robot_socket.connect(robot_endpoint, ec);
  return ec;
}

bool MWSSRobot::require() {
  if (!is_aviable) {
    return false;
  }

  if (!robot_socket.is_open()) {
    return false;
  }

  // set flag busy
  is_aviable = false;

  return true;
}

void MWSSRobotModule::robotFree(Robot *robot) {
  MWSSRobot *MWSS_robot = reinterpret_cast<MWSSRobot *>(robot);
  mwssrm_mtx.lock();
  for (auto i = aviable_connections.begin(); i != aviable_connections.end();
       ++i) {
    if ((*i) == MWSS_robot) {
      MWSS_robot->free();
      mwssrm_mtx.unlock();
      return;
    }
  }
  mwssrm_mtx.unlock();
}

void MWSSRobot::free() {
  if (is_aviable) {
    return;
  }
  is_aviable = true;
  robot_socket.close();
}

void MWSSRobotModule::final() {
  mwssrm_mtx.lock();
  for (auto i = aviable_connections.begin(); i != aviable_connections.end();
       ++i) {
    delete (*i);
  }
  aviable_connections.clear();
  mwssrm_mtx.unlock();
}

void MWSSRobotModule::destroy() {
  delete mi;
  for (unsigned int j = 0; j < COUNT_MWSSROBOT_FUNCTIONS; ++j) {
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
}

AxisData **MWSSRobotModule::getAxis(unsigned int *count_axis) {
  (*count_axis) = COUNT_AXIS;
  return robot_axis;
}

void MWSSRobot::axisControl(system_value axis_index, variable_value value) {
  bool need_send = false;

  if (axis_index == 1) {
    if (((is_locked) && (!value)) || ((!is_locked) && (value))) {
      is_locked = !!value;
      need_send = true;
    }
  } else {
    need_send = (!is_locked) && (axis_state[axis_index - 1] != value);
  }

  if (need_send) {
    axis_state[axis_index - 1] = value;
    robot_command_mtx.lock();
    switch (axis_index) {
      case 2: {  // MoveChassie
        if (value >= 0) {
          command_for_robot[4] = 0;
          command_for_robot[5] = 0;
        } else {
          command_for_robot[4] = 1;
          command_for_robot[5] = 1;
        }
        command_for_robot[9] = (int)abs(value);
        command_for_robot[10] = (int)abs(value);
        break;
      }
      case 3: {  // RotateChassie
        if (value > 0) {
          command_for_robot[4] = 1;
          command_for_robot[5] = 0;
        } else if (value == 0) {
          command_for_robot[4] = 0;
          command_for_robot[5] = 0;
        } else {
          command_for_robot[4] = 0;
          command_for_robot[5] = 1;
        }
        command_for_robot[9] = (int)abs(value);
        command_for_robot[10] = (int)abs(value);
        break;
      }
      case 4: {  // RotateTurrel
        if (value >= 0) {
          command_for_robot[3] = 0;
        } else {
          command_for_robot[3] = 1;
        }
        command_for_robot[8] = (int)abs(value);
        break;
      }
      case 5: {  // RotateLeftWeapon
        if (value >= 0) {
          command_for_robot[1] = 0;
        } else {
          command_for_robot[1] = 1;
        }
        command_for_robot[6] = (int)abs(value);
        break;
      }
      case 6: {  // RotateRightWeapon
        if (value >= 0) {
          command_for_robot[2] = 0;
        } else {
          command_for_robot[2] = 1;
        }
        command_for_robot[7] = (int)abs(value);
        break;
      }
      case 7: {  // FireLeftWeapon
        command_for_robot[11] = (value) ? 1 : 0;
        break;
      }
      case 8: {  // FireRightWeapon
        command_for_robot[12] = (value) ? 1 : 0;
        break;
      }
      default:
        break;
    }
    sendCommandForRobot();
    robot_command_mtx.unlock();
  }
}

void *MWSSRobotModule::writePC(unsigned int *buffer_length) {
  *buffer_length = 0;
  return NULL;
}

FunctionResult *MWSSRobot::executeFunction(CommandMode mode,
                                           system_value functionId,
                                           void **args) {
  FunctionResult *fr = NULL;
  try {
    switch (functionId) {
      case ROBOT_COMMAND_FREE: {
        for (int i = 0; i < 7; i++) {  // We have 7 "motors"
          if (motors_state_vector[i]->thread_pointer != NULL) {
            motors_state_vector[i]->thread_pointer->join();
            delete motors_state_vector[i]->thread_pointer;
          }
        }
        break;
      }
      case ROBOT_COMMAND_HAND_CONTROL_BEGIN: {
        robot_motors_state_mtx.lock();
        for (int i = 0; i < 7; i++) {  // We have 7 "motors"
          motors_state_vector[i]->now_state = 0;
          motors_state_vector[i]->req = NULL;
        }
        robot_motors_state_mtx.unlock();
        robot_command_mtx.lock();
        for (int i = 1; i < 13; i++) {
          command_for_robot[i] = 0;
        }
        sendCommandForRobot();
        robot_command_mtx.unlock();
        break;
      }
      case ROBOT_COMMAND_HAND_CONTROL_END: {
        robot_motors_state_mtx.lock();
        for (int i = 0; i < 7; i++) {  // We have 7 "motors"
          motors_state_vector[i]->now_state = 0;
          motors_state_vector[i]->req = NULL;
        }
        robot_motors_state_mtx.unlock();
        robot_command_mtx.lock();
        for (int i = 1; i < 13; i++) {
          command_for_robot[i] = 0;
        }
        sendCommandForRobot();
        robot_command_mtx.unlock();
        break;
      }
      case 1: {  // moveChassie
        variable_value *input1 = (variable_value *)args[0];
        if (!isSpeed(*input1)) {
          throw std::exception();
        }
        variable_value *input2 = (variable_value *)args[1];
        if (!isSpeed(*input2)) {
          throw std::exception();
        }
        variable_value *input3 = (variable_value *)args[2];
        if (!isTime(*input3)) {
          throw std::exception();
        }

        Request *left_motor, *right_motor;
        right_motor =
            new Request((int)*input2, 0, motors_state_vector[4], NULL);
        left_motor = new Request((int)*input1, (int)*input3,
                                 motors_state_vector[3], right_motor);

        CREATE_THREAD_MACRO(3, left_motor);
        break;
      }
      case 2: {  // moveTurrel
        char input1 = *((const char *)args[0]);
        int num_motor;
        switch (input1) {
          case 'L': {
            num_motor = 0;
            break;
          }
          case 'R': {
            num_motor = 1;
            break;
          }
          case 'D': {
            num_motor = 2;
            break;
          }
          default:
            throw std::exception();
        }
        variable_value *input2 = (variable_value *)args[1];
        if (!isSpeed(*input2)) {
          throw std::exception();
        }
        variable_value *input3 = (variable_value *)args[2];
        if (!isTime(*input3)) {
          throw std::exception();
        }

        Request *turrel_motor;
        turrel_motor = new Request((int)*input2, (int)*input3,
                                   motors_state_vector[num_motor], NULL);

        CREATE_THREAD_MACRO(num_motor, turrel_motor);
        break;
      }
      case 3: {  // fireWeapon
        char input1 = *((const char *)args[0]);
        int num_motor;
        switch (input1) {
          case 'L': {
            num_motor = 5;
            break;
          }
          case 'R': {
            num_motor = 6;
            break;
          }
          default:
            throw std::exception();
        }
        variable_value *input2 = (variable_value *)args[1];
        if (!isSpeed(*input2)) {
          throw std::exception();
        }
        variable_value *input3 = (variable_value *)args[2];
        if (!isTime(*input3)) {
          throw std::exception();
        }

        Request *weapon_motor;
        weapon_motor = new Request((bool)*input2, (int)*input3,
                                   motors_state_vector[num_motor], NULL);

        CREATE_THREAD_MACRO(num_motor, weapon_motor);

        break;
      }
    }
    fr = new FunctionResult(FunctionResult::Types::VALUE, 0);
  } catch (...) {
    fr = new FunctionResult(FunctionResult::Types::EXCEPTION);
  }
  return fr;
}

int MWSSRobotModule::startProgram(int uniq_index) { return 0; }

void MWSSRobotModule::readPC(void *buffer, unsigned int buffer_length) {}

int MWSSRobotModule::endProgram(int uniq_index) { return 0; }

PREFIX_FUNC_DLL RobotModule *getRobotModuleObject() {
  return new MWSSRobotModule();
}

void MWSSRobot::sendCommandForRobotWithChangedMotorsState() {
  // Пробегает по motors_state_vector и в соответствии с ним собирает сообщение
  robot_command_mtx.lock();
  for (int i = 0; i < 7; i++) {
    switch (i) {
      case 0: {  // LEFT TURREL
        int speed = motors_state_vector[i]->now_state;
        command_for_robot[1] = (speed >= 0) ? 0 : 1;
        command_for_robot[6] = abs(speed);
        break;
      }
      case 1: {  // RIGHT TURREL
        int speed = motors_state_vector[i]->now_state;
        command_for_robot[2] = (speed >= 0) ? 0 : 1;
        command_for_robot[7] = abs(speed);
        break;
      }
      case 2: {  // DOWN TURREL
        int speed = motors_state_vector[i]->now_state;
        command_for_robot[3] = (speed >= 0) ? 0 : 1;
        command_for_robot[8] = abs(speed);
        break;
      }
      case 3: {  // LEFT CHASSIE
        int speed = motors_state_vector[i]->now_state;
        command_for_robot[4] = (speed >= 0) ? 0 : 1;
        command_for_robot[9] = abs(speed);
        break;
      }
      case 4: {  // RIGHT CHASSIE
        int speed = motors_state_vector[i]->now_state;
        command_for_robot[5] = (speed >= 0) ? 0 : 1;
        command_for_robot[10] = abs(speed);
        break;
      }
      case 5: {  // LEFT WEAPON
        int speed = motors_state_vector[i]->now_state;
        command_for_robot[11] = (speed != 0) ? 1 : 0;
        break;
      }
      case 6: {  // LEFT WEAPON
        int speed = motors_state_vector[i]->now_state;
        command_for_robot[12] = (speed != 0) ? 1 : 0;
        break;
      }
    }
  }
  sendCommandForRobot();
  robot_command_mtx.unlock();
}

void MWSSRobot::sendCommandForRobot() {
  std::string temp_string("");
  for (int i = 0; i < 19; i++) {
    temp_string.append(std::to_string((int)command_for_robot[i]));
    temp_string.append(" ");
  }
  colorPrintf(ConsoleColor(ConsoleColor::green), "send to robot: %s \n",
              temp_string.c_str());
  robot_socket.send(boost::asio::buffer(command_for_robot, 19));
}

void MWSSRobot::prepare(colorPrintfRobot_t *colorPrintf_p,
                        colorPrintfRobotVA_t *colorPrintfVA_p) {
  this->colorPrintf_p = colorPrintfVA_p;
}

void MWSSRobot::colorPrintf(ConsoleColor colors, const char *mask, ...) {
  va_list args;
  va_start(args, mask);
  (*colorPrintf_p)(this, uniq_name, colors, mask, args);
  va_end(args);
}

void MWSSRobotModule::colorPrintf(ConsoleColor colors, const char *mask, ...) {
  va_list args;
  va_start(args, mask);
  (*colorPrintf_p)(this, colors, mask, args);
  va_end(args);
}
// Constructor
MWSSRobot::MWSSRobot(boost::asio::ip::tcp::endpoint robot_endpoint)
    : is_locked(false),
      is_aviable(true),
      robot_socket(robot_io_service_),
      robot_endpoint(robot_endpoint) {
  uniq_name = new char[40];
  sprintf(uniq_name, "robot-%u", 1);
  // Задает начальные позиции осей
  for (unsigned int i = 0; i < COUNT_AXIS; ++i) {
    axis_state.push_back(0);
  }

  for (int i = 0; i < 7; i++) {  // We have 7 "motors"
    motors_state_vector.push_back(new MotorState());
  }

  /// Command for robot massive
  /// 0 - first byte
  /// 1 - 3 turrel motors Relay   Left Right Down
  /// 4 - 5 chassie Relay         L R
  /// 6 - 8 turrel motors power   L R D
  /// 9 - 10 chassie motors power L R
  /// 11 - 12 weapon relay       L R
  /// 13 - 15 turrel scalers		L R D
  /// 16 - 17 chassie scalers		L R
  /// 18 - last byte
  for (int i = 1; i < 13; i++) {
    command_for_robot[i] = 0;
  }
  command_for_robot[0] = 0x7E;
  command_for_robot[13] = 5;
  command_for_robot[14] = 5;
  command_for_robot[15] = 9;
  command_for_robot[16] = 15;
  command_for_robot[17] = 15;
  command_for_robot[18] = 0x7F;
}
MWSSRobot::~MWSSRobot() {
  delete[] uniq_name;
  for (int i = 0; i < 7; i++) {  // We have 7 "motors"
    delete motors_state_vector[i];
  }
}

PREFIX_FUNC_DLL unsigned short getRobotModuleApiVersion() {
  return MODULE_API_VERSION;
}