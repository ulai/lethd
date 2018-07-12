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

#include <dirent.h>

using namespace p44;

#define DEFAULT_LOGLEVEL LOG_NOTICE


// MARK: ==== Application

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

  AnalogIoPtr sensor;

  MLMicroSeconds starttime;

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
      { 0  , "sensor",         true,  "pinspec;analog sensor input to use" },
      { 0  , "ledchain",       true,  "devicepath;ledchain device to use" },
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

      // create sensor input
      sensor =  AnalogIoPtr(new AnalogIo(getOption("sensor","missing"), false, 0));

      // - create and start API server and wait for things to happen
      string apiport;
      if (getStringOption("jsonapiport", apiport)) {
        apiServer = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
        apiServer->setConnectionParams(NULL, apiport.c_str(), SOCK_STREAM, AF_INET);
        apiServer->setAllowNonlocalConnections(getOption("jsonapinonlocal"));
        apiServer->startServer(boost::bind(&LEthD::apiConnectionHandler, this, _1), 3);
      }


    } // if !terminated
    // app now ready to run (or cleanup when already terminated)
    return run();
  }



  virtual void initialize()
  {
    LOG(LOG_NOTICE, "lethd initialize()");
    LOG(LOG_NOTICE, "sensor input returns %.1f", sensor->value());
    terminateApp(EXIT_FAILURE);
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
    /*
    JsonObjectPtr o;
    if (aUri=="files") {
      // return a list of files
      string action;
      if (!aData->get("action", o)) {
        aIsAction = false;
      }
      else {
        action = o->stringValue();
      }
      if (!aIsAction) {
        DIR *dirP = opendir (datadir.c_str());
        struct dirent *direntP;
        if (dirP==NULL) {
          err = SysError::errNo("Cannot read data directory: ");
        }
        else {
          JsonObjectPtr files = JsonObject::newArray();
          bool foundSelected = false;
          while ((direntP = readdir(dirP))!=NULL) {
            string fn = direntP->d_name;
            if (fn=="." || fn=="..") continue;
            JsonObjectPtr file = JsonObject::newObj();
            file->add("name", JsonObject::newString(fn));
            file->add("ino", JsonObject::newInt64(direntP->d_ino));
            file->add("type", JsonObject::newInt64(direntP->d_type));
            if (fn==selectedfile) foundSelected = true;
            file->add("selected", JsonObject::newBool(fn==selectedfile));
            files->arrayAppend(file);
          }
          closedir (dirP);
          if (!foundSelected) selectedfile.clear(); // remove selection not matching any of the existing files
          aRequestDoneCB(files, ErrorPtr());
          return true;
        }
      }
      else {
        // a file action
        if (!aData->get("name", o)) {
          err = WebError::webErr(400, "Missing 'name'");
        }
        else {
          // addressing a particular file
          // - try to open to see if it exists
          string filename = o->c_strValue();
          string filepath = datadir;
          pathstring_format_append(filepath, "%s", filename.c_str());
          FILE *fileP = fopen(filepath.c_str(),"r");
          if (fileP==NULL) {
            err = WebError::webErr(404, "File '%s' not found", filename.c_str());
          }
          else {
            // exists
            fclose(fileP);
            if (action=="rename") {
              // rename file
              if (!aData->get("newname", o)) {
                err = WebError::webErr(400, "Missing 'newname'");
              }
              else {
                string newname = o->stringValue();
                if (newname.size()<3) {
                  err = WebError::webErr(415, "'newname' is too short (min 3 characters)");
                }
                else {
                  string newpath = datadir;
                  pathstring_format_append(newpath, "%s", newname.c_str());
                  if (rename(filepath.c_str(), newpath.c_str())!=0) {
                    err = SysError::errNo("Cannot rename file: ");
                  }
                }
              }
            }
//              else if (action=="download") {
//
//              }
            else if (action=="delete") {
              if (unlink(filepath.c_str())!=0) {
                err = SysError::errNo("Cannot delete file: ");
              }
            }
            else if (action=="select") {
              if (selectedfile==filename) {
                selectedfile.clear(); // unselect
              }
              else {
                selectedfile = filename; // select
              }
            }
            else if (action=="send") {
              err = sendFile(filepath);
            }
            else {
              err = WebError::webErr(400, "Unknown files action");
            }
          }
        }
      }
      actionStatus(aRequestDoneCB, err);
      return true;
    }
    else if (aIsAction && aUri=="log") {
      if (aData->get("level", o)) {
        int lvl = o->int32Value();
        LOG(LOG_NOTICE, "\n====== Changed Log Level from %d to %d\n", LOGLEVEL, lvl);
        SETLOGLEVEL(lvl);
      }
      actionStatus(aRequestDoneCB, err);
      return true;
    }
    else if (aUri=="/") {
      string uploadedfile;
      string cmd;
      if (aData->get("uploadedfile", o))
        uploadedfile = o->stringValue();
      if (aData->get("cmd", o))
        cmd = o->stringValue();
    }
    */
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
