#include <AccelStepper.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008
#define short_get_high_byte(x) ((HIGH_BYTE & x) >> 8)
#define short_get_low_byte(x)  (LOW_BYTE & x)
#define bytes_to_short(h,l) ( ((h << 8) & 0xff00) | (l & 0x00FF) );
byte mac[] = {0x90, 0xA2, 0xDA, 0x0D, 0x4C, 0x8C} ; //the mac adress in HEX of ethernet shield or uno shield board
byte ip[] = {169, 254, 211, 2}; // the IP adress of your device, that should be in same universe of the network you are using

// the next two variables are set when a packet is received
byte remoteIp[4];        // holds received packet's originating IP
unsigned int remotePort; // holds received packet's originating port

//customisation: Artnet SubnetID + UniverseID
//edit this with SubnetID + UniverseID you want to receive
byte SubnetID = {0};
byte UniverseID = {0};
short select_universe = ((SubnetID * 16) + UniverseID);

//customisation: edit this if you want for example read and copy only 4 or 6 channels from channel 12 or 48 or whatever.
const int number_of_channels = 512; //512 for 512 channels
int numar_canal = 1; /// numarul canalului nodului
const int number_of_control_channels = 6; /// numarul canalelor de control

//buffers
const int MAX_BUFFER_UDP = 768;
char packetBuffer[MAX_BUFFER_UDP]; //buffer to store incoming data
byte buffer_channel_arduino[number_of_channels]; //buffer to store filetered DMX data

// art net parameters
unsigned int localPort = 6454;      // artnet UDP port is by default 6454
const int art_net_header_size = 9;
const int max_packet_size = 576;
char ArtNetHead[8] = "Art-Net";
char OpHbyteReceive = 0;
char OpLbyteReceive = 0;
//short is_artnet_version_1=0;
//short is_artnet_version_2=0;
//short seq_artnet=0;
//short artnet_physical=0;
short incoming_universe = 0;
boolean is_opcode_is_dmx = 0;
boolean is_opcode_is_artpoll = 0;
boolean match_artnet = 1;
short Opcode = 0;
EthernetUDP Udp;

//LCD PINs
const int pin_RS = 8;
const int pin_EN = 9;
const int pin_d4 = 4;
const int pin_d5 = 5;
const int pin_d6 = 6;
const int pin_d7 = 7;
const int pin_BL = A5;
LiquidCrystal lcd( pin_RS,  pin_EN,  pin_d4,  pin_d5,  pin_d6,  pin_d7);

//Actuator PINs
#define dirPin 3
#define stepPin 2
AccelStepper Actuator1(1, stepPin, dirPin);
#define stepsPerRev 200
#define MS1 8
#define MS2 9
#define MS3 10

#define max_speed 1000
#define min_speed 100

#define EndStop 1

void setup() {
  //LCD init
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Art-Net Actuator V1");
  lcd.setCursor(0, 1);
  lcd.print("Press any key");
  delay(1000);
  int x = analogRead(0);
  while (x > 760) {
    //Steptem putin
    x = analogRead(0);
  }
  lcd.clear();
  SelectChannel(numar_canal);
  // put your setup code here, to run once:
  Serial.begin(115200);
  //setup ethernet and udp socket
  Ethernet.begin(mac, ip);
  //Control Pins
  Actuator1.setMaxSpeed(max_speed);
  Actuator1.setAcceleration(10);
  //EndStop
  pinMode(EndStop, INPUT_PULLUP);
}

int start_address = numar_canal - 1;

class Actuator {
  public:
    int steps;
    bool dir;
    int speed_ms;
    int CurrPos;
    int SoftBot;
    int SoftTop;
    Actuator() {
    }
    Actuator(int pasi, bool directie, int viteza) {
      this->steps = pasi;
      this->dir = directie;
      this->speed_ms = map(viteza, 0, 255, min_speed, max_speed);
      this->CurrPos = 0;
      this->SoftTop = 0;
      this->SoftBot = 0;
    }
    ~Actuator() {
    }
    void setNbOfSteps(int pasi) {
      this->steps = map(pasi, 0, 255, 0, stepsPerRev);
    }
    void setDir(bool directie) {
      this->dir = directie;
    }
    void setSpeedMs(int viteza) {
      this->speed_ms = map(viteza, 0, 255, min_speed, max_speed);
    }
    void SetZero() {
      Actuator1.setCurrentPosition(0);
      this->CurrPos = 0;
    }
    void SetSoftTop() {
      this->SoftTop = this->CurrPos;
    }
    void SetSoftBot() {
      this->SoftBot = this->CurrPos;
    }
    void GoToZero() {
      int initial_homing = -1;
      Actuator1.setSpeed(max_speed / 2);
      while ( analogRead(EndStop) >= 1000 ) { // Make the Stepper move CCW until the switch is activated
        Actuator1.moveTo(initial_homing);  // Set the position to move to
        Actuator1.setSpeed(max_speed / 2);
        initial_homing--;  // Decrease by 1 for next move if needed
        Actuator1.run();
        //Actuator1.setSpeed(max_speed);// Start moving the stepper
        //delay(5);
      }
      this->CurrPos = 0;
      this->SetZero();
      Serial.print("\n Homing Complete!");
    }

