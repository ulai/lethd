//
//  Copyright (c) 2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of lethd.
//
//  lethd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  lethd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with lethd. If not, see <http://www.gnu.org/licenses/>.
//

#include "application.hpp"
#include "digitalio.hpp"
#include "analogio.hpp"
#include "jsoncomm.hpp"
#include "ledchaincomm.hpp"

#include "textview.hpp"
#include "lethdapi.hpp"
#include "fader.hpp"
#include "neuron.hpp"

#include <dirent.h>

using namespace p44;

#define DEFAULT_LOGLEVEL LOG_NOTICE


// MARK: ==== Application

#define MKSTR(s) _MKSTR(s)
#define _MKSTR(s) #s

#define LED_MODULE_COLS 74
#define LED_MODULE_ROWS 7
#define LED_MODULE_BORDER_LEFT 1
#define LED_MODULE_BORDER_RIGHT 1

typedef boost::function<void (JsonObjectPtr aResponse, ErrorPtr aError)> RequestDoneCB;

class LEthD : public CmdLineApp
{
  typedef CmdLineApp inherited;

  // API Server
  SocketCommPtr apiServer;

  // LED+Button
  ButtonInputPtr button;
  IndicatorOutputPtr greenLed;
  IndicatorOutputPtr redLed;

  AnalogIoPtr sensor0;
  AnalogIoPtr sensor1;
  AnalogIoPtr pwmDimmer;

  // LED chain
  LEDChainCommPtr ledChain;
  int ledRows;
  int ledCols;
  int ledBorderRight;
  int ledBorderLeft;
  int ledOrientation;

  TextViewPtr message;

  MLMicroSeconds starttime;

  MLTicket sampleTicket;

  LethdApiPtr lethdApi;
  FaderPtr fader;
  NeuronPtr neuron;

public:

  LEthD() :
    starttime(MainLoop::now())
  {
  }

