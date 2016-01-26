/*
* File: mwssrobot_module.h
* Author: m79lol, iskinmike
*
*/
#ifndef MWSS_ROBOT_MODULE_H
#define MWSS_ROBOT_MODULE_H

#define ROBOT_COMMAND_FREE 0
#define ROBOT_COMMAND_HAND_CONTROL_BEGIN -1
#define ROBOT_COMMAND_HAND_CONTROL_END -2

//////////// Service FUNCTIONS and STRUCTURES
struct MotorState;
struct Request;

struct Request {
  int new_speed;      // Новая скорость
  int time;           // Время сколько будет спать поток
  MotorState *motor;  // Указатель на Request *req в структуре MOtorState.. Нет,
                      // это указательна структуру Motor state.
  Request *next_request;

  Request(int new_speed, int time, MotorState *motor, Request *next_request)
      : new_speed(new_speed),
        time(time),
        motor(motor),
        next_request(next_request){};
  Request() : new_speed(0), time(0), motor(NULL), next_request(NULL){};
};

struct MotorState {
  int now_state;  // Скорость  speed
  Request *req;   // указатель на структуру запроса, изначально NULL
  boost::thread *thread_pointer;  // Указатель на поток связанный с мотором
  MotorState() : now_state(0), req(NULL), thread_pointer(NULL){};
};

class MWSSRobot : public Robot {
  void sendCommandForRobot();
  void sendCommandForRobotWithChangedMotorsState();

  void robotSleeperThread(Request *arg);

  char *uniq_name;
  colorPrintfRobotVA_t *colorPrintf_p;

  bool is_locked;
  bool is_aviable;

  boost::mutex robot_motors_state_mtx;
  boost::mutex robot_command_mtx;
  boost::asio::io_service robot_io_service_;
  boost::asio::ip::tcp::socket robot_socket;
  boost::asio::ip::tcp::endpoint robot_endpoint;

  std::vector<boost::thread *> robot_thread_vector;
  std::vector<MotorState *> motors_state_vector;

  std::vector<variable_value> axis_state;  // Позиции осей запоминаем
  unsigned char command_for_robot[19];     // массив текущих значений

 public:
  boost::system::error_code connect();

  bool require();
  void free();

  // Constructor
  MWSSRobot(boost::asio::ip::tcp::endpoint robot_endpoint);
  void prepare(colorPrintfRobot_t *colorPrintf_p,
               colorPrintfRobotVA_t *colorPrintfVA_p);
  FunctionResult *executeFunction(CommandMode mode, system_value command_index,
                                  void **args);
  void axisControl(system_value axis_index, variable_value value);
  ~MWSSRobot();

  void colorPrintf(ConsoleColor colors, const char *mask, ...);
};

typedef std::vector<MWSSRobot *> m_connections;

class MWSSRobotModule : public RobotModule {
  boost::mutex mwssrm_mtx;
  m_connections aviable_connections;
  FunctionData **mwssrobot_functions;
  AxisData **robot_axis;
  colorPrintfModuleVA_t *colorPrintf_p;

#ifndef ROBOT_MODULE_H_000
  ModuleInfo *mi;
#endif

 public:
  MWSSRobotModule();

  // init
#ifdef ROBOT_MODULE_H_000
  const char *getUID();
#else
  const struct ModuleInfo &getModuleInfo();
#endif
  void prepare(colorPrintfModule_t *colorPrintf_p,
               colorPrintfModuleVA_t *colorPrintfVA_p);

  // compiler only
  FunctionData **getFunctions(unsigned int *count_functions);
  AxisData **getAxis(unsigned int *count_axis);
  void *writePC(unsigned int *buffer_length);

  // intepreter - devices
  int init();
  Robot *robotRequire();
  void robotFree(Robot *robot);
  void final();

  // intepreter - program & lib
  void readPC(void *buffer, unsigned int buffer_length);

  // intepreter - program
  int startProgram(int uniq_index);
  int endProgram(int uniq_index);

  // destructor
  void destroy();
  ~MWSSRobotModule(){};

  void colorPrintf(ConsoleColor colors, const char *mask, ...);
};
#endif /* MWSS_ROBOT_MODULE_H */