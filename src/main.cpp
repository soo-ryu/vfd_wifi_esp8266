#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <vfd_20T201DA2.h>

// WiFi credentials
const char *ssid = "ELECTRO";
const char *password = "electroelectro123";

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200);

// VFD Pins
const int CLK = 2;
const int DATA = 4;
const int RST = 5;

vfd_20T201DA2 display(CLK, DATA, RST);

// Web Server
WiFiServer server(80);
String header = "";
String userText = ""; // stores user-submitted text

unsigned long timeoutTime = 2000;

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();

  display.begin(20, 2);
  display.clear();
  display.writeText("Waiting...");
  server.begin();
}

String urlDecode(String input)
{
  String decoded = "";
  char temp[] = "0x00";
  unsigned int len = input.length();
  unsigned int i = 0;

  while (i < len)
  {
    char c = input.charAt(i);
    if (c == '+')
    {
      decoded += ' ';
    }
    else if (c == '%')
    {
      if (i + 2 < len)
      {
        temp[2] = input.charAt(i + 1);
        temp[3] = input.charAt(i + 2);
        decoded += (char)strtol(temp, NULL, 16);
        i += 2;
      }
    }
    else
    {
      decoded += c;
    }
    i++;
  }
  return decoded;
}

void loop()
{
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();
  String weekDays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  String months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime((time_t *)&epochTime);

  int day = ptm->tm_mday;
  int month = ptm->tm_mon;
  int year = ptm->tm_year + 1900;
  int wday = ptm->tm_wday;

  String dateStr = weekDays[wday] + " " + String(day) + " " + months[month];

  // Update VFD every second
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000)
  {
    lastUpdate = millis();
    display.clear();
    display.setCursorPos(0, 0);
    display.writeText(formattedTime + " " + dateStr); // Line 1
    display.setCursorPos(0, 1);
    display.writeText(userText); // Line 2 (user input)
  }

  WiFiClient client = server.accept();
  if (client)
  {
    Serial.println("New Client.");
    String currentLine = "";
    unsigned long currentTime = millis();
    unsigned long previousTime = currentTime;

    while (client.connected() && currentTime - previousTime <= timeoutTime)
    {
      currentTime = millis();
      if (client.available())
      {
        char c = client.read();
        header += c;

        if (c == '\n')
        {
          if (currentLine.length() == 0)
          {
            if (header.indexOf("GET /display?text=") >= 0)
            {
              int startIndex = header.indexOf("GET /display?text=") + strlen("GET /display?text=");
              int endIndex = header.indexOf(" HTTP/");
              String rawText = header.substring(startIndex, endIndex);
              userText = urlDecode(rawText);
              if (userText.length() > 20)
                userText = userText.substring(0, 20); // limit to one line
              Serial.println("User text: " + userText);
            }

            // Webpage Response
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            client.println("<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<style>html{font-family: Helvetica; text-align: center; margin-top: 50px;}</style></head>");
            client.println("<body><h2>Send Text to VFD</h2>");
            client.println("<form action=\"/display\" method=\"GET\">");
            client.println("<input type=\"text\" name=\"text\" maxlength=\"40\" style=\"font-size:20px; width:60%;\" required>");
            client.println("<br><br><input type=\"submit\" value=\"Send\" style=\"font-size:20px;\"></form>");
            client.println("<p>Current text: " + userText + "</p>");
            client.println("</body></html>");
            client.println();
            break;
          }
          else
          {
            currentLine = "";
          }
        }
        else if (c != '\r')
        {
          currentLine += c;
        }
      }
    }

    header = "";
    client.stop();
    Serial.println("Client disconnected.");
  }
}
