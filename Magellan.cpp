/*
  Magellan v1.55 NB-IoT Playgroud Platform .
  support Quectel BC95
  NB-IoT with AT command

  Develop with coap protocol
  support post and get method
  only Magellan IoT Platform
  https://www.aismagellan.io

*** if you use uno it limit 100
    character for payload string

    example
      payload = {"temperature":25.5}
      the payload is contain 20 character

  Author:   Phisanupong Meepragob
  Create Date:     1 May 2017.
  Modified: 2 May 2018.

  (*bug fix can not get response form buffer but can send data to the server)
  (*bug fix get method retransmit message)
  Released for private use.
*/

#include "Magellan.h"

const char serverIP[] = "103.20.205.85";
char *pathauth;

unsigned int Msg_ID = 0;

#define LOGGING 1

//################### Buffer #######################
String input;
//################### counter value ################
byte k = 0;
//################## flag ##########################
bool end = false;
bool send_NSOMI = false;
bool created = false;
bool connected = false;
bool success = false;
//################### Parameter ####################
unsigned int auth_len = 0;
unsigned int path_hex_len = 0;
//-------------------- timer ----------------------
unsigned long previous = 0;

void event_null(char *data) {}
Magellan::Magellan()
{
  Event_debug =  event_null;
}

/*-------------------------------
    Check NB Network connection
    Author: Device innovation Team
  -------------------------------
*/
bool Magellan:: getNBConnect()
{
  _serial_flush();
  _Serial->println(F("AT+CGATT?"));
  AIS_BC95_RES res = wait_rx_bc(500, F("+CGATT"));
  if (res.status)
  {
    if (res.data.indexOf(F("+CGATT:0")) != -1)
      return false;
    if (res.data.indexOf(F("+CGATT:1")) != -1)
      return true;
  }
  return false;
}

/*-------------------------------
  Check response from BC95
  Author: Device innovation Team
  -------------------------------
*/
AIS_BC95_RES Magellan:: wait_rx_bc(long tout, String str_wait)
{
  unsigned long pv_ok = millis();
  unsigned long current_ok = millis();
  unsigned char flag_out = 1;
  unsigned char res = -1;
  AIS_BC95_RES res_;
  res_.temp = "";
  res_.data = "";

  while (flag_out)
  {
    if (_Serial->available())
    {
      input = _Serial->readStringUntil('\n');
      //if (debug) Serial.println(input);
      res_.temp += input;
      if (input.indexOf(str_wait) != -1)
      {
        res = 1;
        flag_out = 0;
      }
      else if (input.indexOf(F("ERROR")) != -1)
      {
        res = 0;
        flag_out = 0;
      }
    }
    current_ok = millis();
    if (current_ok - pv_ok >= tout)
    {
      flag_out = 0;
      res = 0;
      pv_ok = current_ok;
    }
  }

  res_.status = res;
  res_.data = input;
  input = "";
  return (res_);
}

bool Magellan::sendcmd(String cmd, String ret = "OK", int timeout = 2000, int trytime = 3) {
  byte retry = 0;
  AIS_BC95_RES res;

  do {
    _serial_flush();
    _Serial->println(cmd);
    res = wait_rx_bc(timeout, ret);
    if (res.status)
    {
#if LOGGING
      Serial.println(res.data);
#endif
      return true;
    } else {
      retry++;
      if (retry >= trytime) {
        return false;
      }
      delay(500);
    }
  } while (!res.status);

  delay(500);
  _serial_flush();

  return false;
}

bool Magellan::check_module()
{
  delay(2000);
  return sendcmd(F("AT"));
}

