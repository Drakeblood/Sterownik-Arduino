#include <ArxSmartPtr.h>
#include <arduino-timer.h>

#include <TFT.h>  
#include <SPI.h>
#include <SD.h>
#include <Thermistor.h>
#include <NTC_Thermistor.h>
#include <Stepper.h>

#define GOOD_TEMPERATURE 78.1f
#define TOO_HIGH_TEMPERATURE 80.f

#define ONE_MINUTE 60000
#define THREE_MINUTES 180000
#define FIVE_MINUTES 300000

struct Vector2D
{
  int X;
  int Y;

  Vector2D(int x, int y)
  {
    X = x;
    Y = y;
  }
};

bool g_IsGoodTemperature;
float g_T1, g_T2, g_T3, g_P;
double g_T1OnGoodTemperatureStartTimePoint;
std::shared_ptr<Timer<>> g_Timer, g_LCDTextTimer;
std::shared_ptr<TFT> g_TFTScreen;
std::shared_ptr<Thermistor> g_Thermistor1, g_Thermistor2, g_Thermistor3;
std::shared_ptr<Stepper> g_Stepper1, g_Stepper2;

void SprawdzT1(), Krok2(), UpdateScreenValues(), PrintOnScreenFromString(String Text, Vector2D ScreenPosition);
bool IsTemperatureEqual(float A, float B, float ErrorTolerance = 0.05f);
int GetAvgAnalogRead(int Pin, int AnalogReadAmount = 16);
float MapFloat(float x, float in_min, float in_max, float out_min, float out_max);

void setup()
{ 
  g_IsGoodTemperature = false;

  g_Timer = std::make_shared<Timer<>>();
  g_LCDTextTimer = std::make_shared<Timer<>>();
  g_LCDTextTimer->every(3000, UpdateScreenValues); 

  g_TFTScreen = std::make_shared<TFT>(12/*CS*/, 11/*RS*/, 10/*RST*/);
  g_TFTScreen->begin();
  g_TFTScreen->setRotation(3);
  g_TFTScreen->background(0, 0, 0);
  g_TFTScreen->stroke(255, 255, 255);
  g_TFTScreen->setTextSize(1);
  g_TFTScreen->text("T1:", 4, 4);
  g_TFTScreen->text("T2:", 4, 14);
  g_TFTScreen->text("T3:", 4, 24);
  g_TFTScreen->text("P:", 4, 44);

  g_Thermistor1 = std::make_shared<NTC_Thermistor>(A0/*SENSOR_PIN*/, 9810/*REFERENCE_RESISTANCE*/, 10000/*NOMINAL_RESISTANCE*/, 25/*NOMINAL_TEMPERATURE*/, 3950/*B_VALUE*/);
  g_Thermistor2 = std::make_shared<NTC_Thermistor>(A1/*SENSOR_PIN*/, 9810/*REFERENCE_RESISTANCE*/, 10000/*NOMINAL_RESISTANCE*/, 25/*NOMINAL_TEMPERATURE*/, 3950/*B_VALUE*/);
  g_Thermistor3 = std::make_shared<NTC_Thermistor>(A2/*SENSOR_PIN*/, 9810/*REFERENCE_RESISTANCE*/, 10000/*NOMINAL_RESISTANCE*/, 25/*NOMINAL_TEMPERATURE*/, 3950/*B_VALUE*/);

  g_T1 = g_Thermistor1->readCelsius();
  g_T2 = g_Thermistor1->readCelsius();
  g_T3 = g_Thermistor1->readCelsius();

  g_P = 0.f;

  g_Stepper1 = std::make_shared<Stepper>(200/*STEPS*/, 2/*PIN1*/, 3/*PIN2*/, 4/*PIN3*/, 5/*PIN4*/);
  g_Stepper1->setSpeed(20);
  //SprawdzT1();
}

void loop()
{
  g_Timer->tick();
  g_LCDTextTimer->tick();
}

void SprawdzT1()
{
  if(IsTemperatureEqual(g_T1, GOOD_TEMPERATURE))
  {
    if(!g_IsGoodTemperature)
    {
      g_T1OnGoodTemperatureStartTimePoint = millis();
      g_IsGoodTemperature = true;
    }
    else
    {
      if(millis() - g_T1OnGoodTemperatureStartTimePoint > FIVE_MINUTES)
      {
        Krok2();
      }
    }
    return;
  }
  else if(g_T1 < GOOD_TEMPERATURE)
  {
    //zwieksz P1 o 5%
    g_Timer->in(THREE_MINUTES, SprawdzT1);
  }
  else if(g_T1 > TOO_HIGH_TEMPERATURE)
  {
    //zmniejsz P1 o 10%
    g_Timer->in(THREE_MINUTES, SprawdzT1);
  }
  else if(g_T1 > GOOD_TEMPERATURE)
  {
    //zmniejsz P1 o 5%
    g_Timer->in(THREE_MINUTES, SprawdzT1);
  }

  g_IsGoodTemperature = false;
}

void Krok2()
{
  //zwieksz P1 o 5%
  if(IsTemperatureEqual(g_T1, GOOD_TEMPERATURE))
  {
    g_Timer->in(THREE_MINUTES, Krok2);
  }
  else if(g_T1 > GOOD_TEMPERATURE)
  {
    //zmniejsz P1 o 5%
  }
}

void UpdateScreenValues()
{
  g_TFTScreen->stroke(0, 0, 0);
  
  PrintOnScreenFromString(String(g_T1), Vector2D(24, 4));
  PrintOnScreenFromString(String(g_T2), Vector2D(24, 14));
  PrintOnScreenFromString(String(g_T3), Vector2D(24, 24));
  PrintOnScreenFromString(String(g_P), Vector2D(24, 44));
  
  g_TFTScreen->stroke(255, 255, 255);
  
  g_T1 = g_Thermistor1->readCelsius();
  g_T2 = g_Thermistor2->readCelsius();
  g_T3 = g_Thermistor3->readCelsius();
  
  g_P = MapFloat((5.0f * GetAvgAnalogRead(A3) / 1023), 0.5f, 4.5f, 0.0f, 2.06f);
  
  PrintOnScreenFromString(String(g_T1), Vector2D(24, 4));
  PrintOnScreenFromString(String(g_T2), Vector2D(24, 14));
  PrintOnScreenFromString(String(g_T3), Vector2D(24, 24));
  PrintOnScreenFromString(String(g_P), Vector2D(24, 44));
}

void PrintOnScreenFromString(String Text, Vector2D ScreenPosition)
{
  char *Text_char_array = new char[Text.length()];

  Text.toCharArray(Text_char_array, Text.length() + 1);

  g_TFTScreen->text(Text_char_array, ScreenPosition.X, ScreenPosition.Y);

  delete [] Text_char_array;
  Text_char_array = NULL;
}

bool IsTemperatureEqual(float A, float B, float ErrorTolerance = 0.05f)
{
  return fabs(A - B) < ErrorTolerance;
}

float MapFloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int GetAvgAnalogRead(int Pin, int AnalogReadAmount = 16)
{
  int Raw = 0;
  for (int i = 0; i < AnalogReadAmount; i++)
  {
    Raw += analogRead(Pin);
  }
  return Raw / AnalogReadAmount;
}
