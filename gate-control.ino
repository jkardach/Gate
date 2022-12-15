/*
copyright Forkbeard Labs

This application is used to control a household gate. 

Particle Functions:
Open - issues an open gate function which will cause the gate to completely open
    * uses D1 as the open signal.  Normally LOW signal, logic HIGH for 100 ms 
      will cause the gate to open. 
Close - issues a close gate function which will cause the gate to completely 
      close
    * uses D2 as the close signal.  Normally LOW signal, logic HIGH for 100ms 
      will cause the gate to close. 
Stop - issues a stop gate function which will stop the gate from opening or 
       closing.
    * uses D3 as a stop gate function.  Normally HIGH signal, logic LOW for 
      100ms will cause the gate to stop
    
** Next functions are logical, don't use hardware but uses the Open, Close and 
   Stop functions to control the gate.
pOpen - issues a partial open command to the gate.  
    a.  Open gate command
    b.  Wait "pOpen_Time" seconds
    c.  Stop gate command
    d.  wait "rOpen_Time" seconds
    e.  Close gate command 
    f.  wait "pOpen_Time" seconds

Particle function:
gate - takes a string "open", "close", "stop", or "popen".

Particle Variables:
IP Address - WLAN IP address controller is connected to.

openClose_Time - integer used in the open/close command to define the number 
of seconds the open or close command run to open/close gate (completely)
pOpen_Time - integer used in the pOpen command to define the number of seconds 
   for open command to run before issuing a stop
rOpen_Time - integer used in the pOpen command to wait before closing the gate 
   after it has been partially open.
pulse_Time - number of ms for the pulse to open, close or stop the gate.

WebPage - using Webduino

The HTML/CSS/Java script is loaded right from the ROM using the P macro to save space.  There
are four range sliders used to pick the timing for opening and closing the gate and these
are all contained in a struct.  The default values (which affect the HTML slider position and
value label) are loaded from the C++ side and sent directly to the webpage.  When these values 
are changed by the slider, the C++ side will write theses to EEPROM and the next power cycle 
the saved default values are used.  

The the gate timing within the JavaScript/HTML and C++ code are independent as communications from
the server to client side is limited.  The C++ timers seem to be slower than the HTML timers, I'll
try to debug this.

*/

//#include <MDNS.h>
#include <WebDuino.h>

//#define DEBUG 1     //  remove to remove debug code

using namespace std;
#define VERSION "V0.1"

// no-cost stream operator as described at 
// http://sundial.org/arduino/?page_id=119
template<class T>
inline Print &operator <<(Print &obj, T arg)
{ obj.print(arg); return obj; }

#define PREFIX "/gate"

#define OPEN_CLOSE_MIN 10   // minimum range to OpenClose gate
#define OPEN_CLOSE_MAX 60   // maximum range to OpenClose gate
#define POPEN_MIN 1         // minimum range to partially open gate
#define POPEN_MAX 25        // maximum range to partially open gate
#define ROPEN_MIN 10        // minimum range for gate to remain open
#define ROPEN_MAX 120       // maximum range for gate to remain open
#define PULSE_MIN 50        // minimum range for gate button pulse
#define PULSE_MAX 1000      // maximum range for gate button pulse

//variables for timers
// int openClose_Time = 15;    // Time it takes to open or close the gate (openClose)
// int pOpen_Time = 3;         // Time it takes to partially open the gate (pOpen)
// int rOpen_Time = 25;        // time gate is left open for a pOpen (rOpen)
// int pulse = 100;            // default pulse width to open, close or stop gate (pulse)
String myIP = "";

struct GateTimes {
    int openClose_Time;
    int pOpen_Time;
    int rOpen_Time;
    int pulse;
};

GateTimes gateTimes = { 15, 3, 25, 100 }; // openClose_Time, pOpen_Time, rOpen_Time, pulse

WebServer webserver(PREFIX, 80);    // webserver object listening on port 80, PREFIX index