/*------------------------------
  Connect to NB-IoT Network
              &
  Initial Magellan
  ------------------------------
*/
bool Magellan::begin(char *auth, Stream *serial)
{
  created = false;
  _Serial = serial;
  //_Serial->begin(9600);
  _serial_flush();

  Serial.println("Initial Magellan");

#if LOGGING
  Serial.println(F("############## Welcome to NB-IoT network ###############"));
#endif
  /*---------------------------------------
      Initial BC95 Module
    ---------------------------------------
  */
  AIS_BC95_RES res;

  delay(5000);
#if LOGGING
  Serial.println(F(">>Pre Test AT command"));
#endif
  check_module();
#if LOGGING
  Serial.println(F(">>Set to auto register on network"));
#endif
  sendcmd(F("AT+NCONFIG=AUTOCONNECT,TRUE"));
#if LOGGING
  Serial.println(F(">>Power on CFUN"));
#endif
  sendcmd(F("AT+CFUN=1"));
#if LOGGING
  Serial.println(F(">>Disable eDRX function of the module"));
#endif
  sendcmd(F("AT+CEDRXS=0,5"));
#if LOGGING
  Serial.println(F(">>Enable the use of PSM"));
#endif
  sendcmd(F("AT+CPSMS=1"));
#if LOGGING
  Serial.println(F(">>Connecting to network"));
#endif
  sendcmd(F("AT+CGATT=1"));
#if LOGGING
  Serial.println(F(">>Reboot Module ...."));
#endif
  sendcmd(F("AT+NRB"));
  delay(5000);
#if LOGGING
  Serial.println(F(">>Pre Test AT command"));
#endif
  check_module();

  Serial.print("Connecting");

  /*--------------------------------------
      Check network connection
    --------------------------------------
  */
  int retry = 0;
  int timeout = 0;
  while (!connected) {
    connected = getNBConnect();
    Serial.print(F("."));
    timeout++;

    //Execute AT+CGATT? after 300s, "+CGATT:1" is not returned?
    if (timeout >= 150) {

      timeout = 0;
      retry++;

      //Failed after 3 times of retries according to the back off algorithm.
      if (retry >= 3) {
        //power off module
        return created;
      }
#if LOGGING
      Serial.println(F(">>Reboot Module ...."));
#endif
      sendcmd(F("AT+NRB"));
      delay(5000);
#if LOGGING
      Serial.println(F(">>Pre Test AT command"));
#endif
      check_module();
#if LOGGING
      Serial.println(F(">>Power off CFUN"));
#endif
      sendcmd(F("AT+CFUN=0"));
#if LOGGING
      Serial.println(F(">>Clear stored EARFCN values"));
#endif
      sendcmd(F("AT+NCSEARFCN"));
#if LOGGING
      Serial.println(F(">>Power on CFUN"));
#endif
      sendcmd(F("AT+CFUN=1"));
#if LOGGING
      Serial.println(F(">>Connecting to network"));
#endif
      sendcmd(F("AT+CGATT=1"));

    }
    delay(2000);
  }

  /*-------------------------------------
      Create network socket
    -------------------------------------
  */
#if LOGGING
  Serial.println(F(">>Create socket"));
#endif
  sendcmd(F("AT+NSOCR=DGRAM,17,5684,1"));

  Serial.println("Connected");

  Serial.print(F("RSSI "));
  Serial.print(rssi());
  Serial.println(F(" dBm"));

  auth_len = 0;
  pathauth = auth;
  cnt_auth_len(pathauth);

  if (auth_len > 13) {
    path_hex_len = 2;
  }
  else {
    path_hex_len = 1;
  }

  previous = millis();
  created = true;

   Serial.println("Magellan ready");

  return created;
}

/*------------------------------
    CoAP Message menagement
  ------------------------------
*/
void Magellan::printHEX(char *str)
{
  char *hstr;
  hstr = str;
  char out[3] = "";
  int i = 0;
  bool flag = false;
  while (*hstr)
  {
    flag = itoa((int) * hstr, out, 16);

    if (flag)
    {
      _Serial->print(out);
#if LOGGING
      if (debug)
      {
        Serial.print(out);
      }
#endif
    }
    hstr++;
  }
}

void Magellan::printmsgID(unsigned int messageID)
{
  char Msg_ID[3];

  utoa(highByte(messageID), Msg_ID, 16);
  if (highByte(messageID) < 16)
  {
#if LOGGING
    if (debug) Serial.print(F("0"));
#endif
    _Serial->print(F("0"));
#if LOGGING
    if (debug) Serial.print(Msg_ID);
#endif
    _Serial->print(Msg_ID);
  }
  else
  {
    _Serial->print(Msg_ID);
#if LOGGING
    if (debug)  Serial.print(Msg_ID);
#endif
  }

  utoa(lowByte(messageID), Msg_ID, 16);
  if (lowByte(messageID) < 16)
  {
#if LOGGING
    if (debug)  Serial.print(F("0"));
#endif
    _Serial->print(F("0"));
#if LOGGING
    if (debug)  Serial.print(Msg_ID);
#endif
    _Serial->print(Msg_ID);
  }
  else
  {
    _Serial->print(Msg_ID);
#if LOGGING
    if (debug)  Serial.print(Msg_ID);
#endif
  }
}

