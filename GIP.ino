/* Copyright (C) Mobiele Apotheek - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Michiel Van Kenhove <michiel.vankenhove@gmail.com>, September 2017 - June 2018
*/

#include <AccelStepper.h>
#include <StringSplitter.h> // https://github.com/aharshac/StringSplitter
#include <Wire.h>
#include "DeponHeader.h"
#include "TransPosHeader.h"

#define noodstop 2
#define enableTrans 12
#define authenticatiePersoon 23
#define signaallamp 24
#define readyWithSort 25
#define transportBandPositionReached 26
#define nema17Mag2_step 27
#define nema17Mag2_dir 28
#define nema17Mag2_enable 29
#define nema17Mag1_step 30
#define nema17Mag1_dir 31
#define nema17Mag1_enable 32
#define nema17Mag0_step 33
#define nema17Mag0_dir 34
#define nema17Mag0_enable 35
#define dvdMag2_step 36
#define dvdMag2_dir 37
#define dvdMag2_enable 38
#define dvdMag1_step 39
#define dvdMag1_dir 40
#define dvdMag1_enable 41
#define dvdMag0_step 4
#define dvdMag0_dir 43
#define dvdMag0_enable 44
#define stepperGlijbrug_step A0
#define stepperGlijbrug_dir 6
#define stepperGlijbrug_enable 47
#define gloeidraadMag0 50
#define gloeidraadMag1 49
#define gloeidraadMag2 48
#define veiligheidsRelais 51
#define start 52

/******
*DEBUG
*******/

bool DEBUG = false;

/******
*
*******/

static String medicationAvailable[] = { "Amoxillin", "Temazepam", "Gabapentin" };

String medId = "";
int patientAmount = 0;
int medType;
int medAmount;
int med0 = 0; //Medicatie van magazijn 0
int med1 = 0; //Medicatie van magazijn 1
int med2 = 0; //Medicatie van magazijn 2

transPosition posWhereTransShouldBe = TRANS_BEGIN_MAG0_BAK0;

static int steps1Zakje = 524; //TODO: CHANGE 
/*
D=8.5mm, O=26.70353755551mm --> 200 steps = 26.70mm, 70mm/O=2.6213755332782761185463208084884 --> 2.62 rotaties voor 70mm
2.62*200=524,27510665565522370926416169768 steps
--> 524
*/
static int stepsDvdMag2 = 250;
static int stepsDvdMag1 = 110;
static int stepsDvdMag0 = 100; //TODO: CHANGE
static int stepsGlijbrug = 750;

static int speedDvdMag2_uitgaand = 80;
static int speedDvdMag1_uitgaand = 80;
static int speedDvdMag0_uitgaand = 80;

static int speedDvdMag2_ingaand = 200;
static int speedDvdMag1_ingaand = 200;
static int speedDvdMag0_ingaand = 200;

static int speedNema17 = 700;
static int speedGlijbrug = 800;

AccelStepper stepperGlijbrug(AccelStepper::DRIVER, stepperGlijbrug_step, stepperGlijbrug_dir);

AccelStepper nema17Mag0(AccelStepper::DRIVER, nema17Mag0_step, nema17Mag0_dir);
AccelStepper nema17Mag1(AccelStepper::DRIVER, nema17Mag1_step, nema17Mag1_dir);
AccelStepper nema17Mag2(AccelStepper::DRIVER, nema17Mag2_step, nema17Mag2_dir);

AccelStepper dvdMag0(AccelStepper::DRIVER, dvdMag0_step, dvdMag0_dir);
AccelStepper dvdMag1(AccelStepper::DRIVER, dvdMag1_step, dvdMag1_dir);
AccelStepper dvdMag2(AccelStepper::DRIVER, dvdMag2_step, dvdMag2_dir);



volatile bool signaallampOn = false;
volatile bool gloeiMag0_on = false;
volatile bool gloeiMag1_on = false;
volatile bool gloeiMag2_on = false;

volatile bool wasJustInISR = false;

/*
Linkse magazijn: 0
Middelste magazijn: 1
Rechtse magazijn: 2
*/