  virtual int main(int argc, char **argv)
  {
    const char *usageText =
      "Usage: %1$s [options]\n";
    const CmdLineOptionDescriptor options[] = {
      { 0  , "pwmdimmer",      true,  "pinspec;PWM dimmer output pin" },
      { 0  , "sensor0",        true,  "pinspec;analog sensor0 input to use" },
      { 0  , "sensor1",        true,  "pinspec;analog sensor1 input to use" },
      { 0  , "ledchain",       true,  "devicepath;ledchain device to use" },
      { 0  , "ledrows",        true,  "rows;number of LED rows (default=" MKSTR(LED_MODULE_ROWS)  ")" },
      { 0  , "ledcols",        true,  "columns;total number of LED columns (default=" MKSTR(LED_MODULE_COLS) ")" },
      { 0  , "orientation",    true,  "orientation;text view orientation (default=0=normal)" },
      { 0  , "borderleft",     true,  "columns;number of hidden columns on the left (default=" MKSTR(LED_MODULE_BORDER_LEFT) ")" },
      { 0  , "borderright",    true,  "columns;number of hidden columns on the right (default=" MKSTR(LED_MODULE_BORDER_RIGHT) ")" },
      { 0  , "lethdapiport",   true,  "port;server port number for lETHd JSON API (default=none)" },
      { 0  , "jsonapiport",    true,  "port;server port number for JSON API (default=none)" },
      { 0  , "jsonapinonlocal",false, "allow JSON API from non-local clients" },
      { 0  , "button",         true,  "input pinspec; device button" },
      { 0  , "greenled",       true,  "output pinspec; green device LED" },
      { 0  , "redled",         true,  "output pinspec; red device LED" },
      { 'l', "loglevel",       true,  "level;set max level of log message detail to show on stdout" },
      { 0  , "errlevel",       true,  "level;set max level for log messages to go to stderr as well" },
      { 0  , "dontlogerrors",  false, "don't duplicate error messages (see --errlevel) on stdout" },
      { 0  , "deltatstamps",   false, "show timestamp delta between log lines" },
      { 'r', "resourcepath",   true,  "path;path to the images and sounds folders" },
      { 'd', "datapath",       true,  "path;path to the r/w persistent data" },
      { 'h', "help",           false, "show this text" },
      { 0, NULL } // list terminator
    };

    // parse the command line, exits when syntax errors occur
    setCommandDescriptors(usageText, options);
    parseCommandLine(argc, argv);

    if ((numOptions()<1) || numArguments()>0) {
      // show usage
      showUsage();
      terminateApp(EXIT_SUCCESS);
    }

    // build objects only if not terminated early
    if (!isTerminated()) {
      int loglevel = DEFAULT_LOGLEVEL;
      getIntOption("loglevel", loglevel);
      SETLOGLEVEL(loglevel);
      int errlevel = LOG_ERR; // testing by default only reports to stdout
      getIntOption("errlevel", errlevel);
      SETERRLEVEL(errlevel, !getOption("dontlogerrors"));
      SETDELTATIME(getOption("deltatstamps"));

      // create button input
      button = ButtonInputPtr(new ButtonInput(getOption("button","missing")));
      button->setButtonHandler(boost::bind(&LEthD::buttonHandler, this, _1, _2, _3), true, Second);
      // create LEDs
      greenLed = IndicatorOutputPtr(new IndicatorOutput(getOption("greenled","missing")));
      redLed = IndicatorOutputPtr(new IndicatorOutput(getOption("redled","missing")));

      // create sensor input(s)
      sensor0 =  AnalogIoPtr(new AnalogIo(getOption("sensor0","missing"), false, 0));
      sensor1 =  AnalogIoPtr(new AnalogIo(getOption("sensor1","missing"), false, 0));
      // create PWM output
      pwmDimmer = AnalogIoPtr(new AnalogIo(getOption("pwmdimmer","missing"), true, 0)); // off to begin with
      // create LED chain output
      string ledChainDev;
      if (getStringOption("ledchain", ledChainDev)) {
        // get geometry
        ledRows = LED_MODULE_ROWS;
        ledCols = LED_MODULE_COLS;
        ledBorderLeft = LED_MODULE_BORDER_LEFT;
        ledBorderRight = LED_MODULE_BORDER_RIGHT;
        ledOrientation = View::right;
        getIntOption("ledrows", ledRows);
        getIntOption("ledcols", ledCols);
        getIntOption("borderleft", ledBorderLeft);
        getIntOption("borderright", ledBorderRight);
        getIntOption("orientation", ledOrientation);
        // create chain driver
        ledChain = LEDChainCommPtr(new LEDChainComm(LEDChainComm::ledtype_ws281x, ledChainDev, ledRows*ledCols, ledCols, false, true));
        ledChain->begin();
      }
      // - create and start API server and wait for things to happen
      string apiport;
      if (getStringOption("jsonapiport", apiport)) {
        apiServer = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
        apiServer->setConnectionParams(NULL, apiport.c_str(), SOCK_STREAM, AF_INET6);
        apiServer->setAllowNonlocalConnections(getOption("jsonapinonlocal"));
        apiServer->startServer(boost::bind(&LEthD::apiConnectionHandler, this, _1), 3);
      }

      // - create and start API server for lethd server
      if (getStringOption("lethdapiport", apiport)) {
        fader = FaderPtr(new Fader(boost::bind(&LEthD::fadeUpdate, this, _1)));
        neuron = NeuronPtr(new Neuron(ledChain, boost::bind(&LEthD::neuronMeasure, this), boost::bind(&LEthD::neuronSpike, this, _1)));
        lethdApi = LethdApiPtr(new LethdApi(message, fader, neuron, boost::bind(&LEthD::initFeature, this, _1)));
        lethdApi->start(apiport.c_str());
      }
    } // if !terminated
    // app now ready to run (or cleanup when already terminated)
    return run();
  }


  virtual void initialize()
  {
    LOG(LOG_NOTICE, "lethd initialize()");
    if (ledChain) {
      message = TextViewPtr(new TextView(ledBorderLeft, 0, ledCols-ledBorderLeft-ledBorderRight, (View::Orientation)ledOrientation));
      message->setBackGroundColor(black); // not transparent!
      message->setText("Hello World", true);
      //MainLoop::currentMainLoop().executeNow(boost::bind(&LEthD::step, this, _1));
    }

    MainLoop::currentMainLoop().executeNow(boost::bind(&LEthD::features, this, _1));
  }

  void initFeature(JsonObjectPtr aData) {
    ErrorPtr err;
    JsonObjectPtr o;
    LOG(LOG_INFO, "initFeature: %s", aData->c_strValue());

    if(aData->get("text", o)) {
      // TODO init text, ledchain etc.
    } else if(aData->get("light", o)) {
      LOG(LOG_INFO, "initialize fader");
      fader->initialize();
    } else if(aData->get("neuron", o)) {
      LOG(LOG_INFO, "initialize neuron");
      neuron->initialize();
      neuron->start(o->get("movingAverageCount")->doubleValue(), o->get("threshold")->doubleValue());
    }
  }

