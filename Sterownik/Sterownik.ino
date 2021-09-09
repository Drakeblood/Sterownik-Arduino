#include <ArxSmartPtr.h>
#include <arduino-timer.h>

#include <TFT.h>
#include <SPI.h>
#include <Thermistor.h>
#include <NTC_Thermistor.h>
#include <Stepper.h>

#define P_STEPPER_STEPS 2
#define WATER_STEPPER_STEPS 5

#define SCREEN_COLUMN_1 4
#define SCREEN_COLUMN_2 80

#define SCREEN_ROW_1 4
#define SCREEN_ROW_2 16
#define SCREEN_ROW_3 28
#define SCREEN_ROW_4 40
#define SCREEN_ROW_5 52
#define SCREEN_ROW_6 64
#define SCREEN_ROW_7 76

#define SCREEN_T1_X 24
#define SCREEN_T2_X 24
#define SCREEN_T3_X 100
#define SCREEN_P_X 24
#define SCREEN_STEP_X 32

#define GOOD_T1_TEMPERATURE 30.1f //78.1f
#define GOOD_T3_TEMPERATURE 30.1f //78.1f
#define TOO_HIGH_T1_TEMPERATURE 80.f
#define WATER_CONTROL_ACTIVATION_TEMPERATURE 30.f //50.f

#define ONE_MINUTE 5000 //60000
#define THREE_MINUTES 5000 //180000
#define FIVE_MINUTES 10000 //300000

bool g_IsGoodTemperature, g_IsWaterControlActived;
int g_CurrentStep;
float g_T1, g_T2, g_T3, g_P;
unsigned long g_T1OnGoodTemperatureStartTimePoint;
String g_CurrentActionInStepText, g_CurrentActionWaterControlText;
std::shared_ptr<Timer<>> g_T1Timer, g_T3Timer, g_LCDTextTimer;
std::shared_ptr<TFT> g_TFTScreen;
std::shared_ptr<Thermistor> g_Thermistor1, g_Thermistor2, g_Thermistor3;
std::shared_ptr<Stepper> g_Stepper1, g_Stepper2;

void T1Control1(), T1Control2(), T1Control3(), WaterControl(), UpdateScreenValues(), ClearCurrentActionInStepTextOnScreen(), PrintOnScreenFromString(String Text, int X, int Y);
bool IsEqual(float A, float B, float ErrorTolerance = 0.05f);
int GetAvgAnalogRead(int Pin, int AnalogReadAmount = 16);
float MapFloat(float x, float in_min, float in_max, float out_min, float out_max);