void setup()
{
	Serial.begin(9600);
	Wire.begin();
	//pinMode(raspberryPi, INPUT);

	pinMode(noodstop, INPUT_PULLUP);
	pinMode(enableTrans, OUTPUT);
	pinMode(authenticatiePersoon, INPUT);
	pinMode(signaallamp, OUTPUT);
	pinMode(readyWithSort, OUTPUT);
	pinMode(transportBandPositionReached, INPUT);

	pinMode(nema17Mag0_enable, OUTPUT);
	pinMode(nema17Mag1_enable, OUTPUT);
	pinMode(nema17Mag2_enable, OUTPUT);

	pinMode(dvdMag0_enable, OUTPUT);
	pinMode(dvdMag1_enable, OUTPUT);
	pinMode(dvdMag2_enable, OUTPUT);

	pinMode(stepperGlijbrug_enable, OUTPUT);

	pinMode(gloeidraadMag0, OUTPUT);
	pinMode(gloeidraadMag1, OUTPUT);
	pinMode(gloeidraadMag2, OUTPUT);

	pinMode(veiligheidsRelais, INPUT_PULLUP);
	pinMode(start, INPUT); //PULLUP door RPI

	pinMode(13, OUTPUT);

	delay(20);

	digitalWrite(enableTrans, HIGH);

	disableSignaalLamp();

	digitalWrite(readyWithSort, LOW);

	digitalWrite(nema17Mag0_enable, LOW);
	digitalWrite(nema17Mag1_enable, LOW);
	digitalWrite(nema17Mag2_enable, LOW);

	digitalWrite(dvdMag0_enable, LOW);
	digitalWrite(dvdMag1_enable, LOW);
	digitalWrite(dvdMag2_enable, LOW);

	digitalWrite(stepperGlijbrug_enable, LOW);

	disableGloeidraad0();
	disableGloeidraad1();
	disableGloeidraad2();

	stepperGlijbrug.setMaxSpeed(1000);

	nema17Mag0.setMaxSpeed(800);
	nema17Mag1.setMaxSpeed(800);
	nema17Mag2.setMaxSpeed(800);

	dvdMag0.setMaxSpeed(250);
	dvdMag1.setMaxSpeed(250);
	dvdMag2.setMaxSpeed(250);



	attachInterrupt(digitalPinToInterrupt(noodstop), ISR_NS, HIGH);
}

void loop()
{
	if (DEBUG)
	{
		for (int i = 0; i < 1; i++)
		{
			digitalWrite(13, HIGH);
			delay(1000);
			digitalWrite(13, LOW);
			delay(1000);
		}
		delay(2500);
	}
	while (digitalRead(noodstop) == HIGH) {} // Zolang de noodstop zich niet in normale toestand bevindt

	if (DEBUG)
	{
		for (int i = 0; i < 2; i++)
		{
			digitalWrite(13, HIGH);
			delay(1000);
			digitalWrite(13, LOW);
			delay(1000);
		}
		delay(2500);
	}

	while (digitalRead(authenticatiePersoon) == LOW) {} // Wachten op RFID authenticatie
	delay(20);
	if (DEBUG)
	{
		for (int i = 0; i < 3; i++)
		{
			digitalWrite(13, HIGH);
			delay(1000);
			digitalWrite(13, LOW);
			delay(1000);
		}
		delay(2500);
	}
	while (digitalRead(authenticatiePersoon) == HIGH)
	{
		if (DEBUG)
		{
			digitalWrite(13, HIGH);
		}
		if (Serial.available())
		{
			medId = Serial.readString();
			//medId --> 0_1:1_1-#_#:#_#-#_#:#_#-#_#:#_#-#_#

			if (DEBUG)
			{
				digitalWrite(13, LOW);
			}

			StringSplitter* medIdSplitter;

			if (medId.indexOf("-") > 0)
			{
				medIdSplitter = new StringSplitter(medId, '-', 5);
				//medIdSplitter --> ["0_1:1_1", "#_#:#_#", ...]
				patientAmount = medIdSplitter->getItemCount();
			}
			else
			{
				patientAmount = 1;
			}

			//Signaallamp aanzetten
			enableSignaalLamp();

			String tempMedArray[5];

			switch (patientAmount)
			{
			case 1:
				sortOnePatient(medId);
				break;

			case 2:
				for (int i = 0; i < patientAmount; i++)
				{
					tempMedArray[i] = medIdSplitter->getItemAtIndex(i);
				}
				sortTwoPatients(tempMedArray);
				break;

			case 3:
				for (int i = 0; i < patientAmount; i++)
				{
					tempMedArray[i] = medIdSplitter->getItemAtIndex(i);
				}
				sortThreePatients(tempMedArray);
				break;

			case 4:
				for (int i = 0; i < patientAmount; i++)
				{
					tempMedArray[i] = medIdSplitter->getItemAtIndex(i);
				}
				sortFourPatients(tempMedArray);
				break;

			case 5:
				for (int i = 0; i < patientAmount; i++)
				{
					tempMedArray[i] = medIdSplitter->getItemAtIndex(i);
				}
				sortFivePatients(tempMedArray);
				break;
			default:
				// ERROR!
				break;
			}

			digitalWrite(readyWithSort, HIGH);
			delay(100);
			digitalWrite(readyWithSort, LOW);

			disableSignaalLamp();
		}
	}
}