// default webserver command, will display the webpage
void defaultCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete) {
    // process POSTs
    if(type == WebServer::POST) {
        bool repeat;
        char name[16], value[16];
        do {
            repeat = server.readPOSTparam(name, 16, value, 16);
            // if(repeat) { Serial.printf("name: %s, value: %s\n", name, value); }
            
            if(strcmp(name, "openClose") == 0) {
                gateTimes.openClose_Time = strtoul(value, NULL, 10);
                EEPROM.put(10, gateTimes);  // write gateTimes to EEPROM
                setOpenTime();
            }
            if(strcmp(name, "pOpen") == 0) {
                gateTimes.pOpen_Time = strtoul(value, NULL, 10);
                EEPROM.put(10, gateTimes);  // write gateTimes to EEPROM
                setPopenTime();
            }
            if(strcmp(name, "rOpen") == 0) {
                gateTimes.rOpen_Time = strtoul(value, NULL, 10);
                EEPROM.put(10, gateTimes);  // write gateTimes to EEPROM
                setRopenTime();
            }
            if(strcmp(name, "pulse") == 0) {
                gateTimes.pulse = strtoul(value, NULL, 10);
                EEPROM.put(10, gateTimes);  // write gateTimes to EEPROM
            }
            if(strcmp(name, "command") == 0) {
                if(strcmp(value, "Open") == 0) { open(); }
                if(strcmp(value, "Close") == 0) { close(); }
                if(strcmp(value, "Stop") == 0) { stop(); }
                if(strcmp(value, "Partial Open") == 0) { pOpen(); }
            }
        } while (repeat);
        server.httpSeeOther(PREFIX);  // not sure I want to reload the page
        return;
    }
    
    // process get command
    server.httpSuccess();       // for GET or HEAD send "it's all OK headers"
    
    if(type == WebServer::GET) {
    // store HTML in program memory using P macro
    P(webPage1) = R""""(
    <html lang="en" dir="ltr">
      <head>
        <meta charset="utf-8">
        <meta name="apple-mobile-web-app-capable" content="yes">
        <meta http-equiv="X-UA-Compatible" content="IE=edge,chrome=1">
        <meta name="viewport" content="width=device-width, user-scalable=no">
        <title>Nacht Gate</title>
        <style media="screen">
          body {
            width: auto;
            background-color: #F8CB2E;
            color: blue;
          }
          .nacht {
            font-size : 20px;
            height: 40px;
            width: 100%;
            padding-left: 2px;
            padding-right: 2px;
            margin-left: auto;
            margin-right: auto;
          }
          #gateStatus {
            background-color: blue;
          }
          input {
            width: 90%;
            background-color: green;
            color: white;
            margin: 2px auto 2px auto;
          }
          td {
            padding-left: 4px;
            padding-right: 4px;
          }
          .slideValues {
            color: black;
          }
          hr {
            width: 30%;
            margin: 25px auto 25px auto;
            <!-- align: center;
            border-style: none none dotted;
            border-width: 5px;
            width: 5%; -->
          }
        </style>
      </head>
      <body>
        <h1 style="text-align:center">Nachtsheim Gate Control</h1>
        <form class="" action="index.html" method="post">
          <input type="button" id=gateStatus class="nacht" value="Gate Closed" disabled><br>
          <input type="button" id=gateButton class="nacht" value="Open"><br>
          <input type="button" id=stopButton class="nacht" value="Stop" disabled><br>
          <input type="button" id=pOpenButton class="nacht" value="Partial Open"><br>
          <hr>
          <table>
            <tr>
              <td><label>10</label>
    )"""";          
    P(webPage2) = R""""(
              <td><label>60</label>
              <td><label>Open/Close</label>
    )"""";
    P(webPage3) = R""""(
            </tr>
            <tr>
              <td><label>1</label></td>
    )"""";
    P(webPage4) = R""""(
              <td><label>25</label></td>
              <td><label>Partial Open</label></td>
    )"""";          
    P(webPage5) = R""""(
            </tr>
            <tr>
              <td><label>10</label></td>
    )"""";
    P(webPage6) = R""""(
              <td><label>120</label></td>
              <td><label>Remain Open</label></td>
    )"""";
    P(webPage7) = R""""(
            </tr>
            <tr>
              <td><label>50</label></td>
    )"""";
    P(webPage8) = R""""(
              <td><label>1000</label></td>
              <td><label>Pulse </label></td>
    )"""";
    P(webPage9) = R""""(
            </tr>
          </table>
        </form>
        <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.6.1/jquery.min.js"></script>
      </body>
      <script type="text/javascript">
        var next = 0;
        var timerID = 0;
        const gateBusyStatus = ['Gate Opening', 'Gate Closing', 'Gate Remaining Open'];
    )""""
    "function stopButtonPressed() {"
    R""""(
        $('#gateStatus').val('Gate Opened');
        $('#gateButton').val('Close');
        $('#gateButton').css('background-color', 'green');
        $('#gateButton').attr('disabled', false);
        $('#pOpenButton').val('Partial Open');
        $('#pOpenButton').css('background-color', 'green');
        $('#pOpenButton').attr('disabled', true);
        $('#stopButton').attr('disabled', true);
        console.log("$.post('/Gate', { 'Stop' });");
        }
    )""""
    "function pOpenNext(state, status, color) {"
    R""""(    
        switch(next) {
            case 0:
                pOpen('Opening', 'Gate Opening', 'red');  
                break;
            case 1:
                timerID = setTimeout(function() { pOpen('Remaining Open', 'Gate Remaining Open', 'red'); }, $('#pOpen').val() + '000');
                break;
            case 2:
                timerID = setTimeout(function() { pOpen('Closing', 'Gate Closing', 'red'); }, $('#rOpen').val() + '000');
                break;
            case 3:
                timerID = setTimeout(function() { pOpen('Partial Open', 'Gate Closed', 'green'); }, $('#pOpen').val() + '000');
                break;
            default:
                $('#gateButton').attr('disabled', false);
          }
        }
    )""""
    "function pOpen(state, status, color) {"
    R""""(
        $('#pOpenButton').val(state);
        $('#pOpenButton').css('background-color', color);
        $('#gateStatus').val(status);
        next++;
        pOpenNext(state, status, color);
    }
    )""""
    "function gateOpenClose(state, status) {"
    R""""(
        $('#gateButton').attr('disabled', false);
        if(status == 'Gate Closed') {
            $('#pOpenButton').attr('disabled', false);
        }
        $('#gateButton').css('background-color', 'green');
        $('#gateStatus').val(status);
        $('#gateButton').val(state);
        $('#stopButton').attr('disabled', true);
    }
    )""""
    "function buttonPressed(state, nextState, status) {"
    R""""(
        $('#gateButton').attr('disabled', true);
        $('#pOpenButton').attr('disabled', true);
        $('#stopButton').attr('disabled', false);
        $("#gateStatus").val(status);
        $('#gateButton').val(state);
        $('#gateButton').css('background-color', 'red');
        timerID = setTimeout(function() { gateOpenClose(nextState, status); }, $('#openClose').val() + '000');
    }
    $(':button').bind('click', function(event, ui) {    
        var butName = $(this).val(); 
        var gateStatus = $("#gateStatus").val();
        if(butName == 'Stop') {
            clearTimeout(timerID); 
            stopButtonPressed();
            $.post('/gate', { command: butName });
        } else if(butName == 'Open') {
            console.log("in Open");
            buttonPressed('Opening', 'Close', 'Gate Open');
            $.post('/gate', { command: butName });
        } else if(butName == 'Close') {
            buttonPressed('Closing', 'Open', 'Gate Closed');
            $.post('/gate', { command: butName });
        } else if(butName == 'Partial Open') {
            $('#gateButton').attr('disabled', true);
            $('#stopButton').attr('disabled', false);
            next = 0;
            pOpenNext('Opening', 'Gate Opening', 'red');
            $.post('/gate', { command: butName });
        }
    });
    $('.times').on('input change', function(event, ui) {
        var name = $(this).attr('name');
        var value = $(this).val();
        if(name == "pulse") {
            $("#" + name + "TimeValue").text(value + "ms");
        } else {
            $("#" + name + "TimeValue").text(value + "s");
        }
        if(event.type == 'change') {
            $.post('/gate', { [name]: value });
            
            console.log("$.post('/gate', { " + name + ": " + value + " });");
        }
    });
    </script>
    </html>
    )"""";
    
    // have to build sliders manually, as the default values change
    char buffer[10];
    // openClose
    sprintf(buffer, "%d", gateTimes.openClose_Time);
    server.printP(webPage1);
    server << "<td><input type='range' class='times' id='openClose' name='openClose' value='" << buffer;
    server << "' min='10' max='60' step='1'>";
    server.printP(webPage2);
    server << "<td><label id=openCloseTimeValue class=slideValues>" << buffer << "s</label></td>";
    server.printP(webPage3);
    
    // pOpen
    sprintf(buffer, "%d", gateTimes.pOpen_Time);
    server << "<td><input type='range' class='times' id='pOpen' name='pOpen' value='" << buffer;
    server << "' min='1' max='25' step='1'></td>";
    server.printP(webPage4);
    server << "<td><label id=pOpenTimeValue class=slideValues>" << buffer << "s</label></td>";
    server.printP(webPage5);
 
    // rOpen
    sprintf(buffer, "%d", gateTimes.rOpen_Time);
    server << "<td><input type='range' class='times' id='rOpen' name='rOpen' value='" << buffer;
    server << "' min='10' max='120' step='1'></td>";
    server.printP(webPage6);
    server << "<td><label id=rOpenTimeValue class=slideValues>" << buffer << "s</label></td>";
    server.printP(webPage7);
    
    // pulse 
    sprintf(buffer, "%d", gateTimes.pulse);
    server << "<td><input type='range' class='times' id='pulse' name='pulse' value='" << buffer;
    server << "' min='50' max='1000' step='10'></td>";
    server.printP(webPage8);
    server << "<td><label id=pulseTimeValue class=slideValues>" << buffer << "ms</label></td>";
    server.printP(webPage9);
    }
}