void setup()
{
  g_IsGoodTemperature = false;
  g_IsWaterControlActived = false;

  g_CurrentStep = 0;

  g_CurrentActionInStepText = "";
  g_CurrentActionWaterControlText = "";

  g_T1Timer = std::make_shared<Timer<>>();

  g_T3Timer = std::make_shared<Timer<>>();
  g_T3Timer->every(ONE_MINUTE, WaterControl);

  g_LCDTextTimer = std::make_shared<Timer<>>();
  g_LCDTextTimer->every(3000, UpdateScreenValues);

  g_TFTScreen = std::make_shared<TFT>(12 /*CS*/, 11 /*RS*/, 10 /*RST*/);
  g_TFTScreen->begin();
  g_TFTScreen->setRotation(3);
  g_TFTScreen->background(0, 0, 0);
  g_TFTScreen->stroke(255, 255, 255);
  g_TFTScreen->setTextSize(1);
  g_TFTScreen->text("P Control", SCREEN_COLUMN_1, SCREEN_ROW_1);
  g_TFTScreen->text("T1:", SCREEN_COLUMN_1, SCREEN_ROW_2);
  g_TFTScreen->text("T2:", SCREEN_COLUMN_1, SCREEN_ROW_3);
  g_TFTScreen->text("P:", SCREEN_COLUMN_1, SCREEN_ROW_4);
  g_TFTScreen->text("Step", SCREEN_COLUMN_1, SCREEN_ROW_6);
  g_TFTScreen->text("Water Control", SCREEN_COLUMN_2, SCREEN_ROW_1);
  g_TFTScreen->text("T3:", SCREEN_COLUMN_2, SCREEN_ROW_2);
  g_TFTScreen->text("Not active", SCREEN_COLUMN_2, SCREEN_ROW_6);

  g_Thermistor1 = std::make_shared<NTC_Thermistor>(A0 /*SENSOR_PIN*/, 9810 /*REFERENCE_RESISTANCE*/, 10000 /*NOMINAL_RESISTANCE*/, 25 /*NOMINAL_TEMPERATURE*/, 3950 /*B_VALUE*/);
  g_Thermistor2 = std::make_shared<NTC_Thermistor>(A1 /*SENSOR_PIN*/, 9810 /*REFERENCE_RESISTANCE*/, 10000 /*NOMINAL_RESISTANCE*/, 25 /*NOMINAL_TEMPERATURE*/, 3950 /*B_VALUE*/);
  g_Thermistor3 = std::make_shared<NTC_Thermistor>(A2 /*SENSOR_PIN*/, 9810 /*REFERENCE_RESISTANCE*/, 10000 /*NOMINAL_RESISTANCE*/, 25 /*NOMINAL_TEMPERATURE*/, 3950 /*B_VALUE*/);

  g_T1 = g_Thermistor1->readCelsius();
  g_T2 = g_Thermistor1->readCelsius();
  g_T3 = g_Thermistor1->readCelsius();

  g_P = 0.f;

  g_Stepper1 = std::make_shared<Stepper>(200 /*STEPS*/, 2 /*PIN1*/, 3 /*PIN2*/, 4 /*PIN3*/, 5 /*PIN4*/);
  g_Stepper1->setSpeed(20);

  g_Stepper2 = std::make_shared<Stepper>(200 /*STEPS*/, 6 /*PIN1*/, 7 /*PIN2*/, 8 /*PIN3*/, 9 /*PIN4*/);
  g_Stepper2->setSpeed(20);

  T1Control1();
}

void loop()
{
  g_T1Timer->tick();
  g_T3Timer->tick();
  g_LCDTextTimer->tick();
}

void T1Control1()
{
  ClearCurrentActionInStepTextOnScreen();
  if (g_CurrentStep != 1)
  {
    g_CurrentStep = 1;
    g_TFTScreen->text("1", SCREEN_STEP_X, SCREEN_ROW_6);
  }

  if (IsEqual(g_T1, GOOD_T1_TEMPERATURE, 1.f))
  {
    if (!g_IsGoodTemperature)
    {
      g_T1OnGoodTemperatureStartTimePoint = millis();
      g_IsGoodTemperature = true;
    }
    else
    {
      unsigned long CurrentGoodTemperatureTime = millis() - g_T1OnGoodTemperatureStartTimePoint;
      g_CurrentActionInStepText = String(CurrentGoodTemperatureTime / 1000);
      PrintOnScreenFromString(g_CurrentActionInStepText, SCREEN_COLUMN_1, SCREEN_ROW_7);

      if (CurrentGoodTemperatureTime > FIVE_MINUTES)
      {
        T1Control2();
        return;
      }
    }
    g_T1Timer->in(1000, T1Control1);

    return;
  }
  else if (g_T1 < GOOD_T1_TEMPERATURE)
  {
    g_Stepper1->step(-P_STEPPER_STEPS);

    g_CurrentActionInStepText = "Warming";
    PrintOnScreenFromString(g_CurrentActionInStepText, SCREEN_COLUMN_1, SCREEN_ROW_7);
  }
  else if (g_T1 > TOO_HIGH_T1_TEMPERATURE)
  {
    g_Stepper1->step(P_STEPPER_STEPS * 2);

    g_CurrentActionInStepText = "Cooling";
    PrintOnScreenFromString(g_CurrentActionInStepText, SCREEN_COLUMN_1, SCREEN_ROW_7);
  }
  else if (g_T1 > GOOD_T1_TEMPERATURE)
  {
    g_Stepper1->step(P_STEPPER_STEPS);

    g_CurrentActionInStepText = "Cooling";
    PrintOnScreenFromString(g_CurrentActionInStepText, SCREEN_COLUMN_1, SCREEN_ROW_7);
  }

  if (g_IsGoodTemperature)
  {
    g_IsGoodTemperature = false;
  }

  g_T1Timer->in(THREE_MINUTES, T1Control1);
}