    void MoveActuatorManual() {
      int stepsToDo = this->steps;
      if (this->dir) {
        this->CurrPos += this->steps;
      }
      else {
        this->CurrPos -= this->steps;
        stepsToDo *= -1;
      }
      Actuator1.moveTo(this->CurrPos);
      Actuator1.setSpeed(max_speed);
      while (Actuator1.currentPosition() != this->CurrPos)
        Actuator1.run();
    }
    void LoadFromEEPROM() {
      this->SoftBot = (-1) * float(float(EEPROM[0] * 100 + EEPROM[1]) / 100) * 255;
      this->SoftTop = (-1) * float(float(EEPROM[2] * 100 + EEPROM[3]) / 100) * 255;
      Serial.print("\nLoaded Soft Bot at: ");
      Serial.print(this->SoftBot);
      Serial.print(" and Soft Top at: ");
      Serial.print(this->SoftTop);
      this->GoToSoftTop();
    }
    void MoveInRange(int Desired_Pos, int speed_m) {
      this->setSpeedMs(speed_m);
      int StepsToDo = map(Desired_Pos, 0, 255, this->SoftTop, this->SoftBot);
      Actuator1.moveTo(StepsToDo);
      Actuator1.setSpeed(this->speed_ms);
      while (Actuator1.currentPosition() != StepsToDo)
        Actuator1.run();
      Serial.print("\nCurrent Pos:");
      Serial.print(Desired_Pos);
      Serial.print(" meaning: ");
      Serial.print(Actuator1.currentPosition() - this->SoftTop);
      Serial.print(" steps from SoftTop.");
    }
    void GoToSoftTop() {
      this->GoToZero();
      Actuator1.moveTo(this->SoftTop);
      Actuator1.setSpeed(max_speed);
      while (Actuator1.currentPosition() != this->SoftTop)
        Actuator1.run();
      Serial.print("\nOn Soft Top");
    }
};

Actuator Act;

int params[6] = { -1, -1, -1, -1, -1, -1}; // C1 = Position ( 0 - 255 ), C2 = Viteza, C3 = Load from EEPROM / New Limits ( 0 / 1 ), C4 = SoftTopLim, C5 = SoftBotLim, C6 = Done
bool new_command = false, first_command = true;
int prev_p[6] = { -2, -2, -2, -2, -2, -2};

void SetNewLimits(int start_add) { //In Setup C1 = steps, C2 = Dir, C4 = SetSoftTop, C5 = SetSoftBot, C6 = Save&Exit
  int prev_pa[6];
  for (int i = 0 ; i < 6 ; i++)
    prev_pa[i] = prev_p[i];
  int params[6] = { -1, -1, -1, -1, -1, -1};
  bool new_command = false;
  Act.GoToZero();
  while (params[5] != 255) {
    getDMXParams(start_address, params);
    while ( !params[2] ) {
      if (analogRead(0) < 760) {
        lcd.clear();
        SelectChannel(numar_canal);
      }
      getDMXParams(start_address, params);
    }
    new_command = check_new_command(prev_pa, params);
    if (new_command) {
      if (params[3] == 255) { // SetSoftTop
        Act.SetSoftTop();
        Serial.println("\nSet Zero!");
      }
      else if (params[4] == 255) { // SetSoftBot
        Act.SetSoftBot();
        Serial.println("\nSet Lenght!");
      }
      else if (params[0] > 0) { //Move C1 pasi in directia C2
        Act.setNbOfSteps(params[0]);
        Act.setDir(params[1]);
        Act.MoveActuatorManual();
        Serial.print("\nCurr Pos:");
        Serial.print(Act.CurrPos);
        Serial.print("\n");
        Serial.print(params[0]);
      }
    }
  }
  Act.GoToSoftTop();
  //SAVE in EEPROM : EEPROM[0] = supraunitar, EEPROM[1] = fractionar
  float multiplicator = Act.SoftBot / 255; // Act.SoftBot = float(EEPROM[0] * 100 + EEPROM[1]) / 100 * 255;
  int sup = multiplicator;
  int subunit = (multiplicator * 100) - sup;
  EEPROM[0] = sup;
  EEPROM[1] = subunit;
  multiplicator = Act.SoftTop / 255;
  sup = multiplicator;
  subunit = (multiplicator * 100) - sup;
  EEPROM[2] = sup;
  EEPROM[3] = subunit;
  Serial.println("\nSaved New Config!: CurrPos:");
  Serial.print(Act.CurrPos);
  Serial.print(" ");
  Serial.print(Act.SoftBot);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (analogRead(0) < 760) {
    lcd.clear();
    SelectChannel(numar_canal);
  }
  start_address = numar_canal - 1;
  getDMXParams(start_address, params);
  while ( params[2] == -1 ) {
    if (analogRead(0) < 760) {
      lcd.clear();
      SelectChannel(numar_canal);
    }
    getDMXParams(start_address, params);
  }
  new_command = check_new_command(prev_p, params);
  if (new_command) {
    if ( params[2] && first_command ) { //Daca C3 este High
      ///NEW LIMITS
      Serial.print("\nSetting new limits!");
      SetNewLimits(start_address);
      first_command = false;
    }
    else if (params[2] == 0 && first_command) { //Daca C3 este Low
      Serial.print("\nStarting loading from EEPROM");
      Act.LoadFromEEPROM();
      first_command = false;
    }
    else if (params[1] && !first_command) {
      Serial.println("\nMoving in Range!");
      Act.MoveInRange(params[0], params[1]);
    }
  }
}