void Magellan::print_pathlen(unsigned int path_len, char *init_str)
{
  unsigned int extend_len = 0;

  if (path_len > 13) {
    extend_len = path_len - 13;

    char extend_L[3];
    itoa(lowByte(extend_len), extend_L, 16);
    _Serial->print(init_str);
    _Serial->print(F("d"));

#if LOGGING
    if (debug) Serial.print(init_str);
#endif
#if LOGGING
    if (debug) Serial.print(F("d"));
#endif

    if (extend_len <= 15) {
      _Serial->print(F("0"));
      _Serial->print(extend_L);

#if LOGGING
      if (debug) Serial.print(F("0"));
#endif
#if LOGGING
      if (debug) Serial.print(extend_L);
#endif
    }
    else {
      _Serial->print(extend_L);
#if LOGGING
      if (debug) Serial.print(extend_L);
#endif
    }


  }
  else {
    if (path_len <= 9)
    {
      char hexpath_len[2] = "";
      sprintf(hexpath_len, "%i", path_len);
      _Serial->print(init_str);
      _Serial->print(hexpath_len);
#if LOGGING
      if (debug) Serial.print(init_str);
#endif
#if LOGGING
      if (debug) Serial.print(hexpath_len);
#endif

    }
    else
    {
      if (path_len == 10)
      {
        _Serial->print(init_str);
        _Serial->print(F("a"));
#if LOGGING
        if (debug) Serial.print(init_str);
#endif
#if LOGGING
        if (debug) Serial.print(F("a"));
#endif
      }
      if (path_len == 11)
      {
        _Serial->print(init_str);
        _Serial->print(F("b"));
#if LOGGING
        if (debug) Serial.print(init_str);
#endif
#if LOGGING
        if (debug) Serial.print(F("b"));
#endif
      }
      if (path_len == 12)
      {
        _Serial->print(init_str);
        _Serial->print(F("c"));
#if LOGGING
        if (debug) Serial.print(init_str);
#endif
#if LOGGING
        if (debug) Serial.print(F("c"));
#endif
      }
      if (path_len == 13)
      {
        _Serial->print(init_str);
        _Serial->print(F("d"));
#if LOGGING
        if (debug) Serial.print(init_str);
#endif
#if LOGGING
        if (debug) Serial.print(F("d"));
#endif
      }

    }
  }
}

void Magellan::cnt_auth_len(char *auth)
{
  char *hstr;
  hstr = auth;
  char out[3] = "";
  int i = 0;
  bool flag = false;
  while (*hstr)
  {
    auth_len++;
    hstr++;
  }
}

/*-----------------------------
    CoAP Method POST
  -----------------------------
*/
bool Magellan::post(String payload)
{
  success = false;
  unsigned long prev_send = millis();

  if (payload.length() > 100) {
#if LOGGING
    Serial.println("Payload oversize");
#endif
    return false;
  }

  //Fire Fire Fire
  send(payload);
  send(payload);
  send(payload);

  while (!success) {
    waitResponse();
    unsigned long cur_send = millis();
    if (cur_send - prev_send > 10000)
    {
#if LOGGING
      if (printstate) Serial.println(F("Ack timeout"));
#endif
      return false;
    }
  }

  return true;
}

/*-----------------------------
    Massage Package
  -----------------------------
*/
void Magellan::send(String payload)
{
  byte cout = 0;
  AIS_BC95_RES res;

  do {
    //Load new payload for send

#if LOGGING
    if (printstate) Serial.print(F("Load new payload size:"));
    if (printstate) Serial.println(payload.length());
#endif
    char data[payload.length() + 1] = "";
    payload.toCharArray(data, payload.length() + 1);

    if (printstate) Serial.print(F("<<<--- post data : Msg_ID "));
    if (printstate) Serial.print(Msg_ID);
    if (printstate) Serial.print(" ");
    if (printstate) Serial.print(payload);

    _serial_flush();
    _Serial->print(F("AT+NSOST=0,"));
    _Serial->print(serverIP);
    _Serial->print(F(",5683,"));
#if LOGGING
    if (debug) Serial.print(F("AT+NSOST=0,"));
    if (debug) Serial.print(serverIP);
    if (debug) Serial.print(F(",5683,"));

    if (debug) Serial.print(auth_len + 11 + path_hex_len + payload.length());
#endif
    _Serial->print(auth_len + 11 + path_hex_len + payload.length());
#if LOGGING
    if (debug) Serial.print(F(",4002"));
#endif
    _Serial->print(F(",4002"));
    printmsgID(Msg_ID);
    _Serial->print(F("b54e42496f54"));
#if LOGGING
    if (debug) Serial.print(F("b54e42496f54"));
#endif
    print_pathlen(auth_len, "0");

    printHEX(pathauth);
    _Serial->print(F("ff"));
#if LOGGING
    if (debug) Serial.print(F("ff"));
#endif
    printHEX(data);
    _Serial->println();

    if (printstate) Serial.println();

    res = wait_rx_bc(1000, F("OK"));
    if (res.status)
    {
#if LOGGING
      Serial.println(res.data);
#endif
      return;
    } else {
      cout++;
      if (cout > 3) {
#if LOGGING
        if (printstate) Serial.println(F("Send cmd fail"));
#endif
        return;
      }
      delay(500);
    }
  } while (!res.status);
}

