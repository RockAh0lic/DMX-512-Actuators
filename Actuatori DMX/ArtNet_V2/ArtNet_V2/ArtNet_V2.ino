#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008
#define short_get_high_byte(x) ((HIGH_BYTE & x) >> 8)
#define short_get_low_byte(x)  (LOW_BYTE & x)
#define bytes_to_short(h,l) ( ((h << 8) & 0xff00) | (l & 0x00FF) );
byte mac[] = {0x90, 0xA2, 0xDA, 0x0D, 0x4C, 0x8C} ; //the mac adress in HEX of ethernet shield or uno shield board
byte ip[] = {169, 254, 199, 1}; // the IP adress of your device, that should be in same universe of the network you are using

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
const int pin_BL = 10;
LiquidCrystal lcd( pin_RS,  pin_EN,  pin_d4,  pin_d5,  pin_d6,  pin_d7);

//Actuator PINs
#define dirPin 3
#define stepPin 2
#define stepsPerRev 800
#define MS1 8
#define MS2 9
#define MS3 10

#define max_speed 1
#define min_speed 50

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
  pinMode(dirPin, OUTPUT);
  pinMode(stepPin, OUTPUT);

  //Microstepping
  pinMode(MS1, OUTPUT);
  pinMode(MS2, OUTPUT);
  pinMode(MS3, OUTPUT);
}

int start_address = numar_canal - 1;

class Actuator {
  public:
    int steps;
    bool dir;
    int speed_ms;
    int CurrPos;
    int TotalSteps;
    Actuator() {
    }
    Actuator(int pasi, bool directie, int viteza) {
      this->steps = pasi;
      this->dir = directie;
      this->speed_ms = map(viteza, 0, 255, min_speed, max_speed);
      this->CurrPos = 0;
      this->TotalSteps = 0;
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
      this->CurrPos = 0;
    }
    void SetTotalLenght() {
      this->TotalSteps = this->CurrPos;
    }
    void SaveCurrPos(int val_c1) { //Dupa orice Miscare
      EEPROM[2] = val_c1;
    }
    void GoToZero() {
      digitalWrite(dirPin, LOW);
      for (int i = this->CurrPos; i >= 0; i--) {
        digitalWrite(stepPin, HIGH);
        delay(2);
        digitalWrite(stepPin, LOW);
      }
      this->CurrPos = 0;
      SaveCurrPos(0);
    }
    void MoveActuatorManual() {
      if (this->dir) {
        digitalWrite(dirPin, HIGH);
        this->CurrPos += this->steps;
      }
      else {
        digitalWrite(dirPin, LOW);
        this->CurrPos -= this->steps;
      }
      for (int i = 0; i < this->steps; i++) {
        digitalWrite(stepPin, HIGH);
        delay(2);
        digitalWrite(stepPin, LOW);
      }
    }
    void LoadFromEEPROM() {
      this->TotalSteps = float(float(EEPROM[0] * 100 + EEPROM[1]) / 100) * 255;
      int factor_pozitie = EEPROM[2];
      this->CurrPos = map(EEPROM[2], 0, 255, 0, this->TotalSteps);
      Serial.print("\nLoaded Total Steps: ");
      Serial.print(this->TotalSteps);
      Serial.print(" and current steps position: ");
      Serial.print(this->CurrPos);
      Serial.print(" meaning position: ");
      Serial.print(map(this->CurrPos, 0, this->TotalSteps, 0, 255));
    }
    void MoveInRange(int Desired_Pos, int speed_m) {
      this->setSpeedMs(speed_m);
      int StepsToDo = map(Desired_Pos, 0, 255, 0, this->TotalSteps);
      bool UpDown;
      if (StepsToDo > this->CurrPos){
        digitalWrite(dirPin, HIGH);
        UpDown = true;
      }
      else{
        digitalWrite(dirPin, LOW);
        UpDown = false;
      }
      int RealTimeSteps = this->CurrPos;
      int stepsToDo = abs(StepsToDo - this->CurrPos);
      Serial.print("Steps to do: ");
      Serial.print(stepsToDo);
      Serial.print(" for final pos: ");
      Serial.print(Desired_Pos);
      Serial.print(" meaning: ");
      Serial.print(StepsToDo);
      Serial.print(" steps from 0.");
      for (int i = 0; i <= stepsToDo; i++) {
        digitalWrite(stepPin, HIGH);
        delay(this->speed_ms);
        digitalWrite(stepPin, LOW);
        if(UpDown){
          RealTimeSteps++;
        }else{
          RealTimeSteps--;
        }
        int RealPos = map(RealTimeSteps, 0, this->TotalSteps, 0, 255);
        this->SaveCurrPos(RealPos);
      }
      this->CurrPos = StepsToDo;
      this->SaveCurrPos(Desired_Pos);
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
        Act.SetZero();
        Serial.println("\nSet Zero!");
      }
      else if (params[4] == 255) { // SetSoftBot
        Act.SetTotalLenght();
        Serial.println("\nSet Lenght!");
      }
      else if (params[0]) { //Move C1 pasi in directia C2
        Act.setNbOfSteps(params[0]);
        Act.setDir(params[1]);
        Act.MoveActuatorManual();
        Serial.print("\nCurr Pos:");
        Serial.print(Act.CurrPos);
      }
    }
  }
  Act.GoToZero();
  //SAVE in EEPROM : EEPROM[0] = supraunitar, EEPROM[1] = fractionar
  float multiplicator = Act.TotalSteps / 255; // Act.TotalSteps = float(EEPROM[0] * 100 + EEPROM[1]) / 100 * 255;
  int sup = multiplicator;
  int subunit = (multiplicator * 100) - sup;
  EEPROM[0] = sup;
  EEPROM[1] = subunit;
  Serial.println("\nSaved New Config!: CurrPos:");
  Serial.print(Act.CurrPos);
  Serial.print(" ");
  Serial.print(Act.TotalSteps);
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
      SetNewLimits(start_address);
      first_command = false;
    }
    else if (params[2] == 0 && first_command) { //Daca C3 este Low
      Serial.print("\nStarting loading from EEPROM");
      Act.LoadFromEEPROM();
      Serial.println("\nMoving in Range!");
      Act.MoveInRange(params[0], params[1]);
      first_command = false;
    }
    else if (params[1] && !first_command) {
      Serial.println("\nMoving in Range!");
      Act.MoveInRange(params[0], params[1]);
    }
  }
}
