
/*
   Michael John Watters
   B00751280

   This Code runs the LCD display via the Ardino Nano V3.0 using I2C.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins on the Nano.
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

void setup() {
  Serial.begin(9600);  // Sets the serial rate.
  lcd.begin(16, 2);    // Number of columns and rows.
  lcd.setCursor(0, 0); // Set cursor to the start 0,0 is the top left.
}

// Declare the last displayed String
String lastDisplayedString;

// Define terminator signal and new line signal.
const char terminator = '^';
const char newLineIndicator = '|';

// Simple loop used to read Serial Communication.
// Reads a string until it finds the terminator char '^'.
void loop() {

  // Read the String until terminator
  String readString = Serial.readStringUntil(terminator);

  // log the read String to the terminal
  Serial.println("Serial Read: <" + readString + ">");

  // if readString is different and not an empty string
  // clear the LCD and write the new string to the LCD.
  // then set the lastDisplayedString.
  if (readString != lastDisplayedString && readString != "") {

    //Clear the LCD, as we have new string to display.
    lcd.clear();

    // Find where to break the string into two lines using index or -1 if not found.
    int newLineMarker = readString.indexOf(newLineIndicator);

    //Spilt the string into two rows for display to the LCD.
    String row_one, row_two;

    if (newLineMarker != -1) {
      row_one = readString.substring(0, newLineMarker);
      
      //Check for the presents of a newLineIndicator char.
      row_two = readString.substring(newLineMarker + 1, readString.length());
    } else {
      row_one = readString;
      row_two = "";
    }

    // Print Row One to the LCD
    lcd.setCursor(0, 0); // set the cursor to the 2nd line.
    lcd.print(row_one); // write the row.

    // Print Row Two to the LCD
    lcd.setCursor(0, 1); //set the cursor to the 2nd line.
    lcd.print(row_two);  //write the row.

    lastDisplayedString = readString; // set the lastDisplayedString
  }
}