void Magellan::_serial_flush()
{
  while (_Serial->available())_Serial->read();
}

void Magellan:: waitResponse()
{
  unsigned long current = millis();
  if ((current - previous >= 500) && !(_Serial->available()))
  {
    _Serial->println(F("AT+NSORF=0,512"));
    previous = current;
  }

  if (_Serial->available())
  {
    char data = (char)_Serial->read();
    if (data == '\n' || data == '\r')
    {
      if (k > 2)
      {
        end = true;
        k = 0;
      }
      k++;
    }
    else
    {
      input += data;
    }
#if LOGGING
    if (debug) Serial.println(input);
#endif
  }
  if (end) {

    if (input.indexOf(F("+NSONMI:")) != -1)
    {
#if LOGGING
      if (debug) Serial.print(F("send_NSOMI "));
      if (debug) Serial.println(input);
#endif
      if (input.indexOf(F("+NSONMI:")) != -1)
      {
#if LOGGING
        if (debug) Serial.print(F("found NSONMI "));
#endif
        _Serial->println(F("AT+NSORF=0,512"));
        input = F("");
        send_NSOMI = true;
#if LOGGING
        if (printstate) Serial.println();
#endif
      }
      end = false;

    }
    else
    {
#if LOGGING
      if (debug) Serial.print(F("--->>> Get buffer: "));
      if (debug) Serial.println(input);
#endif
      end = false;

      if (input.indexOf(F("0,103.20.205.85")) != -1)
      {
        int index1 = input.indexOf(F(","));
        if (index1 != -1)
        {
          int index2 = input.indexOf(F(","), index1 + 1);
          index1 = input.indexOf(F(","), index2 + 1);
          index2 = input.indexOf(F(","), index1 + 1);
          index1 = input.indexOf(F(","), index2 + 1);

          Serial.print(F("--->>>  Response: "));
          Serial.println(input.substring(index2 + 1, index1));

          success = true;
        }
      }
      else if ((input.indexOf(F("ERROR")) != -1))
      {
        if ((input.indexOf(F("+CME ERROR"))) != -1)
        {
          int index1 = input.indexOf(F(":"));
          if (index1 != -1) {
            String LastError = input.substring(index1 + 1, input.length());

            if ((LastError.indexOf(F("159"))) != -1)
            {
#if LOGGING
              Serial.print(F("Uplink busy"));
#endif
            }
          }
        }
      }

      send_NSOMI = false;
      input = F("");
    }
  }
}

String Magellan:: rssi()
{
  delay(200);
  int rssi = 0;
  String data_csq = "";
  String data_input = "";

  _serial_flush();
  _Serial->println(F("AT+CSQ"));
  delay(500);
  while (1)
  {
    if (_Serial->available())
    {
      data_input = _Serial->readStringUntil('\n');
      //Serial.println(data_input);
      if (data_input.indexOf(F("OK")) != -1)
      {
        break;
      }
      else
      {
        if (data_input.indexOf(F("+CSQ")) != -1)
        {
          int start_index = data_input.indexOf(F(":"));
          int stop_index  = data_input.indexOf(F(","));
          data_csq = data_input.substring(start_index + 1, stop_index);
          if (data_csq == "99")
          {
            data_csq = "N/A";
          }
          else
          {
            rssi = data_csq.toInt();
            rssi = (2 * rssi) - 113;
            data_csq = String(rssi);
          }
        }
      }
    }
  }
  return data_csq;
}

bool Magellan:: pingIP(String IP)
{
  String data = "";

  _serial_flush();
  _Serial->println("AT+NPING=" + IP);
  AIS_BC95_RES res = wait_rx_bc(3000, F("+NPING"));

  if (res.status)
  {
    data = res.data;
    _Serial->println(data);
    int index = data.indexOf(F(":"));
    int index2 = data.indexOf(F(","));
    int index3 = data.indexOf(F(","), index2 + 1);
#if LOGGING
    Serial.println("# Ping Success");
    Serial.println("# Ping IP:" + data.substring(index + 1, index2) + ",ttl= " + data.substring(index2 + 1, index3) + ",rtt= " + data.substring(index3 + 1, data.length()));
#endif
    return true;
  } else {
#if LOGGING
    if (debug) Serial.println("# Ping Failed");
#endif
  }
  res = wait_rx_bc(500, F("OK"));
  return false;
}
