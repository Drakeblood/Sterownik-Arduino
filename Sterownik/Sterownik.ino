#include <ArxSmartPtr.h>
#include <arduino-timer.h>

#include <TFT.h>  
#include <SPI.h>
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
float g_T1, g_T2, g_T3;
double g_T1OnGoodTemperatureStartTimePoint;
std::shared_ptr<Timer<>> g_Timer, g_LCDTextTimer;
std::shared_ptr<TFT> g_TFTScreen;
std::shared_ptr<Thermistor> g_Thermistor1, g_Thermistor2, g_Thermistor3;
std::shared_ptr<Stepper> g_Stepper1, g_Stepper2;

void SprawdzT1(), Krok2(), UpdateTemperature(), PrintTemperature();
bool IsTemperatureEqual(float A, float B, float ErrorTolerance = 0.05f);

void setup()
{ 
  g_IsGoodTemperature = false;

  g_Timer = std::make_shared<Timer<>>();
  g_LCDTextTimer = std::make_shared<Timer<>>();
  g_LCDTextTimer->every(3000, UpdateTemperature); 

  g_TFTScreen = std::make_shared<TFT>(12/*CS*/, 11/*RS*/, 10/*RST*/);
  g_TFTScreen->begin();
  g_TFTScreen->setRotation(3);
  g_TFTScreen->background(0, 0, 0);
  g_TFTScreen->stroke(255, 255, 255);
  g_TFTScreen->setTextSize(2);
  g_TFTScreen->text("T1:", 6, 6);
  g_TFTScreen->text("T2:", 6, 36);
  g_TFTScreen->text("T3:", 6, 66);

  g_Thermistor1 = std::make_shared<NTC_Thermistor>(A0/*SENSOR_PIN*/, 9810/*REFERENCE_RESISTANCE*/, 10000/*NOMINAL_RESISTANCE*/, 25/*NOMINAL_TEMPERATURE*/, 3950/*B_VALUE*/);
  g_Thermistor2 = std::make_shared<NTC_Thermistor>(A1/*SENSOR_PIN*/, 9810/*REFERENCE_RESISTANCE*/, 10000/*NOMINAL_RESISTANCE*/, 25/*NOMINAL_TEMPERATURE*/, 3950/*B_VALUE*/);
  g_Thermistor3 = std::make_shared<NTC_Thermistor>(A2/*SENSOR_PIN*/, 9810/*REFERENCE_RESISTANCE*/, 10000/*NOMINAL_RESISTANCE*/, 25/*NOMINAL_TEMPERATURE*/, 3950/*B_VALUE*/);

  g_T1 = g_Thermistor1->readCelsius();
  g_T2 = g_Thermistor1->readCelsius();
  g_T3 = g_Thermistor1->readCelsius();

  g_Stepper1 = std::make_shared<Stepper>(200/*STEPS*/, 2/*PIN1*/, 3/*PIN2*/, 4/*PIN3*/, 5/*PIN4*/);
  
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

void UpdateTemperature()
{
  g_TFTScreen->stroke(0, 0, 0);
  
  PrintTemperature(String(g_T1), Vector2D(50, 6));
  PrintTemperature(String(g_T2), Vector2D(50, 36));
  PrintTemperature(String(g_T3), Vector2D(50, 66));
  
  g_TFTScreen->stroke(255, 255, 255);
  
  g_T1 = g_Thermistor1->readCelsius();
  g_T2 = g_Thermistor2->readCelsius();
  g_T3 = g_Thermistor3->readCelsius();
  
  PrintTemperature(String(g_T1), Vector2D(50, 6));
  PrintTemperature(String(g_T2), Vector2D(50, 36));
  PrintTemperature(String(g_T3), Vector2D(50, 66));
}

void PrintTemperature(String TemperatureText, Vector2D ScreenPosition)
{
  char *TemperatureText_char_array = new char[TemperatureText.length()];

  TemperatureText.toCharArray(TemperatureText_char_array, TemperatureText.length() + 1);

  g_TFTScreen->text(TemperatureText_char_array, ScreenPosition.X, ScreenPosition.Y);

  delete [] TemperatureText_char_array;
  TemperatureText_char_array = NULL;
}

bool IsTemperatureEqual(float A, float B, float ErrorTolerance = 0.05f)
{
  return fabs(A - B) < ErrorTolerance;
}
