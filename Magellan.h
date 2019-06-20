#ifndef Magellan_h
#define Magellan_h

#include <Arduino.h>
#include <Stream.h>

struct AIS_BC95_RES
{
  char status;
  String data;
  String temp;
};

class Magellan
{
  public:
    Magellan();

    bool debug;
    bool default_server;
    bool printstate = true;

    void (*Event_debug)(char *data);
    bool getNBConnect();
    String rssi();

    //################# Magellan Platform ########################
    bool begin(char *auth, Stream *serial);
    void printHEX(char *str);
    void printmsgID(unsigned int messageID);
    void cnt_auth_len(char *auth);
    bool post(String payload);
    void send(String payload);
    void waitResponse();
    void _serial_flush();
    bool pingIP(String IP);
    bool check_module();
  private:

    AIS_BC95_RES  wait_rx_bc(long tout, String str_wait);
    void print(char *str);
    void print_pathlen(unsigned int path_len, char *init_str);
    bool sendcmd(String cmd, String ret, int timeout = 2000, int trytime = 3);

  protected:
    Stream *_Serial;
};


#endif