  void features(MLTimer &aTimer) {
    MainLoop::currentMainLoop().retriggerTimer(aTimer, 10*MilliSecond);
  }

  void fadeUpdate(double aValue) {
    LOG(LOG_INFO, "fadeUpdate %f", aValue);
    if (pwmDimmer) pwmDimmer->setValue(aValue);
  }

  double neuronMeasure() {
    double value = -1;
    if (sensor0) value = sensor0->value();
    return value;
  }

  void neuronSpike(double aValue) {
    lethdApi->send(aValue);
  }

  void step(MLTimer &aTimer)
  {
    if (message) {
      message->step();
      updateDisplay();
    }
    MainLoop::currentMainLoop().retriggerTimer(aTimer, 10*MilliSecond);
  }

  void updateDisplay()
  {
    if (message && message->isDirty()) {
      for (int x=0; x<ledCols; x++) {
        for (int y=0; y<ledRows; y++) {
          PixelColor p = message->colorAt(x, y);
          PixelColor dp = dimPixel(p, p.a);
          ledChain->setColorXY(x, y, dp.r, dp.g, dp.b);
        }
      }
      ledChain->show();
      message->updated();
    }
  }

  void demoSample(MLTimer &aTimer)
  {
    double ad = sensor0->value();
    LOG(LOG_INFO, "A/D sample value = %.0f", ad);
    // adjust PWM
    if (pwmDimmer) pwmDimmer->setValue(ad/1024*100);
//    // show LED bar
//    if (ledChain) {
//      const int numLeds = 10;
//      int onLeds = ad/1024*numLeds;
//      LOG(LOG_INFO, "onLeds=%d", onLeds);
//      for (int i=0; i<numLeds; i++) {
//        if (i<=onLeds) {
//          ledChain->setColor(i, 255, 255-(255*i/numLeds), 0);
//        }
//        else {
//          ledChain->setColor(i, 0, 50, 0);
//        }
//      }
//      ledChain->show();
//    }
    MainLoop::currentMainLoop().retriggerTimer(aTimer, 333*MilliSecond);
  }



  // MARK: ==== Button


  void buttonHandler(bool aState, bool aHasChanged, MLMicroSeconds aTimeSincePreviousChange)
  {
    LOG(LOG_INFO, "Button state now %d%s", aState, aHasChanged ? " (changed)" : " (same)");
  }




  // MARK: ==== API access


  SocketCommPtr apiConnectionHandler(SocketCommPtr aServerSocketComm)
  {
    JsonCommPtr conn = JsonCommPtr(new JsonComm(MainLoop::currentMainLoop()));
    conn->setMessageHandler(boost::bind(&LEthD::apiRequestHandler, this, conn, _1, _2));
    conn->setClearHandlersAtClose(); // close must break retain cycles so this object won't cause a mem leak
    return conn;
  }


  void apiRequestHandler(JsonCommPtr aConnection, ErrorPtr aError, JsonObjectPtr aRequest)
  {
    // Decode mg44-style request (HTTP wrapped in JSON)
    if (Error::isOK(aError)) {
      LOG(LOG_INFO,"API request: %s", aRequest->c_strValue());
      JsonObjectPtr o;
      o = aRequest->get("method");
      if (o) {
        string method = o->stringValue();
        string uri;
        o = aRequest->get("uri");
        if (o) uri = o->stringValue();
        JsonObjectPtr data;
        bool upload = false;
        bool action = (method!="GET");
        // check for uploads
        string uploadedfile;
        if (aRequest->get("uploadedfile", o)) {
          uploadedfile = o->stringValue();
          upload = true;
          action = false; // other params are in the URI, not the POSTed upload
        }
        if (action) {
          // JSON data is in the request
          data = aRequest->get("data");
        }
        else {
          // URI params is the JSON to process
          data = aRequest->get("uri_params");
          if (data) action = true; // GET, but with query_params: treat like PUT/POST with data
          if (upload) {
            // move that into the request
            data->add("uploadedfile", JsonObject::newString(uploadedfile));
          }
        }
        // request elements now: uri and data
        if (processRequest(uri, data, action, boost::bind(&LEthD::requestHandled, this, aConnection, _1, _2))) {
          // done, callback will send response and close connection
          return;
        }
        // request cannot be processed, return error
        LOG(LOG_ERR,"Invalid JSON request");
        aError = WebError::webErr(404, "No handler found for request to %s", uri.c_str());
      }
      else {
        LOG(LOG_ERR,"Invalid JSON request");
        aError = WebError::webErr(415, "Invalid JSON request format");
      }
    }
    // return error
    requestHandled(aConnection, JsonObjectPtr(), aError);
  }