void ISR_NS() {
	volatile bool wasGloei0_on = gloeiMag0_on;
	volatile bool wasGloei1_on = gloeiMag1_on;
	volatile bool wasGloei2_on = gloeiMag2_on;

	digitalWrite(enableTrans, LOW);
	disableGloeidraad0();
	disableGloeidraad1();
	disableGloeidraad2();

	while (digitalRead(noodstop) == HIGH)
	{
		/*Noodstop ingedrukt*/
	}

	while (digitalRead(veiligheidsRelais) == HIGH || digitalRead(start) == HIGH) {}

	digitalWrite(enableTrans, HIGH);

	while (digitalRead(transportBandPositionReached) == LOW) {}

	switch (posWhereTransShouldBe)
	{
	case TRANS_BEGIN_MAG0_BAK0:
		moveTransportbandToMag0AndBAK0();
		break;
	case TRANS_BAK1:
		moveTransportbandToBak1();
		break;
	case TRANS_MAG1_BAK2:
		moveTransportbandToMag1AndBAK2();
		break;
	case TRANS_BAK3:
		moveTransportbandToBak3();
		break;
	case TRANS_MAG2_BAK4:
		moveTransportbandToMag2AndBAK4();
		break;
	}

	while (digitalRead(transportBandPositionReached) == LOW) {}

	if (wasGloei0_on)
	{
		enableGloeidraad0();
	}

	if (wasGloei1_on)
	{
		enableGloeidraad1();
	}

	if (wasGloei2_on)
	{
		enableGloeidraad2();
	}

	wasJustInISR = true;
}

bool needMultipleMedication = false;
String medication[1][1];

void sortOnePatient(String medicationRaw)
{
	retrieveMagazineContents();

	StringSplitter* splitMultipleMedicationOne;

	//medicationRaw --> 0_1:1_1

	if (medicationRaw.indexOf(":") > 0)
	{
		needMultipleMedication = true;
		splitMultipleMedicationOne = new StringSplitter(medicationRaw, ':', 3); // Max. 3 verschillende medicatie per patiënt
		//splitMultipleMedicationOne --> ["0_1", "1_1"]
	}
	else
	{
		needMultipleMedication = false;
	}

	if (needMultipleMedication)
	{
		for (int i = 0; i < splitMultipleMedicationOne->getItemCount(); i++)
		{
			sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK0, i, (splitMultipleMedicationOne->getItemCount() - 1));
		}
	}
	else
	{
		//medicationRaw --> 0_1
		sortOneTypeMedication(medicationRaw, whereToDepon::BAK0, 1, 1);
	}
}

void sortTwoPatients(String _medicationRaw[])
{
	retrieveMagazineContents();

	for (int i = 0; i < 2; i++)
	{
		StringSplitter* splitMultipleMedicationOne;

		String medicationRaw = _medicationRaw[i];
		//medicationRaw --> 0_1:1_1

		int itteration = i;

		if (medicationRaw.indexOf(":") > 0)
		{
			needMultipleMedication = true;
			splitMultipleMedicationOne = new StringSplitter(medicationRaw, ':', 3); // Max. 3 verschillende medicatie per patiënt
																					//splitMultipleMedicationOne --> ["0_1", "1_1"]
		}
		else
		{
			needMultipleMedication = false;
		}

		if (needMultipleMedication)
		{
			for (int i = 0; i < splitMultipleMedicationOne->getItemCount(); i++)
			{
				switch (itteration)
				{
				case 0:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK0, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 1:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK1, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 2:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK2, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 3:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK3, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 4:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK4, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				}
			}
		}
		else
		{
			//medicationRaw --> 0_1
			switch (itteration)
			{
			case 0:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK0, 1, 1);
				break;
			case 1:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK1, 1, 1);
				break;
			case 2:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK2, 1, 1);
				break;
			case 3:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK3, 1, 1);
				break;
			case 4:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK4, 1, 1);
				break;
			}
		}
	}
}

