#include "FIFO.h"
// #include "line_robot.h"
#include "ColorSensor.h"
#include <SharpIR.h>
//#include <Servo.h>
#include <SoftwareServo.h>

//===============================================================================
//===============================Variables=======================================
//===============================================================================
// Logic states
#define StartZone 0x0
#define TunelZone 0x1 // Если 44 в High
#define CylinderZone 0x2
#define MountZone 0x3 // Если 44 в Low
#define EndZone 0x4

#define pinCurrState 44
uint8_t currentIndexState;
uint8_t StatesZone[5];
int MainColorR1;
int CountCheckHandicap = 0;

// Логика тонеля
bool TunelZoneStart = false;

// Логика горки
bool MountZoneStart = false;

// Логика цилиндров
bool CylinderDetected = false;
int countCylinders = 0;

uint8_t CountCylinderColors[6] = {0, 0, 0, 0, 0, 0}; // Черный, белый, красный, желиый, зеленый, синий
// Иницилизация движения
#define IN1 5
#define IN2 4
#define IN3 6
#define IN4 7

int Speed = 150;
unsigned char countSensor = 2;
unsigned char PinsSensorLine[] = {A1, A2};
// AnalogLineRobot line_robot(PinsSensorLine, countSensor);
short int sensorValues[2];

short int lineFl = 0, countLine = 0;

double Kp = 0.15;
double Kd = 0.7;
int prev_error = 0;
int last_move;

// Передвижение по датчикам линий
double Kp_IR = 3;
double Kd_IR = 1;
int prev_error_IR = 0;
int last_move_IR;
int IdealDistance = 0;
// Датчик цвета
ColorSensor sensor(11, 12, 13); // OUT, S2, S3

// Датчики растояния
#define LimitDistanseRight 12
#define TopDistSensor A3
#define RightDistSensor A4
int distanseTop = 0;
int distanseRight = 0;
int distanseTopAnalog = 0;
int distanseRightAnalog = 0;
#define model 1080
SharpIR Sen_ir_1(TopDistSensor, model);
SharpIR Sen_ir_2(RightDistSensor, model);

// Серво
//Servo servo;
SoftwareServo servo1;

// Светодиоды
#define RedLED 36
#define GreenLED 40
#define BlueLED 38

//========================Передача данных=======================================

// Специальные операции
#define ILLEGAL_FUNCTION 0x01     // Код функции не поддерживается
#define SLAVE_DEVICE_FAILURE 0x02 // Произошла ошибка при выполнении операции
#define ILLEGAL_DATA_VALUE 0x03   // Данные неккоректны(CRC)
#define RECEIVED_DATA 0x04        // Данные получены
#define TEST_CONNECT 0x05         // Тест соединения 
#define START_OPERATIONS 0x06     // Начало работы второго робота
#define RED_ON 0x07               // Зажечь красный светодиод
#define GREEN_ON 0x08             // Зажечь зеленый светодиод
#define LED_OFF 0x09              // Выключить светодиод
#define REQUEST_LOAD_CARGO 0x0A   // Запрос погрузки
#define CONFIRM_LOAD_CARGO 0x0B   // Подтвержение начала погрузки
#define END_LOAD_CARGO 0x0C       // Завершение погрузки
#define CYLINDER_COLORS 0x0D      // Передача цветов целиндров
#define ROBOT2_CONFIRM_SYNC 0x0E  // Робот 2 готов к движениею за роботом 1

FIFO BufferInput;
unsigned long lastReceive = millis();
int sizePackage = 0;

// Methods
void ReceiveData(); // Принятие данных
void SendData(uint8_t operation, uint8_t* data, int data_len = 0);  // Передача данных
uint8_t crc8(const uint8_t *addr, uint8_t len); // Расчет CRC8

//===============================================================================



//===============================================================================
//===============================Main=======================================
//===============================================================================
void setup() {
  // put your setup code here, to run once
  Serial.begin(9600); // Для дебага
  Serial2.begin(115200); // Для bluetooth
  InitPins();
  ReadCurrentState();
  Speed = 70;

  // pinMode(47, OUTPUT);
  // digitalWrite(47, HIGH);
  servo1.attach(2);
}

void loop() {
  //TestMethods();
  //  delay(1000);
  //  return;
  Move();
  // MainLogic();
}