  void requestHandled(JsonCommPtr aConnection, JsonObjectPtr aResponse, ErrorPtr aError)
  {
    if (!aResponse) {
      aResponse = JsonObject::newObj(); // empty response
    }
    if (!Error::isOK(aError)) {
      aResponse->add("Error", JsonObject::newString(aError->description()));
    }
    LOG(LOG_INFO,"API answer: %s", aResponse->c_strValue());
    aConnection->sendMessage(aResponse);
    aConnection->closeAfterSend();
  }


  bool processRequest(string aUri, JsonObjectPtr aData, bool aIsAction, RequestDoneCB aRequestDoneCB)
  {
    ErrorPtr err;
    JsonObjectPtr o;
    if (aUri=="brightness") {
      JsonObjectPtr answer;
      if (aIsAction) {
        // set
        if (message && aData->get("text", o)) {
          // text brightness
          message->setAlpha(o->doubleValue()/100*255);
        }
        if (pwmDimmer && aData->get("light", o)) {
          // light brightness
          pwmDimmer->setValue(o->doubleValue());
        }
      }
      // get
      answer = JsonObject::newObj();
      if (message) answer->add("text", JsonObject::newInt32((double)(message->getAlpha())/255.0*100));
      if (pwmDimmer) answer->add("light", JsonObject::newDouble(pwmDimmer->value()));
      aRequestDoneCB(answer, ErrorPtr());
      return true;
    }
    else if (aUri=="text") {
      if (aIsAction) {
        if (aData->get("message", o)) {
          if (message) message->setText(o->stringValue(), true);
          aRequestDoneCB(JsonObjectPtr(), ErrorPtr());
          return true;
        }
        else if (aData->get("color", o)) {
          PixelColor p = webColorToPixel(o->stringValue());
          if (p.a==255) p.a = message->getAlpha();
          message->setTextColor(p);
          aRequestDoneCB(JsonObjectPtr(), ErrorPtr());
          return true;
        }
        else if (aData->get("backgroundcolor", o)) {
          PixelColor p = webColorToPixel(o->stringValue());
          if (p.a==255) p.a = message->getAlpha();
          message->setBackGroundColor(p);
          aRequestDoneCB(JsonObjectPtr(), ErrorPtr());
          return true;
        }
      }
    }
    else if (aUri=="inputs") {
      if (!aIsAction) {
        JsonObjectPtr answer = JsonObject::newObj();
        if (sensor0) answer->add("sensor0", JsonObject::newInt32(sensor0->value()));
        if (sensor1) answer->add("sensor1", JsonObject::newInt32(sensor1->value()));
        aRequestDoneCB(answer, ErrorPtr());
        return true;

      }
    }
    return false;
  }


  void actionDone(RequestDoneCB aRequestDoneCB)
  {
    aRequestDoneCB(JsonObjectPtr(), ErrorPtr());
  }


  void actionStatus(RequestDoneCB aRequestDoneCB, ErrorPtr aError = ErrorPtr())
  {
    aRequestDoneCB(JsonObjectPtr(), aError);
  }



  ErrorPtr processUpload(string aUri, JsonObjectPtr aData, const string aUploadedFile)
  {
    ErrorPtr err;

    string cmd;
    JsonObjectPtr o;
    if (aData->get("cmd", o)) {
      cmd = o->stringValue();
//      if (cmd=="imageupload") {
//        displayPage->loadPNGBackground(aUploadedFile);
//        gotoPage("display", false);
//        updateDisplay();
//      }
//      else
      {
        err = WebError::webErr(500, "Unknown upload cmd '%s'", cmd.c_str());
      }
    }
    return err;
  }



};





int main(int argc, char **argv)
{
  // prevent debug output before application.main scans command line
  SETLOGLEVEL(LOG_EMERG);
  SETERRLEVEL(LOG_EMERG, false); // messages, if any, go to stderr
  // create app with current mainloop
  static LEthD application;
  // pass control
  return application.main(argc, argv);
}