void sortThreePatients(String _medicationRaw[])
{
	retrieveMagazineContents();

	for (int i = 0; i < 3; i++)
	{
		StringSplitter* splitMultipleMedicationOne;

		String medicationRaw = _medicationRaw[i];
		//medicationRaw --> 0_1:1_1

		int itteration = i;

		if (medicationRaw.indexOf(":") > 0)
		{
			needMultipleMedication = true;
			splitMultipleMedicationOne = new StringSplitter(medicationRaw, ':', 3); // Max. 3 verschillende medicatie per patiënt
																					//splitMultipleMedicationOne --> ["0_1", "1_1"]
		}
		else
		{
			needMultipleMedication = false;
		}

		if (needMultipleMedication)
		{
			for (int i = 0; i < splitMultipleMedicationOne->getItemCount(); i++)
			{
				switch (itteration)
				{
				case 0:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK0, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 1:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK1, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 2:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK2, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 3:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK3, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 4:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK4, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				}
			}
		}
		else
		{
			//medicationRaw --> 0_1
			switch (itteration)
			{
			case 0:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK0, 1, 1);
				break;
			case 1:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK1, 1, 1);
				break;
			case 2:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK2, 1, 1);
				break;
			case 3:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK3, 1, 1);
				break;
			case 4:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK4, 1, 1);
				break;
			}
		}
	}
}

void sortFourPatients(String _medicationRaw[])
{
	retrieveMagazineContents();

	for (int i = 0; i < 4; i++)
	{
		StringSplitter* splitMultipleMedicationOne;

		String medicationRaw = _medicationRaw[i];
		//medicationRaw --> 0_1:1_1

		int itteration = i;

		if (medicationRaw.indexOf(":") > 0)
		{
			needMultipleMedication = true;
			splitMultipleMedicationOne = new StringSplitter(medicationRaw, ':', 3); // Max. 3 verschillende medicatie per patiënt
																					//splitMultipleMedicationOne --> ["0_1", "1_1"]
		}
		else
		{
			needMultipleMedication = false;
		}

		if (needMultipleMedication)
		{
			for (int i = 0; i < splitMultipleMedicationOne->getItemCount(); i++)
			{
				switch (itteration)
				{
				case 0:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK0, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 1:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK1, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 2:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK2, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 3:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK3, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 4:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK4, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				}
			}
		}
		else
		{
			//medicationRaw --> 0_1
			switch (itteration)
			{
			case 0:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK0, 1, 1);
				break;
			case 1:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK1, 1, 1);
				break;
			case 2:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK2, 1, 1);
				break;
			case 3:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK3, 1, 1);
				break;
			case 4:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK4, 1, 1);
				break;
			}
		}
	}
}

void sortFivePatients(String _medicationRaw[])
{
	retrieveMagazineContents();

	for (int i = 0; i < 5; i++)
	{
		StringSplitter* splitMultipleMedicationOne;

		String medicationRaw = _medicationRaw[i];
		//medicationRaw --> 0_1:1_1

		int itteration = i;

		if (medicationRaw.indexOf(":") > 0)
		{
			needMultipleMedication = true;
			splitMultipleMedicationOne = new StringSplitter(medicationRaw, ':', 3); // Max. 3 verschillende medicatie per patiënt
																					//splitMultipleMedicationOne --> ["0_1", "1_1"]
		}
		else
		{
			needMultipleMedication = false;
		}

		if (needMultipleMedication)
		{
			for (int i = 0; i < splitMultipleMedicationOne->getItemCount(); i++)
			{
				switch (itteration)
				{
				case 0:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK0, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 1:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK1, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 2:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK2, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 3:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK3, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				case 4:
					sortOneTypeMedication(splitMultipleMedicationOne->getItemAtIndex(i), whereToDepon::BAK4, i, (splitMultipleMedicationOne->getItemCount() - 1));
					break;
				}
			}
		}
		else
		{
			//medicationRaw --> 0_1
			switch (itteration)
			{
			case 0:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK0, 1, 1);
				break;
			case 1:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK1, 1, 1);
				break;
			case 2:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK2, 1, 1);
				break;
			case 3:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK3, 1, 1);
				break;
			case 4:
				sortOneTypeMedication(medicationRaw, whereToDepon::BAK4, 1, 1);
				break;
			}
		}
	}
}