// define pins on photon
const byte M_OPEN = D1;     // drives active HIGH N-Channel MOSFET
const byte M_CLOSE = D2;    // drives active HIGH N-Channel MOSFET
const byte M_STOPb = D3;    // drives active LOW N-Channel MOSFET

int next = 0;       // used to setp through threads

bool openStatus = FALSE;   // default value is FALSE (closed)
bool busy = FALSE;          // indicates if the gate is busy opening or closing

void openDone();
void pOpenOpened();
void pOpenClose();

Timer openTimer(gateTimes.openClose_Time*1000, openDone, TRUE);   // open/close gate timer
Timer pOpenTimer(gateTimes.pOpen_Time*1000, pOpenOpened, TRUE);   // pOpen openClose timer
Timer rOpenTimer(gateTimes.rOpen_Time*1000, pOpenClose, TRUE);    // rOpen stays open timer
Timer pCloseTimer(gateTimes.pOpen_Time*1000, pOpenClosed, TRUE);  // pOpen openClose time

// issues a pulse on the control pin
void issuePulse(int pin) {
    if(pin == M_STOPb) {
        Serial.println("PULSE OCCURS (HIGH, LOW, HIGH)");
        digitalWrite(M_STOPb, LOW);
        delay(gateTimes.pulse);
        digitalWrite(M_STOPb, HIGH);
    } else {
        Serial.println("PULSE OCCURS (LOW, HIGH, LOW)");
        digitalWrite(pin, HIGH);
        delay(gateTimes.pulse);
        digitalWrite(pin, LOW);
    }
}