// Methods
void InitPins()
{
  //servo.attach(9);
  set_pin_leds();
  set_pin_line_robot();
}


// ======================Основная логика робота==============================
void MainLogic()
{
  Read_sensor();
  ReadIrSensor();

  switch (StatesZone[currentIndexState])
  {
    case StartZone:
      HandleStartZone();
      break;

    case TunelZone:
      HandleTunelZone();
      break;

    case MountZone:
      HandleMountZone();
      break;

    case CylinderZone:
      HandleCylinderZone();
      break;

    case EndZone:
      HandleEndZone();
      break;
  }
}

void SetNewState()
{
  CountCheckHandicap = 0;
  currentIndexState = currentIndexState + 1;

  if (StatesZone[currentIndexState] == TunelZone)
  {
    currentIndexState++;
    Serial.println(StatesZone[currentIndexState]);
  }
}


void HandleStartZone()
{
  if (CheckRightHandicap())
  {
    if (CountCheckHandicap == 5)
    {
      MainColorR1 = sensor.Recognize();
      // HandleColorSensor();
      SetNewState();
      Speed = 150;
    }
    CountCheckHandicap++;
  }
  else
  {
    CountCheckHandicap = 0;
  }
  Move();
}

void HandleTunelZone()
{
  Speed = 100;
  if (CheckTunel())
  {
    // Serial.println("Tunel");
    if (CountCheckHandicap == 5 || TunelZoneStart == true)
    {
      if (TunelZoneStart == false)
      {
        TunelZoneStart = true;
        IdealDistance = distanseRight;
        Red_Set();
      }
      TunelMove();
    }
    CountCheckHandicap++;
  }
  else
  {
    // Serial.println("Not");
    CountCheckHandicap = 0;
    Move();

    if (TunelZoneStart == true)
    {
      TunelZoneStart = false;
      Led_Off();
    }
  }

}

void HandleMountZone()
{
  if (CheckRightHandicap())
  {
    if (CountCheckHandicap == 5)
    {
      if (MountZoneStart == false)
      {
        PushButton();
        MountZoneStart = true;
      }
      else
      {
        SetNewState();
      }
    }
    CountCheckHandicap++;
  }
  else
  {
    CountCheckHandicap = 0;
  }
}

void HandleCylinderZone()
{
  if (CheckRightHandicap())
  {
    if (CylinderDetected == false)
    {
      if (CountCheckHandicap == 5)
      {
        CylinderDetected = true;
        set_motors(0, 0);
        CountCheckHandicap = 0;
        delay(1000);
        countCylinders++;

        int color = sensor.Recognize();
        switch ( color )
        {
          case sensor.Black:
            CountCylinderColors[0] = CountCylinderColors[0] + 1;
            break;
          case sensor.White:
            CountCylinderColors[0] = CountCylinderColors[1] + 1;
            break;
          case sensor.Red:
            CountCylinderColors[0] = CountCylinderColors[2] + 1;
            break;
          case sensor.Yellow:
            CountCylinderColors[0] = CountCylinderColors[3] + 1;
            break;
          case sensor.Green:
            CountCylinderColors[0] = CountCylinderColors[4] + 1;
            break;
          case sensor.Blue:
            CountCylinderColors[0] = CountCylinderColors[5] + 1;
            break;
          default:
            break;
        }

        if (color == MainColorR1)
        {
          // Начать погрузку

        }
        else
        {
          // Пнуть
          ShiftCylinder();
        }

      }
      CountCheckHandicap++;
    }
    else
    {
      CountCheckHandicap = 0;
    }
  }
  else
  {
    if (CylinderDetected == true)
    {
      if (CountCheckHandicap == 5)
      {
        CylinderDetected = false;
      }
      CountCheckHandicap++;
    }
    else
    {
      CountCheckHandicap = 0;
    }
  }
  Move();
  if (countCylinders == 6)
  {
    SetNewState();
  }
}

void HandleEndZone()
{
  Move();
}

void ReadCurrentState()
{
  pinMode(pinCurrState, INPUT);

  StatesZone[0] = StartZone;
  StatesZone[2] = CylinderZone;
  StatesZone[4] = EndZone;

  if (digitalRead(pinCurrState))
  {
    StatesZone[1] = TunelZone;
    StatesZone[3] = MountZone;
  }
  else
  {
    StatesZone[1] = MountZone;
    StatesZone[3] = TunelZone;
  }

  /*Serial.println(StatesZone[0]);
    Serial.println(StatesZone[1]);
    Serial.println(StatesZone[2]);
    Serial.println(StatesZone[3]);*/
}