void retrieveMagazineContents()
{
	//Dit werkt wel maar is niet altijd 100% betrouwbaar door soms zeer onlogische antwoorden (uitgeschakeld voor nu --> uitbreiding (hardware is aanwezig))
	/*
	delay(50);

	Wire.requestFrom(0x14, 1);
	while (Wire.available())
	{
		med0 = Wire.read() - '0'; // - '0' is voor ascii te converteren naar decimaal
	}

	delay(50);

	Wire.requestFrom(0x15, 1);
	while (Wire.available())
	{
		med1 = Wire.read() - '0';
	}

	delay(50);

	Wire.requestFrom(0x16, 1);
	while (Wire.available())
	{
		med2 = Wire.read() - '0';
	}*/

	med0 = 0;
	med1 = 1;
	med2 = 2;
}

void moveTransportbandToMag0AndBAK0() //Magazijn 1 en bakje 1
{
	posWhereTransShouldBe = TRANS_BEGIN_MAG0_BAK0;

	delay(2);

	Wire.beginTransmission(0x10);
	Wire.write('a');
	Wire.endTransmission();
}

void moveTransportbandToMag1AndBAK2() //Magazijn 2 en bakje 3
{
	posWhereTransShouldBe = TRANS_MAG1_BAK2;

	delay(2);

	Wire.beginTransmission(0x10);
	Wire.write('b');
	Wire.endTransmission();
}

void moveTransportbandToMag2AndBAK4() //Magazijn 3 en bakje 5
{
	posWhereTransShouldBe = TRANS_MAG2_BAK4;

	delay(2);

	Wire.beginTransmission(0x10);
	Wire.write('c');
	Wire.endTransmission();
}

void moveTransportbandToBak1() //Bakje 2
{
	posWhereTransShouldBe = TRANS_BAK1;

	delay(2);

	Wire.beginTransmission(0x10);
	Wire.write('d');
	Wire.endTransmission();
}

void moveTransportbandToBak3() //Bakje 4
{
	posWhereTransShouldBe = TRANS_BAK3;

	delay(2);

	Wire.beginTransmission(0x10);
	Wire.write('e');
	Wire.endTransmission();
}

void moveGlijbrug()
{
	delay(2);
	Wire.beginTransmission(0x10);
	Wire.write('f');
	Wire.endTransmission();
}


