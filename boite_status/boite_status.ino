
/*
 * =====================================================================================
 *
 *       Filename:  boite_status.ino
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  02/11/2013 13:13:05
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Sebastien Chassot (sinux), seba.ptl@sinux.net
 *        Company:  PostTenebrasLab the geneva's hackerspace   posttenebraslab.ch
 *
 * =====================================================================================
 */

#include <SPI.h>
#include <Bounce.h>

/* Founctions  */
void update_buttons();

/* PINOUT */
/* Output pins are all PWM pins */
#define GALV1 5
#define GALV2 6
#define LED1 9
#define LEDR 10
#define LEDB 13
#define LEDG 11

/* Input pins are digital pins */
#define OnOff 2
#define LEVRR 7
#define LEVRL 4
#define LEVLR 8
#define LEVLL 12

/* Const */
#define STEP 2
#define MINUTE 60000
#define GALV1RANGE 540
#define GALV2RANGE 300
#define BLINKPERIOD 3000
#define BLINKOn 500

#define LED1On digitalWrite(LED1, HIGH)
#define LED1Off digitalWrite(LED1, LOW)
#define LEDROn analogWrite(LEDG, 255) 
#define LEDROff analogWrite(LEDG, 0) 
#define LEDBOn analogWrite(LEDG, 255) 
#define LEDBOff analogWrite(LEDG, 0) 
#define LEDGOn analogWrite(LEDG, 255) 
#define LEDGOff analogWrite(LEDG, 0) 

/* les levier et switch utilisent la librairie Bounce.h  */
Bounce cancel = Bounce(OnOff, 20);        /* debounce 20 ms  */
Bounce levRR = Bounce(LEVRR, 20);
Bounce levRL = Bounce(LEVRL, 20);
Bounce levLR = Bounce(LEVLR, 20);
Bounce levLL = Bounce(LEVLL, 20);

int stateGalv1 = 0;
int stateGalv2 = 0;

long opentime = 0;
long galv2scale = 0;

unsigned long reftime = 0;
unsigned long ledtime = 0;     /*  timer pour acceleration aiguille galva */


void setup() {

 // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial); // wait for serial port to connect. Needed for Leonardo only

  pinMode(OnOff, INPUT);
  pinMode(LEVRR, INPUT);
  pinMode(LEVRL, INPUT);
  pinMode(LEVLR, INPUT);
  pinMode(LEVLL, INPUT);
  
  pinMode(LED1, OUTPUT);      /* LED sous la coupole rouge (elle est blanche) */
  LED1Off;
  pinMode(LEDR, OUTPUT);      /* LED rouge sous la coupole blanche */
  LEDROff;
  pinMode(LEDB, OUTPUT);      /* LED bleue sous la coupole blanche */
  LEDBOff;
  pinMode(LEDG, OUTPUT);      /* LED verte sous la coupole blanche */
  LEDGOff;

}


void loop() {
 
  update_buttons();
  
  /********************
  * levier 1
  *********************/
  if( !levRR.read() )
  {
    opentime = constrain( opentime + STEP + STEP * (long)levRR.duration()/500, 0 , GALV1RANGE );  /* acceleration, valeur de 0 à GALV1RANGE */
    reftime = millis();
    delay(35);
  }
  else if( !levRL.read() )
  {
    opentime = constrain( opentime - STEP -  STEP * (long)levRL.duration()/500, 0 , GALV1RANGE );  /* acceleration, valeur de 0 à GALV1RANGE */
    delay(35);
  }


  /********************
  * levier 2
  *********************/
  if( !levLR.read() )
  {
    galv2scale = constrain( galv2scale + STEP + STEP * (long)levLR.duration()/500, 0 , GALV2RANGE );  /* acceleration, valeur de 0 à GALV2RANGE */
    delay(35);
  }
  else if( !levLL.read() )
  {
    galv2scale = constrain( galv2scale - STEP -  STEP * (long)levLL.duration()/500, 0 , GALV2RANGE );  /* acceleration, valeur de 0 à GALV2RANGE */
    delay(35);
  }


  /********************
  * Interrupteur
  *********************/
  if( cancel.read() && ( cancel.duration() > 2000 ) )
  {
    opentime = 0;
    galv2scale = 0;
  }

  stateGalv1 = map(opentime, 0, GALV1RANGE, 0, 255);        /*  durée d'ouverture de 0 à 540 min <- stateGalv1(0,255)  */
  stateGalv2 = map(galv2scale, 0, GALV2RANGE, 0, 255);      /*  durée d'ouverture de 0 à 540 min <- stateGalv1(0,255)  */


  /*  Une LED rouge clignotte lentement quand le local est fermé */
  if( !opentime )
  {
    if( (millis()-ledtime)> ( BLINKPERIOD + BLINKOn ) )
    {
      LED1Off;
      ledtime = millis();       /*  remise à zero du compteur  */

    }else if( (millis()-ledtime)> BLINKPERIOD ) {
      LED1On;
    }
  }

/****************************************************
 *
 * Temps 
 *
 ***************************************************/

  /*  toute les minutes on decremente le temps */
  if( (millis() - reftime) >= MINUTE )
  {
    opentime--;
    if(opentime < 0)
      opentime = 0;
    reftime = millis();
    // Serial.println("timer decrement");
  }
  

  /*  la dernière heure on passe du vert au rouge  */
  if( opentime < 60 )
  {
    analogWrite(LEDR, map(60 - opentime, 0, 60, 0, 255) );   /* rouge augmente */
    analogWrite(LEDG, map(opentime, 0, 60, 0, 255) );        /*  vert diminue */
  }

  /* quand plus d'une heure c'est vert */
  if( opentime > 60 )
  { 
    LEDGOn;
  }

/****************************************************
 *
 * Affichage pour server et galva
 *
 ***************************************************/

  Serial.print("Le local est ouvert pour : ");    /*  for the server  */
  Serial.print( opentime );
  Serial.print(" et le 2e levier est a : ");
  Serial.println( galv2scale );
  analogWrite(GALV1, stateGalv1);
  analogWrite(GALV2, stateGalv2);

}

void update_buttons() {

  /*   update debounce buttons  */ 
  cancel.update();
  levRR.update();
  levRL.update();
  levLR.update();
  levLL.update();

}