// open_done - openTimer callback, called when gate is opened or closed
void openDone() {
    Serial.println("open/closeDone()");
    busy = FALSE;
    #ifdef DEBUG
    digitalWrite(M_OPEN, LOW);
    digitalWrite(M_CLOSE, LOW);
    Serial.println("M_OPEN: LOW, M_CLOSE: LOW");
    #endif
}

/*
    cpen - opens the gqte
*/
void open() {
    if(busy == TRUE) return;        // if busy return
    Serial.println("open() - after check busy");
    openStatus = TRUE;             // gate is now open
    
    #ifdef DEBUG
    Serial.println("DEBUG, M_OPEN: HIGH");
    digitalWrite(M_OPEN, HIGH);         // active high
    #endif
    #ifndef DEBUG
    Serial.println("not DEBUG, pulsing: M_OPEN");
    issuePulse(M_OPEN);
    #endif
    
    busy = TRUE;
    openTimer.start();  // callback is open_Done
}

/*
    close - closes the gate
*/
void close() {
    if(busy == TRUE) return;        // if busy return
    Serial.println("close() - after check busy");
    openStatus = FALSE;
    
    #ifdef DEBUG
    Serial.println("DEBUG, M_CLOSE: HIGH");
    digitalWrite(M_CLOSE, HIGH);
    #endif
    #ifndef DEBUG
    Serial.println("not DEBUG, pulsing: M_CLOSE");
    issuePulse(M_CLOSE);
    #endif
    
    busy = TRUE;
    openTimer.start();  // let system know when door is closed
}

