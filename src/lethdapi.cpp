//
//  Copyright (c) 2016-2017 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of pixelboardd.
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

SocketCommPtr LEthDApi::apiConnectionHandler(SocketCommPtr aServerSocketComm)
{
  JsonCommPtr conn = JsonCommPtr(new JsonComm(MainLoop::currentMainLoop()));
  conn->setMessageHandler(boost::bind(&LEthDApi::apiRequestHandler, this, conn, _1, _2));
  conn->setClearHandlersAtClose(); // close must break retain cycles so this object won't cause a mem leak
  LOG(LOG_INFO,"apiConnectionHandler");
  connection = conn;
  return conn;
}


void LEthDApi::apiRequestHandler(JsonCommPtr aConnection, ErrorPtr aError, JsonObjectPtr aRequest)
{
  if (Error::isOK(aError)) {
    LOG(LOG_INFO,"API request: %s", aRequest->c_strValue());
    if (processRequest(aRequest, boost::bind(&LEthDApi::requestHandled, this, aConnection, _1, _2))) {
      // done, callback will send response and close connection
      return;
    }
  }
  // return
  requestHandled(aConnection, JsonObjectPtr(), aError);
}

void LEthDApi::requestHandled(JsonCommPtr aConnection, JsonObjectPtr aResponse, ErrorPtr aError)
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

int LEthDApi::init(JsonObjectPtr aData)
{
  LOG(LOG_INFO,"init");
}

void LEthDApi::echo(JsonObjectPtr aData)
{
  JsonObjectPtr answer = JsonObject::newObj();
  answer->add("answer", JsonObject::newString("ok"));
  connection->sendMessage(answer);
}

bool LEthDApi::processRequest(JsonObjectPtr aData, RequestDoneCB aRequestDoneCB)
{
  ErrorPtr err;
  JsonObjectPtr o;

  string aCmd = aData->get("cmd")->stringValue();

  typedef boost::function<void (JsonObjectPtr aData)> FunctionCB;

  static std::map<string, FunctionCB> m;
  m["init"] = boost::bind(&LEthDApi::init, this, _1);
  m["echo"] = boost::bind(&LEthDApi::echo, this, _1);

  if(m.count(aCmd.c_str())) m[aCmd.c_str()](aData);

  return true;
}

LEthDApi::LEthDApi(TextViewPtr aMessage)
{
  message = aMessage;
}

void LEthDApi::start(const char* aApiPort)
{
  apiServer = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
  apiServer->setConnectionParams(NULL, aApiPort, SOCK_STREAM, AF_INET6);
  apiServer->setAllowNonlocalConnections(true);
  apiServer->startServer(boost::bind(&LEthDApi::apiConnectionHandler, this, _1), 3);
  LOG(LOG_INFO, "LEthDApi listening on %s", aApiPort);
}

void LEthDApi::send(int aValue)
{
  if(!connection) return;
  JsonObjectPtr aMessage = JsonObject::newObj();
  aMessage->add("sensor", JsonObject::newInt32(aValue));
  connection->sendMessage(aMessage);
}