bool CheckRightHandicap()
{
  if (distanseRight < LimitDistanseRight)
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool CheckTunel()
{
  if (CheckRightHandicap() && sensorValues[0] < 120 && sensorValues[1] < 120)
  {
    return true;
  }
  else
  {
    return false;
  }
}

//========================Передвижение=======================================
void set_pin_line_robot()
{
  for (unsigned int i = 0; i < sizeof(PinsSensorLine); i++)
    pinMode(PinsSensorLine[i], INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
}

void Read_sensor()
{
  for (unsigned int i = 0; i < sizeof(PinsSensorLine); i++)
    sensorValues[i] = analogRead(PinsSensorLine[i]);
}

void set_motors( int leftSpeed, int rightSpeed)
{
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  if (leftSpeed >= 0) {
    analogWrite(IN1, leftSpeed);
    analogWrite(IN2, 0);
  } else {
    analogWrite(IN1, 0);
    analogWrite(IN2, -leftSpeed);
  }

  if (rightSpeed >= 0) {
    analogWrite(IN3, rightSpeed);
    analogWrite(IN4, 0);
  } else {
    analogWrite(IN3, 0);
    analogWrite(IN4, -rightSpeed);
  }
}

void Move()
{
  // Corssroad();
  if (millis() - last_move > 10)
  {
    last_move = millis();
    int a, b;
    a = sensorValues[0];
    b = sensorValues[1];
    int error = a - b;
    float up = error * Kp;
    float dp = (error - prev_error) * Kd;
    prev_error = error;
    int PID_result = (int)(up + dp);
    set_motors(Speed + PID_result, Speed - PID_result);
  }
}

void TunelMove()
{

  // Corssroad();
  if (millis() - last_move_IR > 10)
  {
    last_move_IR = millis();
    int error = IdealDistance - distanseRight;
    float up = error * Kp_IR;
    float dp = (error - prev_error_IR) * Kd_IR;
    prev_error_IR = error;
    int PID_result = (int)(up + dp);
    set_motors(Speed + PID_result, Speed - PID_result);
  }

}

/*void Move_after_tunnel()
  {
  Read_sensor();
  ReadIrSensor();
  //Corssroad();

  if ((sensorValues[0] && sensorValues[1]) < 75 && (distanseRight < 25))
  {
    last_move = millis();
    int error = (15 - distanseRight) * (Kp * 10);
    set_motors(Speed + error, Speed - error);
  } else if (millis() - last_move > 10)
  {
    last_move = millis();
    int a, b;
    a = sensorValues[0];
    b = sensorValues[1];
    int error = a - b;
    float up = error * Kp;
    float dp = (error - prev_error) * Kd;
    prev_error = error;
    int PID_result = (int)(up + dp);
    set_motors(Speed + PID_result, Speed - PID_result);
  }
  }*/

bool CheckCrossroad()
{
  if ((sensorValues[0] && sensorValues[1] > 400) && lineFl == 0)
  {
    lineFl = 1;
    countLine += 1;
    return true;
  }
  else
  {
    if ((sensorValues[0] && sensorValues[1] < 100) && lineFl == 1)
    {
      lineFl = 0;
      return false;
    }
  }
}

//========================Датчик цвета=======================================
// Метод обработки значений датчика цвета
void HandleColorSensor()
{
  switch ( sensor.Recognize() )
  {
    case sensor.Black:
      Serial.println("Black");
      break;
    case sensor.White:
      Serial.println("White");
      break;
    case sensor.Red:
      Serial.println("Red");
      break;
    case sensor.Yellow:
      Serial.println("Yellow");
      break;
    case sensor.Green:
      Serial.println("Green");
      break;
    case sensor.Blue:
      Serial.println("Blue");
      break;
    default:
      Serial.println("Unknown");
      break;
  }
}

//========================Датчики расстояния=======================================
void ReadIrSensor()
{
  /*
    int averagingTop = 0; // переменная для суммирования данных
    int averagingRight = 0; // переменная для суммирования данных
    // Получение 5 значений
    for (int i=0; i<3; i++)
    {
    distanseTopAnalog = analogRead(TopDistSensor);
    distanseRightAnalog = analogRead(RightDistSensor);
    // значение сенсора переводим в напряжение
    float volts1 = distanseTopAnalog*0.0048828125;
    float volts2 = distanseRightAnalog*0.0048828125;
    // и в расстояние в см
    int distance1=32*pow(volts1,-1.10);
    int distance2=32*pow(volts2,-1.10);
    averagingTop = averagingTop + distance1;
    averagingRight = averagingRight + distance2;
    delay(20); // Ожидание 55 ms перед каждым чтением
    }
    distanseTop = averagingTop / 6;
    distanseRight = averagingRight / 6;
    //
    //  distanseTop = analogRead(TopDistSensor);             // чтение данных с ИК-датчика расстояния
    //  distanseTop = map(distanseTop, 50, 500, 0, 255);     // преобразование к необходимому диапазону
    //
    //  distanseRight = analogRead(RightDistSensor);             // чтение данных с ИК-датчика расстояния
    //  distanseRight = map(distanseRight, 50, 500, 0, 255);     // преобразование к необходимому диапазону
  */
  distanseTop = Sen_ir_2.distance() / 2;
  distanseRight = Sen_ir_1.distance() / 2;
}

//===================серво======================
void set_setrvo() {
  int servo = 9;
  uint8_t tca, tcb;
  pinMode(servo, OUTPUT);
  tca = TCCR1A;
  tcb = TCCR1B;
  TCCR1A = TCCR1A & 0xe0 | 2; //0b1110 0010
  TCCR1B = TCCR1B & 0xe0 | 0x0d; //0b1110 1101

  analogWrite(servo, 38.27);  //15%
  TCCR1A = tca;
  TCCR1B = tcb;
}

void waggle() {
  int servo = 9;
  uint8_t tca, tcb;
  pinMode(servo, OUTPUT);
  tca = TCCR1A;
  tcb = TCCR1B;
  TCCR1A = TCCR1A & 0xe0 | 2; //0b1110 0010
  TCCR1B = TCCR1B & 0xe0 | 0x0d; //0b1110 1101

  analogWrite(servo, 25);  //15%
  delay(1000);

  analogWrite(servo, 38.27);  //15%
  delay(1000);

  analogWrite(servo, 25); //10%
  delay(1000);

  analogWrite(servo, 0);  //15%
  delay(1000);

  TCCR1A = tca;
  TCCR1B = tcb;
}


void ShiftCylinder()
{

}

void PushButton()
{

}
//======================================================


//========================Передача данных=======================================
void HandlePackage()
{
  uint8_t data[BufferInput.size() - 1];
  int i = 0;
  while (BufferInput.size() > 1)
  {
    data[i] = BufferInput.pop();
    i++;
  }

  // Если crc8 верный
  if (crc8(data, i) == BufferInput.pop())
  {
    // Обработка пришедших команд
    SendData(RECEIVED_DATA, nullptr);
  }
  else
  {
    SendData(ILLEGAL_DATA_VALUE, nullptr);
  }
}

void ReceiveData()
{
  uint8_t incomingByte;
  if (Serial2.available() > 0) {  //если есть доступные данные
    // считываем байт
    if (millis() - lastReceive > 500)
    {
      BufferInput.Clear();
      sizePackage = 0;
    }

    if (sizePackage == 0)
    {
      sizePackage = Serial2.read();
    }

    while (Serial2.available() > 0)
    {
      BufferInput.push(Serial2.read());
    }

    if (sizePackage == BufferInput.size())
    {
      HandlePackage();
    }
    lastReceive = millis();
  }
}

void SendData(uint8_t operation, uint8_t* data = nullptr, int data_len = 0)
{
  uint8_t data_send[data_len + 3];
  data_send[0] = data_len + 2;

  data_send[1] = operation;
  for (int i = 0; i < data_len; i++)
  {
    data_send[i + 2] = data[i];
  }
  data_send[data_len + 2] = crc8(data_send + 1, data_len + 1);
  Serial2.write(data_send, data_len + 3);
}

uint8_t crc8(const uint8_t *addr, uint8_t len) {
  uint8_t crc = 0;
  while (len--) {
    uint8_t inbyte = *addr++;
    for (uint8_t i = 8; i; i--) {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix) crc ^= 0x8C;
      inbyte >>= 1;
    }
  }
  return crc;
}
//===============================================================================

//======================Светодиоды===============================
void set_pin_leds() {
  pinMode(RedLED, OUTPUT);
  pinMode(GreenLED, OUTPUT);
  pinMode(BlueLED, OUTPUT);
  Led_Off();
}

void Red_Set() {
  digitalWrite(RedLED, HIGH);
  digitalWrite(GreenLED, LOW);
  digitalWrite(BlueLED, LOW);
}

void Green_Set() {
  digitalWrite(RedLED, LOW);
  digitalWrite(GreenLED, HIGH);
  digitalWrite(BlueLED, LOW);
}

void Blue_Set() {
  digitalWrite(RedLED, LOW);
  digitalWrite(GreenLED, LOW);
  digitalWrite(BlueLED, HIGH);
}

void Led_Off() {
  digitalWrite(RedLED, LOW);
  digitalWrite(GreenLED, LOW);
  digitalWrite(BlueLED, LOW);
}

//========================Тесты=======================================
void TestMethods()
{
  // TestMotors();
  // TestSensorLine();
  // HandleColorSensor();
  // TestIrSensors();
  // TestServo();
  // TestLED();
  // TestTunelZone();
  TestCylinderZone();
}

void TestLED()
{
  // Green_Set();
  // Blue_Set();
  // Red_Set();
  Led_Off();
}

void TestMotors()
{
  // line_robot.procSetMotors(255, 255);
  set_motors(255, 255);
}

void TestServo()
{
  //servo.write(90);
  waggle();
}


void TestIrSensors()
{
  ReadIrSensor();
  Serial.print("IR: ");
  Serial.print(distanseTop);
  Serial.print(":");
  Serial.print(distanseTopAnalog);

  Serial.print("  ");
  Serial.print(distanseRight);
  Serial.print(":");
  Serial.println(distanseRightAnalog);
  delay(1000);
}

void TestSensorLine()
{
  Read_sensor();
  // line_robot.Move();
  // line_robot.GetSensor(sensoValues);
  Serial.print(sensorValues[0]);
  Serial.print(" ");
  Serial.println(sensorValues[1]);
  delay(1000);
}

void TestCrossroad()
{
  Read_sensor();
  if ((sensorValues[0] && sensorValues[1] > 400) && lineFl == 0)
  {
    lineFl = 1;
    countLine += 1;
  } else if ((sensorValues[0] && sensorValues[1] < 100) && lineFl == 1) lineFl = 0;
  Serial.print(lineFl);
  Serial.print(" ");
  Serial.println(countLine);
  delay(1000);
}

void Test_Move_after_tunnel()
{
  Read_sensor();
  ReadIrSensor();
  //Corssroad();

  if ((sensorValues[0] && sensorValues[1]) < 75 && (distanseRight < 25))
  {
    last_move = millis();
    int error = (15 - distanseRight) * (Kp * 10);
    Serial.print("1 ");
    Serial.print(error);
    Serial.print(" ");
    set_motors(Speed + error, Speed - error);
    Serial.print(Speed + error);
    Serial.print(" ");
    Serial.print(Speed - error);
  } else if (millis() - last_move > 10)
  {
    last_move = millis();
    int a, b;
    a = sensorValues[0];
    b = sensorValues[1];
    int error = a - b;
    Serial.print("2 ");
    Serial.print(error);
    Serial.print(" ");
    float up = error * Kp;
    float dp = (error - prev_error) * Kd;
    prev_error = error;
    int PID_result = (int)(up + dp);
    set_motors(Speed + PID_result, Speed - PID_result);
    Serial.print(" ");
    Serial.print(Speed + PID_result);
    Serial.print(" ");
    Serial.print(Speed - PID_result);
  }
  Serial.print(" ");
  Serial.println(distanseRight);
  Serial.print(" ");
  Serial.print(sensorValues[0]);
  Serial.print(" ");
  Serial.println(sensorValues[1]);
  delay(1000);
}

void TestTunelZone()
{
  Speed = 140;
  Read_sensor();
  ReadIrSensor();
  HandleTunelZone();
}

void TestCylinderZone()
{
  MainColorR1 = sensor.Yellow;
  Speed = 100;
  Read_sensor();
  ReadIrSensor();
  HandleCylinderZone();
}