// pulses the stop
void stop() {
    Serial.println("stop(), pulsing: M_STOPb");
    issuePulse(M_STOPb);
    
    #ifdef DEBUG
    Serial.println("DEBUG, M_CLOSE: LOW");
    digitalWrite(M_CLOSE, LOW);     // turn off M_CLOSE (LOW)   
    digitalWrite(M_OPEN, LOW);      // turn off M_OPEN (LOW)
    #endif
    
    busy = false;
    // stop anytimers
    openTimer.stop();
    pOpenTimer.stop();
    rOpenTimer.stop();
    openStatus = true;
}

/*
    pOpen - issues a partial open command to the gate.  
    a.  Open gate command
    b.  Wait "pOpen_Time" seconds
    c.  Stop gate command
    d.  wait "rOpen_Time" seconds
    e.  Close gate command 
    f.  wait "pOpen_Time" seconds
*/
void pOpen() {
    if(busy == TRUE) return;        // if busy return
    Serial.println("pOpen() - after check busy");
    // start opening the gate
    openStatus = TRUE;             // gate is now open
    
    #ifdef DEBUG
    Serial.println("DEBUG, pOPEN M_OPEN: HIGH");
    digitalWrite(M_OPEN, HIGH);
    #endif
    #ifndef DEBUG
    Serial.println("not DEBUG, pulsing: M_OPEN");
    issuePulse(M_OPEN);
    #endif
    
    // wait for pOpenClose_Time
    pOpenTimer.start();            // callback pOpenOpened
}

// *** Timer call backs for pOpen
// pOpenOpened - pOpenTimer callback
void pOpenOpened() {
    Serial.println("pOpenOpened()");
    
    #ifdef DEBUG
    Serial.println("DEBUG, pOPEN M_STOPb: LOW");
    digitalWrite(M_STOPb, LOW);
    #endif
    #ifndef DEBUG
    Serial.println("not DEBUG, pulsing: M_STOPb");
    issuePulse(M_STOPb);
    #endif

    rOpenTimer.start();             // keep gate open for this time, callback pOpenClose
}

// pOpenClose - rOpenTimer callback, gate ready to close
void pOpenClose() {
    Serial.println("pOpenClose()");
    // issue a close pulse
    
    #ifdef DEBUG
    Serial.println("DEBUG, pOpenClose() M_CLOSE: HIGH");
    digitalWrite(M_CLOSE, HIGH);
    #endif
    #ifndef DEBUG
    Serial.println("not DEBUG, pOpenClose() pulsing: M_CLOSE");
    issuePulse(M_CLOSE);
    #endif

    pCloseTimer.start();        // closing gate, callback pOpenClosed
}