void sortOneTypeMedication(String _medicationRaw, whereToDepon _whereToDepon, int itterationAmount, int totalMedAmount)
{
	delay(75);
	//_medicationRaw --> 0_1

	StringSplitter* tempSplitter = new StringSplitter(_medicationRaw, '_', 2);

	medType = tempSplitter->getItemAtIndex(0).toInt();
	medAmount = tempSplitter->getItemAtIndex(1).toInt();

	//medType --> 0
	//medAmount --> 1
	//We gaan ervan uit dat medicatie 0 (medType) in magazijn 1 (middelste) zit. (door retrieveMagazineContents) (Dit geldt nu niet aangezien locatie 'hardcoded' is, zie retrieveMagazineContents)

	if (medType == med0)
	{
		//nodige medicatie zit in magazijn 0
		moveTransportbandToMag0AndBAK0();

		while (digitalRead(transportBandPositionReached) == LOW) {} //wachten tot transportband locatie heeft bereikt

		digitalWrite(nema17Mag0_enable, HIGH);

		delay(100);

		nema17Mag0.move(steps1Zakje * medAmount);
		nema17Mag0.setSpeed(speedNema17);

		while (abs(nema17Mag0.distanceToGo()) > 0)
		{
			nema17Mag0.runSpeedToPosition();
		}

		delay(100);

		digitalWrite(nema17Mag0_enable, LOW);
		digitalWrite(dvdMag0_enable, HIGH);
		enableGloeidraad0();

		delay(500);

		checkIfJustInISR();

		dvdMag0.move(stepsDvdMag0);
		dvdMag0.setSpeed(speedDvdMag0_uitgaand);

		checkIfJustInISR();

		while (abs(dvdMag0.distanceToGo()) > 0)
		{
			checkIfJustInISR();
			dvdMag0.runSpeedToPosition();
		}

		delay(2500);

		dvdMag0.move(-stepsDvdMag0);
		dvdMag0.setSpeed(speedDvdMag0_ingaand);

		while (abs(dvdMag0.distanceToGo()) > 0)
		{
			dvdMag0.runSpeedToPosition();
		}

		delay(50);

		disableGloeidraad0();
		digitalWrite(dvdMag0_enable, LOW);

		delay(1500);

		if (itterationAmount == totalMedAmount)
		{
			switch (_whereToDepon)
			{
			case BAK0:
				moveTransportbandToMag0AndBAK0();
				break;
			case BAK1:
				moveTransportbandToBak1();
				break;
			case BAK2:
				moveTransportbandToMag1AndBAK2();
				break;
			case BAK3:
				moveTransportbandToBak3();
				break;
			case BAK4:
				moveTransportbandToMag2AndBAK4();
				break;
			}

			while (digitalRead(transportBandPositionReached) == LOW) {}

			delay(100);

			digitalWrite(stepperGlijbrug_enable, HIGH);

			delay(1000);

			stepperGlijbrug.move(stepsGlijbrug);
			stepperGlijbrug.setSpeed(speedGlijbrug);

			delay(50);

			while (abs(stepperGlijbrug.distanceToGo()) > 0)
			{
				stepperGlijbrug.runSpeedToPosition();
			}

			delay(1500);

			stepperGlijbrug.move(-stepsGlijbrug);
			stepperGlijbrug.setSpeed(speedGlijbrug);

			delay(50);

			while (abs(stepperGlijbrug.distanceToGo()) > 0)
			{
				stepperGlijbrug.runSpeedToPosition();
			}

			delay(100);

			digitalWrite(stepperGlijbrug_enable, LOW);

			moveTransportbandToMag0AndBAK0();

			while (digitalRead(transportBandPositionReached) == LOW) {}
		}

	}
	else if (medType == med1)
	{
		//nodige medicatie zit in magazijn 1
		moveTransportbandToMag1AndBAK2();

		while (digitalRead(transportBandPositionReached) == LOW) {} //wachten tot transportband locatie heeft bereikt

		digitalWrite(nema17Mag1_enable, HIGH);

		delay(500);

		nema17Mag1.move(steps1Zakje * medAmount);
		nema17Mag1.setSpeed(speedNema17);

		while (abs(nema17Mag1.distanceToGo()) > 0)
		{
			nema17Mag1.runSpeedToPosition();
		}

		delay(100);

		digitalWrite(nema17Mag1_enable, LOW);
		digitalWrite(dvdMag1_enable, HIGH);
		enableGloeidraad1();

		delay(500);

		checkIfJustInISR();

		dvdMag1.move(stepsDvdMag1);
		dvdMag1.setSpeed(speedDvdMag1_uitgaand);

		checkIfJustInISR();

		while (abs(dvdMag1.distanceToGo()) > 0)
		{
			checkIfJustInISR();
			dvdMag1.runSpeedToPosition();
		}

		delay(2500);

		dvdMag1.move(-stepsDvdMag1);
		dvdMag1.setSpeed(speedDvdMag1_ingaand);

		while (abs(dvdMag1.distanceToGo()) > 0)
		{
			dvdMag1.runSpeedToPosition();
		}

		delay(50);

		disableGloeidraad1();
		digitalWrite(dvdMag1_enable, LOW);

		delay(1000);

		if (itterationAmount == totalMedAmount)
		{
			switch (_whereToDepon)
			{
			case BAK0:
				moveTransportbandToMag0AndBAK0();
				break;
			case BAK1:
				moveTransportbandToBak1();
				break;
			case BAK2:
				moveTransportbandToMag1AndBAK2();
				break;
			case BAK3:
				moveTransportbandToBak3();
				break;
			case BAK4:
				moveTransportbandToMag2AndBAK4();
				break;
			}

			while (digitalRead(transportBandPositionReached) == LOW) {}

			delay(100);

			digitalWrite(stepperGlijbrug_enable, HIGH);

			delay(1000);

			stepperGlijbrug.move(stepsGlijbrug);
			stepperGlijbrug.setSpeed(speedGlijbrug);

			delay(50);

			while (abs(stepperGlijbrug.distanceToGo()) > 0)
			{
				stepperGlijbrug.runSpeedToPosition();
			}

			delay(1500);

			stepperGlijbrug.move(-stepsGlijbrug);
			stepperGlijbrug.setSpeed(speedGlijbrug);

			delay(50);

			while (abs(stepperGlijbrug.distanceToGo()) > 0)
			{
				stepperGlijbrug.runSpeedToPosition();
			}

			delay(100);

			digitalWrite(stepperGlijbrug_enable, LOW);

			moveTransportbandToMag0AndBAK0();

			while (digitalRead(transportBandPositionReached) == LOW) {}
		}
	}
	else if (medType == med2)
	{
		//nodige medicatie zit in magazijn 2
		moveTransportbandToMag2AndBAK4();

		while (digitalRead(transportBandPositionReached) == LOW) {} //wachten tot transportband locatie heeft bereikt

		digitalWrite(nema17Mag2_enable, HIGH);

		delay(500);

		nema17Mag2.move(steps1Zakje * medAmount);
		nema17Mag2.setSpeed(speedNema17);

		while (abs(nema17Mag2.distanceToGo()) > 0)
		{
			nema17Mag2.runSpeedToPosition();
		}

		delay(100);

		digitalWrite(nema17Mag2_enable, LOW);
		digitalWrite(dvdMag2_enable, HIGH);
		enableGloeidraad2();

		delay(500);

		checkIfJustInISR();

		dvdMag2.move(stepsDvdMag2);
		dvdMag2.setSpeed(speedDvdMag2_uitgaand);

		checkIfJustInISR();

		while (abs(dvdMag2.distanceToGo()) > 0)
		{
			checkIfJustInISR();
			dvdMag2.runSpeedToPosition();
		}

		delay(2500);

		dvdMag2.move(-stepsDvdMag2);
		dvdMag2.setSpeed(speedDvdMag2_ingaand);

		while (abs(dvdMag2.distanceToGo()) > 0)
		{
			dvdMag2.runSpeedToPosition();
		}

		delay(50);

		disableGloeidraad2();
		digitalWrite(dvdMag2_enable, LOW);

		delay(1000);

		if (itterationAmount == totalMedAmount)
		{
			switch (_whereToDepon)
			{
			case BAK0:
				moveTransportbandToMag0AndBAK0();
				break;
			case BAK1:
				moveTransportbandToBak1();
				break;
			case BAK2:
				moveTransportbandToMag1AndBAK2();
				break;
			case BAK3:
				moveTransportbandToBak3();
				break;
			case BAK4:
				moveTransportbandToMag2AndBAK4();
				break;
			}

			while (digitalRead(transportBandPositionReached) == LOW) {}

			delay(100);

			digitalWrite(stepperGlijbrug_enable, HIGH);

			delay(1000);

			stepperGlijbrug.move(stepsGlijbrug);
			stepperGlijbrug.setSpeed(speedGlijbrug);

			delay(50);

			while (abs(stepperGlijbrug.distanceToGo()) > 0)
			{
				stepperGlijbrug.runSpeedToPosition();
			}

			delay(1500);

			stepperGlijbrug.move(-stepsGlijbrug);
			stepperGlijbrug.setSpeed(speedGlijbrug);

			delay(50);

			while (abs(stepperGlijbrug.distanceToGo()) > 0)
			{
				stepperGlijbrug.runSpeedToPosition();
			}

			delay(100);

			digitalWrite(stepperGlijbrug_enable, LOW);

			moveTransportbandToMag0AndBAK0();

			while (digitalRead(transportBandPositionReached) == LOW) {}
		}
	}
	delay(500);
}

void enableSignaalLamp()
{
	digitalWrite(signaallamp, LOW);
	signaallampOn = true;
}

void disableSignaalLamp()
{
	digitalWrite(signaallamp, HIGH);
	signaallampOn = false;
}


void enableGloeidraad0()
{
	digitalWrite(gloeidraadMag0, LOW);
	gloeiMag0_on = true;
}

void disableGloeidraad0()
{
	digitalWrite(gloeidraadMag0, HIGH);
	gloeiMag0_on = false;
}

void enableGloeidraad1()
{
	digitalWrite(gloeidraadMag1, LOW);
	gloeiMag1_on = true;
}

void disableGloeidraad1()
{
	digitalWrite(gloeidraadMag1, HIGH);
	gloeiMag1_on = false;
}

void enableGloeidraad2()
{
	digitalWrite(gloeidraadMag2, LOW);
	gloeiMag2_on = true;
}

void disableGloeidraad2()
{
	digitalWrite(gloeidraadMag2, HIGH);
	gloeiMag2_on = false;
}

void checkIfJustInISR()
{
	if (wasJustInISR)
	{
		delay(400);
		wasJustInISR = false;
	}
}