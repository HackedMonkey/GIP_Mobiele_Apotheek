/* Copyright (C) Mobiele Apotheek - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Robbe De Bondt/Michiel Van Kenhove <>, September 2017 - June 2018
*/

#include <Wire.h>
#include "SPI.h"
#include "MFRC522.h"


#define SS_PIN 10
#define RST_PIN 9
#define SP_PIN 8

MFRC522 rfid(SS_PIN, RST_PIN);

MFRC522::MIFARE_Key key;

char med;

String strID;
String copyStrID;

void setup() {
	Serial.begin(9600);
	SPI.begin();
	rfid.PCD_Init();

	Wire.begin(0x14); //Per magazijn verschillend
	/*
	Magazijn 0 --> 0x14
	Magazijn 1 --> 0x15
	Magazijn 2 --> 0x16
	*/
	Wire.onRequest(requestEvent);
}

void loop() {
	delay(1000);
	Serial.println(strID);

	if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
		return;

	MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);

	// Check if the PICC of Classic MIFARE type
	if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
		piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
		piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
		Serial.println(F("Your tag is not of type MIFARE Classic."));
		return;
	}

	strID = "";
	for (byte i = 0; i < 4; i++) {
		strID +=
			(rfid.uid.uidByte[i] < 0x10 ? "0" : "") +
			String(rfid.uid.uidByte[i], HEX) +
			(i != 3 ? ":" : "");
	}

	rfid.PICC_HaltA();
	rfid.PCD_StopCrypto1();

	strID.toUpperCase();

	rfid.PCD_Init();
	delay(10);

	copyStrID = strID;
}

void requestEvent() {

	if (copyStrID == "46:FA:01:89") {		//RFID waarde van Amoxillin
		med = '0';
	}
	else if (copyStrID == "7D:81:FF:8B")	//RFID waarde van Temazepam
	{
		med = '1';
	}
	else if (copyStrID == "8C:97:58:89")	//RFID waarde van Gabapentin
	{
		med = '2';
	}

	Wire.write(med);

}
