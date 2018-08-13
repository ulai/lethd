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

#ifndef __lethd_lethdapi_hpp__
#define __lethd_lethdapi_hpp__

#include "p44utils_common.hpp"

#include "jsoncomm.hpp"

namespace p44 {

  typedef boost::function<void (JsonObjectPtr aResponse, ErrorPtr aError)> RequestDoneCB;

  class LEthDApi : public P44Obj
  {
    SocketCommPtr apiServer;
    JsonCommPtr connection;

  public:
    LEthDApi();
    void start(const char* aApiPort);
    void send(int aValue);

  private:
    SocketCommPtr apiConnectionHandler(SocketCommPtr aServerSocketComm);
    void apiRequestHandler(JsonCommPtr aConnection, ErrorPtr aError, JsonObjectPtr aRequest);
    void requestHandled(JsonCommPtr aConnection, JsonObjectPtr aResponse, ErrorPtr aError);
    int init(JsonObjectPtr aData);
    void echo(JsonObjectPtr aData);
    bool processRequest(JsonObjectPtr aData, RequestDoneCB aRequestDoneCB);
  };

  typedef boost::intrusive_ptr<LEthDApi> LEthDApiPtr;

} // namespace p44



#endif /* __lethd_lethdapi_hpp__ */
