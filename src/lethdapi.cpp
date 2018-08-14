//
//  Copyright (c) 2016-2017 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Ueli Wahlen <ueli@hotmail.com>
//
//  This file is part of lethd.
//
//  pixelboardd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  pixelboardd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with pixelboardd. If not, see <http://www.gnu.org/licenses/>.
//

#include "lethdapi.hpp"

using namespace p44;

SocketCommPtr LethdApi::apiConnectionHandler(SocketCommPtr aServerSocketComm)
{
  JsonCommPtr conn = JsonCommPtr(new JsonComm(MainLoop::currentMainLoop()));
  conn->setMessageHandler(boost::bind(&LethdApi::apiRequestHandler, this, conn, _1, _2));
  conn->setClearHandlersAtClose(); // close must break retain cycles so this object won't cause a mem leak

  connection = conn;
  return conn;
}

void LethdApi::apiRequestHandler(JsonCommPtr aConnection, ErrorPtr aError, JsonObjectPtr aRequest)
{
  if (Error::isOK(aError)) {
    LOG(LOG_INFO,"API request: %s", aRequest->c_strValue());
    aError = processRequest(aRequest);
  }
  // return
  requestHandled(aConnection, JsonObjectPtr(), aError);
}

void LethdApi::requestHandled(JsonCommPtr aConnection, JsonObjectPtr aResponse, ErrorPtr aError)
{
  if (!aResponse) {
    aResponse = JsonObject::newObj(); // empty response
  }
  if (!Error::isOK(aError)) {
    aResponse->add("Error", JsonObject::newString(aError->description()));
  }
  LOG(LOG_INFO,"API answer: %s", aResponse->c_strValue());
  aConnection->sendMessage(aResponse);
}

void LethdApi::init(JsonObjectPtr aData)
{
  initFeature(aData);
}

void LethdApi::now(JsonObjectPtr aData)
{
  JsonObjectPtr answer = JsonObject::newObj();
  answer->add("now", JsonObject::newInt64(MainLoop::now()));
  connection->sendMessage(answer);
}

void LethdApi::fade(JsonObjectPtr aData)
{
  double from = fader->current();
  if(aData->get("from")) from = aData->get("from")->doubleValue();
  double to = aData->get("to")->doubleValue();
  int64_t t = aData->get("t")->int64Value();
  MLMicroSeconds start = aData->get("t")->int64Value();
  fader->fade(from, to, t, start);
}

ErrorPtr LethdApi::processRequest(JsonObjectPtr aData)
{
  ErrorPtr err;

  if(!aData->get("cmd")) return LethdApiError::err("Misssing cmd attribute");
  if(!aData->get("cmd")->isType(json_type_string)) return LethdApiError::err("cmd attribute must be a string");
  string aCmd = aData->get("cmd")->stringValue().c_str();

  typedef boost::function<void (JsonObjectPtr aData)> CmdFunctionCB;
  static std::map<string, CmdFunctionCB> m;

  m["init"] = boost::bind(&LethdApi::init, this, _1);
  m["now"] = boost::bind(&LethdApi::now, this, _1);
  m["fade"] = boost::bind(&LethdApi::fade, this, _1);

  if(m.count(aCmd)) {
    m[aCmd](aData);
    return err;
  }

  return LethdApiError::err("Unknown cmd: %s", aCmd.c_str());
}

LethdApi::LethdApi(TextViewPtr aMessage, FaderPtr aFader, InitFeatureCB aInitFeature)
{
  message = aMessage;
  fader = aFader;
  initFeature = aInitFeature;
}

void LethdApi::start(const char* aApiPort)
{
  apiServer = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
  apiServer->setConnectionParams(NULL, aApiPort, SOCK_STREAM, AF_INET6);
  apiServer->setAllowNonlocalConnections(true);
  apiServer->startServer(boost::bind(&LethdApi::apiConnectionHandler, this, _1), 3);
  LOG(LOG_INFO, "LEthDApi listening on %s", aApiPort);
}

void LethdApi::send(int aValue)
{
  if(!connection) return;
  JsonObjectPtr aMessage = JsonObject::newObj();
  aMessage->add("sensor", JsonObject::newInt32(aValue));
  connection->sendMessage(aMessage);
}

ErrorPtr LethdApiError::err(const char *aFmt, ...)
{
  Error *errP = new LethdApiError();
  va_list args;
  va_start(args, aFmt);
  errP->setFormattedMessage(aFmt, args);
  va_end(args);
  return ErrorPtr(errP);
}
