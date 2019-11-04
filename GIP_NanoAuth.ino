/* Copyright (C) Mobiele Apotheek - All Rights Reserved
* Unauthorized copying of this file, via any medium is strictly prohibited
* Proprietary and confidential
* Written by Michiel Van Kenhove <michiel.vankenhove@gmail.com>, September 2017 - June 2018
*/

#include "SPI.h"
#include "MFRC522.h"


#define SS_PIN 10
#define RST_PIN 9
#define SP_PIN 8
#define RFCfgReg = 0x26 << 1
#define authPinArduino 2
#define authPinRPI 4
#define logout 3

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

String authID;

void setup() {
	Serial.begin(9600);
	SPI.begin();
	rfid.PCD_Init();

	pinMode(authPinArduino, OUTPUT);
	pinMode(authPinRPI, OUTPUT);
	pinMode(logout, INPUT);
	digitalWrite(authPinArduino, LOW);
	digitalWrite(authPinRPI, LOW);

	attachInterrupt(digitalPinToInterrupt(logout), ISR_logout, RISING);
}

void loop() {
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

	authID = "";
	for (byte i = 0; i < 4; i++) {
		authID +=
			(rfid.uid.uidByte[i] < 0x10 ? "0" : "") +
			String(rfid.uid.uidByte[i], HEX) +
			(i != 3 ? ":" : "");
	}

	rfid.PICC_HaltA();
	rfid.PCD_StopCrypto1();

	authID.toUpperCase();


	if (authID.equals("A2:6B:AF:89") || authID.equals("9F:79:AB:A9") || authID.equals("7E:64:58:89") || authID.equals("48:23:77:C9") || authID.equals("03:90:A9:89"))
	{
		digitalWrite(authPinRPI, HIGH);
		digitalWrite(authPinArduino, HIGH);
		Serial.println("Logged in");
		delay(500);
		digitalWrite(authPinRPI, LOW);
	}
	else
	{
		digitalWrite(authPinArduino, LOW);
		Serial.println("Wrong ID");
	}

	delay(100);
}

void ISR_logout()
{
	digitalWrite(authPinArduino, LOW);
	digitalWrite(authPinRPI, LOW);
}
