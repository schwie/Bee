#include <Clock.h>
#include <Onboard.h>
#include <XBeePlus.h>
#include <Wire.h> 
#include <Sensorhub.h>
#include <Bee.h>

#define NUM_SAMPLES 32
DateTime date = DateTime(__DATE__, __TIME__);

#include <OneWire.h>
#include <DallasTemperature.h>

#define DSB1820B_UUID 0x0018
#define DSB1820B_LENGTH_OF_DATA 2
#define DSB1820B_SCALE 100
#define DSB1820B_SHIFT 50

//Create Sensorhub Sensor from DallasTemp library
class DSB1820B: public Sensor
{
        public:
                OneWire oneWire;
                DallasTemperature dsb = DallasTemperature(&oneWire);
                
                DSB1820B(uint8_t samplePeriod=1):Sensor(DSB1820B_UUID, DSB1820B_LENGTH_OF_DATA, DSB1820B_SCALE, DSB1820B_SHIFT, true, samplePeriod){};
                String getName(){ return "Temperature Probe"; }
                String getUnits(){ return "C"; }
                void init(){
                   dsb.begin();
                   
                }
                
                void getData(){
                  dsb.requestTemperatures();
                  float sample = dsb.getTempCByIndex(0);
                  uint16_t tmp = (sample + DSB1820B_SHIFT) * DSB1820B_SCALE;
                  data[1]=tmp>>8;
                  data[0]=tmp;
      
                }

};

#define ANALOG_UUID 0x0003
#define ANALOG_LENGTH_OF_DATA 2
#define ANALOG_SCALE 100
#define ANALOG_SHIFT 0

//Create Sensorhub Sensor from DallasTemp library
class Analog: public Sensor
{
        public:
                uint8_t _port;
                
                Analog(uint8_t port, uint8_t samplePeriod=1):Sensor(ANALOG_UUID, ANALOG_LENGTH_OF_DATA, ANALOG_SCALE, ANALOG_SHIFT, false, samplePeriod){
                  _port = port;
                };
                String getName(){ return "Analog Input "; }
                String getUnits(){ return "mV"; }
                void init(){
                    analogReference(ADC_REFSEL0_bm);
                    PR.PRPA &= ~0x02;
                    ADCA.CALL = _ReadCalibrationByte( offsetof(NVM_PROD_SIGNATURES_t, ADCACAL0) );
                    ADCA.CALH = _ReadCalibrationByte( offsetof(NVM_PROD_SIGNATURES_t, ADCACAL1) );
                    ADCA.CALL = _ReadCalibrationByte( offsetof(NVM_PROD_SIGNATURES_t, ADCACAL0) );
                    ADCA.CALH = _ReadCalibrationByte( offsetof(NVM_PROD_SIGNATURES_t, ADCACAL1) );
   
                }
                void getData(){
                    if(_port==0){
                      PORTA.DIRCLR |= 0b10;      //this needs to be slicker with port
                    }
                    else
                      PORTA.DIRCLR |= 0b100;
                  
           
                    
                    int sample = 0;
                    for(int i=0; i<16; i++){
                      if(_port==0) sample+=analogRead12(A0);
                      else sample+=analogRead12(A1);
                    }
                    sample>>=4;
          
                    float resultADC = (analogRead12(A0)/4.056)*1.65; //convert to mV
                    uint16_t tmp = (resultADC * ANALOG_SCALE) + ANALOG_SHIFT;
                    data[1]=tmp>>8;
                    data[0]=tmp;
                }
                

      private:
                uint8_t _ReadCalibrationByte( uint8_t index ){
                  uint8_t result;
                  // Load the NVM Command register to read the calibration row.
                  NVM_CMD = NVM_CMD_READ_CALIB_ROW_gc;
                  result = pgm_read_byte(index);
                  // Clean up NVM Command register. 
                  NVM_CMD = NVM_CMD_NO_OPERATION_gc;
                  return( result );
                };
                
            
};


OnboardTemperature onboardTemp;
BatteryGauge batteryGauge;
Analog analog(0);
Analog analog2(1);
DSB1820B dsb;

#define NUM_SENSORS 5
Sensor * sensor[] = {&onboardTemp, &batteryGauge, &analog, &analog2, &dsb};
Sensorhub sensorhub(sensor,NUM_SENSORS);

#define DEBUG
#define XBEE_ENABLE

void setup(){  
  xbee.begin(9600);
  Serial.begin(57600);
  delay(1000);
  
  clock.begin(date);
  configureSleep();
  sensorhub.init();
  
  #ifdef DEBUG
  Serial.println("IDs:");
  Serial.print("[");
  for(int i=0; i<UUID_WIDTH*NUM_SENSORS;i++){
    Serial.print(sensorhub.ids[i]);
    Serial.print(", ");
  }
  Serial.print("]");
  Serial.println();
  
  Serial.println("starting");
  #endif
  
  #ifdef XBEE_ENABLE
  xbee.sendIDs(&sensorhub.ids[0], UUID_WIDTH*NUM_SENSORS);
  xbee.refresh();
  while(!xbee.available()){
    xbee.refresh();
  }
  xbee.meetCoordinator();
  #endif
}


bool firstRun=true;

void loop(){
  //if A1 woke us up and its log time OR if its the first run OR if the button has been pushed
  bool buttonPressed =  !clock.triggeredByA2() && !clock.triggeredByA1() ;
  clock.print();
  
  if( clock.triggeredByA1() ||  buttonPressed || firstRun){
    Serial.print("Sampling sensors");
    sensorhub.sample(true);
    clock.setAlarm1Delta(0,10);
  }
  
  if( ( clock.triggeredByA2() ||  buttonPressed ||firstRun)){
    #ifdef XBEE_ENABLE
    xbee.enable();
    #endif
    Serial.println("Creating datapoint from samples");
    sensorhub.log(true); 
    #ifdef XBEE_ENABLE
    while(!xbee.sendData(&sensorhub.data[0], sensorhub.getDataSize()));
    xbee.disable();
    #endif
    clock.setAlarm2Delta(10);
  }
  firstRun=false;
  sleep(); 
  
}