void T1Control2()
{
  ClearCurrentActionInStepTextOnScreen();
  if (g_CurrentStep != 2)
  {
    g_CurrentStep = 2;

    g_TFTScreen->stroke(0, 0, 0);
    g_TFTScreen->text("1", SCREEN_STEP_X, SCREEN_ROW_6);

    g_TFTScreen->stroke(255, 255, 255);
    g_TFTScreen->text("2", SCREEN_STEP_X, SCREEN_ROW_6);
  }

  if (IsEqual(g_T1, GOOD_T1_TEMPERATURE, 1.f) || g_T1 < GOOD_T1_TEMPERATURE)//-------
  {
    g_Stepper1->step(-P_STEPPER_STEPS);

    g_CurrentActionInStepText = "Warming";
    PrintOnScreenFromString(g_CurrentActionInStepText, SCREEN_COLUMN_1, SCREEN_ROW_7);

    g_T1Timer->in(THREE_MINUTES, T1Control2);
  }
  else if (g_T1 > GOOD_T1_TEMPERATURE)
  {
    g_Stepper1->step(P_STEPPER_STEPS);

    T1Control3();
    g_T1Timer->every(ONE_MINUTE, T1Control3);
  }
}

void T1Control3()
{
  ClearCurrentActionInStepTextOnScreen();
  if (g_CurrentStep != 3)
  {
    g_CurrentStep = 3;

    g_TFTScreen->stroke(0, 0, 0);
    g_TFTScreen->text("2", SCREEN_STEP_X, SCREEN_ROW_6);

    g_TFTScreen->stroke(255, 255, 255);
    g_TFTScreen->text("3", SCREEN_STEP_X, SCREEN_ROW_6);
  }

  if (IsEqual(g_T1, GOOD_T1_TEMPERATURE, 1.f))//-------
  {
    g_CurrentActionInStepText = "Holding";
    PrintOnScreenFromString(g_CurrentActionInStepText, SCREEN_COLUMN_1, SCREEN_ROW_7);
  }
  else if (g_T1 < GOOD_T1_TEMPERATURE)
  {
    g_Stepper1->step(-P_STEPPER_STEPS);

    g_CurrentActionInStepText = "Warming";
    PrintOnScreenFromString(g_CurrentActionInStepText, SCREEN_COLUMN_1, SCREEN_ROW_7);
  }
  else if (g_T1 > GOOD_T1_TEMPERATURE)
  {
    g_Stepper1->step(P_STEPPER_STEPS);

    g_CurrentActionInStepText = "Cooling";
    PrintOnScreenFromString(g_CurrentActionInStepText, SCREEN_COLUMN_1, SCREEN_ROW_7);
  }
}

