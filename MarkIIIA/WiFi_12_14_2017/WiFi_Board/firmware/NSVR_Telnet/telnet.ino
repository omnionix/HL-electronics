/*  Embedis - Telnet Interface
    Copyright (C) 2015, 2016 PatternAgents, LLC
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
// telnet : client & server
//          depending on the Embedis "wifi_mode" key, 
//          either a telnet server (ap) or telnet client (sta) will be run.
//          A client instance will connect to exactly one server instace.
//
// Usage :
// Call setup_telnet() from your main setup() function.
// Call loop_telnet() from your main loop() function.

// CAUTION: Password authentication isn't very secure.
//          Credentials are easily guessed, and sent in clear text.

#include <WiFiServer.h>
#include <WiFiClient.h>
#include "Embedis.h"

WiFiServer server23(23);
WiFiClient server23Client;
//WiFiClient client;
//Embedis embedis23(server23Client);

void setup_telnet() 
{
   String mode = setting_wifi_mode();  
   if (mode == "ap") {
      // default is access point or "ap"
      // start the Telnet Server
      server23 = WiFiServer(23);
      server23.begin();
      server23.setNoDelay(true);
      LOG( String() + F("[ Embedis : Starting Telnet Server ]") );    
   } else {
      // station "sta" will be the Telnet Client
      // nothing to setup for the Telnet Client yet.
      LOG( String() + F("[ Embedis : Starting Telnet Client ]") );
   }
}

void doDataTransfer() {
  if (Serial.available() > 0) {
   // server23Client.write(12);
    while (Serial.available() > 0) {
      server23Client.write(Serial.read());
    }  
  }



  int avail = server23Client.available();
  if (avail > 0){
    //server23Client.write(6);
    //read up to 64 bytes from wifi
    uint8_t buffer[64];
    server23Client.read(buffer, avail);


    Serial.write(buffer, avail);
  }
}
void loop_telnet_server() 
{
    String temp_telnet_passphrase = setting_default_passphrase();
    static int eat = 0;
    static int auth = 0;

    // new connections
    if (server23.hasClient()) {
        if (!server23Client.connected()) {
            server23Client.stop();
            server23Client = server23.available();
            //embedis23.reset(true);
            eat = 0;
            auth = -2;
        } else {
            server23.available().stop();
        }
    }

    int ch;

    // discard negotiation from the client
    while (eat >= 0 || auth >= 0) {
        int peek = server23Client.peek();
        if (peek < 0) break;
        if (peek == 255) {
            server23Client.read();
            eat = 2;
            continue;
        }
        if (eat > 0 && eat <= 3) {
            ch = server23Client.read();
            if (--eat==1) {
                if (ch == 250) eat = 250; // SB
                if (ch == 240) eat = 0;   // SE
            }
            continue;
        }
        if (eat == 250 || peek == 0 || peek == 10) {
            server23Client.read();
            continue;
        }
        eat = -1;
        break;
    }

    switch(auth) {
    case -99:
        // Logged in
        doDataTransfer();
        break;
    case -2:
        server23Client.write(255); // IAB
        server23Client.write(253); // DO
        server23Client.write(34);  // LINEMODE
        server23Client.write(255); // IAB
        server23Client.write(250); // SB
        server23Client.write(34);  // LINEMODE
        server23Client.write(1);   // MODE: EDIT
        server23Client.write(3);   // DEFAULT MASK
        server23Client.write(255); // IAB
        server23Client.write(240); // SE
        server23Client.write(255); // IAB
        server23Client.write(251); // WILL
        server23Client.write(1);   // ECHO
        //nobreak
    case -1:
        server23Client.print("Password:");
        //temp_telnet_passphrase = setting_login_passphrase();
        auth = 0;
        return;
    default:
        if (eat >= 0) return;
        ch = server23Client.read();
        if (ch < 0) break;
        if (ch == 13) {
            server23Client.println("");
            if (auth == temp_telnet_passphrase.length()) {
                server23Client.write(255); // IAB
                server23Client.write(252); // WONT
                server23Client.write(1);   // ECHO
                auth = -99;
                temp_telnet_passphrase = "";
                server23Client.println("Logged in.");
            } else {
                auth = -1;
            }
            eat = 0;
            break;
        }
        if (auth >= 0 && temp_telnet_passphrase[auth] == ch) {
            auth++;
            break;
        }
        // Failed password. Stay in default until CR.
        auth = -3;
        break;
    }

}

void loop_telnet_client() 
{
    String temp_telnet_passphrase;
    static int client_state = 0;
    yield();
    // run the Telnet Client
    switch(client_state) {
    case 0:
        // connected yet?
        if (WiFi.status() == WL_CONNECTED) {
          LOG( String() + F("[ Embedis : Starting Telnet Client ]") );
          client_state++;
        }
        break;
    case 1:
        if (server23Client.connect("192.168.4.1", 23)) {
          LOG( String() + F("[ Embedis : Connected to Telnet Server ]") );
          client_state++;
        } else {
          LOG( String() + F("[ Embedis : !!! Failed to Connect with Telnet Server !!! ]") );
          client_state = 0;
        }
        break;
    case 2:
        // send the password
        delay(500);
        temp_telnet_passphrase = setting_sta_passphrase();
        server23Client.println(temp_telnet_passphrase);
        client_state++;
        break;
    case 3:
        // check for login message
        client_state++;
        break;
    case 4:
        client_state++;
        break;           
    case 5:
        /*
        // if the server's disconnected, stop the client:
        if (!client.connected()) {
          LOG( String() + F("[ Embedis : !!! Lost Connection with Telnet Server !!! ]") );
          client.stop();
          client_state = 0;
          break;
        } else {
        */
          // if there are incoming bytes available
          // from the server, read them and print them:
          while (server23Client.available() > 0) {
            Serial.write(server23Client.read());
          }
          // as long as there are bytes in the serial queue,
          // read them and send them out the socket if it's open:
          while (Serial.available() > 0) {
            char inChar = Serial.read();
            if (server23Client.connected()) {
              server23Client.write(inChar);
            }
          }
          /*
        }
        */
        break;        
    default:
        // shouldn't get here, reset the state machine
        client_state = 0;
        break;
    }
}