// pOpen_closed - pCloseTimer callback, called when gate is closed
void pOpenClosed() {
    Serial.println("pOpenClosed()");
    
    #ifdef DEBUG
    Serial.println("DEBUG, pOpenClosed() M_CLOSE: LOW");
    digitalWrite(M_CLOSE, LOW);
    #endif
    
    busy = false;
    openStatus = FALSE;
}

// gate - this is the particle function call.
int gate(String command) {
    if(command == "open") {
        open();
    } else if(command == "close") {
        close();
    } else if (command == "stop") {
        stop();
    }  if (command == "popen") {
        pOpen();
    } else return -1;
    return 1;
}

void setOpenTime() {
    openTimer.changePeriod(gateTimes.openClose_Time*1000);
    openTimer.stop();
}

void setPopenTime() {
    pOpenTimer.changePeriod(gateTimes.pOpen_Time*1000);      // pOpen open/cose timer
    pOpenTimer.stop();
}

void setRopenTime() {
    rOpenTimer.changePeriod(gateTimes.rOpen_Time*1000);
    rOpenTimer.stop();
}

void setup() {
    Serial.begin();
    waitFor(Serial.isConnected, 30000);
    // set control pins to open drain outputs
    pinMode(M_OPEN, OUTPUT);
    pinMode(M_CLOSE, OUTPUT);
    pinMode(M_STOPb, OUTPUT);
    
    // set default values
    Serial.println("Setting default pin values");
    Serial.println("M_OPEN: LOW, M_CLOSE: LOW, M_STOPb: HIGH");
    digitalWrite(M_OPEN, LOW);
    digitalWrite(M_CLOSE, LOW);
    digitalWrite(M_STOPb, HIGH);
    
    // select external antennae
    STARTUP(WiFi.selectAntenna(ANT_EXTERNAL)); // selects the u.FL antenna
    WiFi.setHostname("NachtGate");
    // set wifi credentials
    WiFi.setCredentials("WalshWireless", "420Walsh2008");   // Nachtsheim
    WiFi.setCredentials("Kardach", "kardach123");   // kardach
    
    // Connect if settings were successfully saved
    WiFi.connect();
    waitFor(WiFi.ready, 30000);
    Particle.connect();
    
    // close the gate
    close();
    
    // have a bool stored at address 0.  default value is 255
    uint16_t value;
    EEPROM.get(0, value);
    if(value != 0) {
        value = 0;      // set value to zero
        EEPROM.put(0, value);   // write it to EEPROM
        EEPROM.put(10, gateTimes);  // write gateTimes to EEPROM
    } else {
        EEPROM.get(10, gateTimes);  // get the latest times
    }
    
    myIP = WiFi.localIP().toString();

    // // setup the particle functions
    Particle.function("gate", gate);

    // // setup the particle variables
    Particle.variable("IP Address", myIP);
    Particle.variable("openClose_Time", gateTimes.openClose_Time);
    Particle.variable("pOpen_Time", gateTimes.pOpen_Time);
    Particle.variable("rOpen_Time", gateTimes.rOpen_Time);
    Particle.variable("pulse_Time", gateTimes.pulse);
    Particle.variable("Version", VERSION);
    
    webserver.setDefaultCommand(&defaultCmd);
    
    Serial.println("Default Gate Time Values:");
    Serial.print("gateTimes.openClose_Time: ");
    Serial.print(gateTimes.openClose_Time);
    Serial.println("s");
    
    Serial.print("gateTimes.pOpen_Time: ");
    Serial.print(gateTimes.pOpen_Time);
    Serial.println("s");
    
    Serial.print("gateTimes.rOpen_Time: ");
    Serial.print(gateTimes.rOpen_Time);
    Serial.println("s");
    
    Serial.print("gateTimes.pulse: ");
    Serial.print(gateTimes.pulse);
    Serial.println("ms");
    
    // start webserver
    webserver.begin();

    Serial.print("http://");
    Serial.print(WiFi.localIP().toString());
    Serial.println("/gate to bring up the webpage");

}

void loop() {
    webserver.processConnection();
    
}