void WaterControl()
{
  if(g_T1 > WATER_CONTROL_ACTIVATION_TEMPERATURE)
  {
    if(!g_IsWaterControlActived)
    {
      g_IsWaterControlActived = true;

      g_TFTScreen->stroke(0, 0, 0);
      PrintOnScreenFromString("Not active", SCREEN_COLUMN_2, SCREEN_ROW_6);
      g_TFTScreen->stroke(255, 255, 255);
      PrintOnScreenFromString("Active", SCREEN_COLUMN_2, SCREEN_ROW_6);
    }

    g_TFTScreen->stroke(0, 0, 0);
    PrintOnScreenFromString(g_CurrentActionWaterControlText, SCREEN_COLUMN_2, SCREEN_ROW_7);
    g_TFTScreen->stroke(255, 255, 255);

    if (IsEqual(g_T3, GOOD_T3_TEMPERATURE, 2.f))
    {
      g_CurrentActionWaterControlText = "Holding";
      PrintOnScreenFromString(g_CurrentActionWaterControlText, SCREEN_COLUMN_2, SCREEN_ROW_7);
    }
    else if (g_T3 < GOOD_T3_TEMPERATURE)
    {
      g_Stepper2->step(WATER_STEPPER_STEPS);

      g_CurrentActionWaterControlText = "Warming";
      PrintOnScreenFromString(g_CurrentActionWaterControlText, SCREEN_COLUMN_2, SCREEN_ROW_7);
    }
    else if (g_T3 > GOOD_T3_TEMPERATURE)
    {
      g_Stepper2->step(-WATER_STEPPER_STEPS);

      g_CurrentActionWaterControlText = "Cooling";
      PrintOnScreenFromString(g_CurrentActionWaterControlText, SCREEN_COLUMN_2, SCREEN_ROW_7);
    }
  }
  else if(g_IsWaterControlActived)
  {
    g_IsWaterControlActived = false;

    g_TFTScreen->stroke(0, 0, 0);
    PrintOnScreenFromString("Active", SCREEN_COLUMN_2, SCREEN_ROW_6);
    PrintOnScreenFromString(g_CurrentActionWaterControlText, SCREEN_COLUMN_2, SCREEN_ROW_7);
    g_TFTScreen->stroke(255, 255, 255);
    PrintOnScreenFromString("Not active", SCREEN_COLUMN_2, SCREEN_ROW_6);
  }
}

void UpdateScreenValues()
{
  g_TFTScreen->stroke(0, 0, 0);

  PrintOnScreenFromString(String(g_T1), SCREEN_T1_X, SCREEN_ROW_2);
  PrintOnScreenFromString(String(g_T2), SCREEN_T2_X, SCREEN_ROW_3);
  PrintOnScreenFromString(String(g_T3), SCREEN_T3_X, SCREEN_ROW_2);
  PrintOnScreenFromString(String(g_P), SCREEN_P_X, SCREEN_ROW_4);

  g_TFTScreen->stroke(255, 255, 255);

  g_T1 = g_Thermistor1->readCelsius();
  g_T2 = g_Thermistor2->readCelsius();
  g_T3 = g_Thermistor3->readCelsius();

  g_P = MapFloat((5.0f * GetAvgAnalogRead(A3) / 1023), 0.5f, 4.5f, 0.0f, 2.06f);

  PrintOnScreenFromString(String(g_T1), SCREEN_T1_X, SCREEN_ROW_2);
  PrintOnScreenFromString(String(g_T2), SCREEN_T2_X, SCREEN_ROW_3);
  PrintOnScreenFromString(String(g_T3), SCREEN_T3_X, SCREEN_ROW_2);
  PrintOnScreenFromString(String(g_P), SCREEN_P_X, SCREEN_ROW_4);
}

void ClearCurrentActionInStepTextOnScreen()
{
  g_TFTScreen->stroke(0, 0, 0);
  PrintOnScreenFromString(g_CurrentActionInStepText, SCREEN_COLUMN_1, SCREEN_ROW_7);
  g_TFTScreen->stroke(255, 255, 255);
}

void PrintOnScreenFromString(String Text, int X, int Y)
{
  char *Text_char_array = new char[Text.length()];

  Text.toCharArray(Text_char_array, Text.length() + 1);

  g_TFTScreen->text(Text_char_array, X, Y);

  delete[] Text_char_array;
  Text_char_array = NULL;
}

bool IsEqual(float A, float B, float ErrorTolerance = 0.05f)
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