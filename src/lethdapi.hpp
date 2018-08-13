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

#ifndef __lethd_lethdapi_hpp__
#define __lethd_lethdapi_hpp__

#include "p44utils_common.hpp"

#include "jsoncomm.hpp"
#include "textview.hpp"
#include "fader.hpp"

namespace p44 {

  typedef boost::function<void (JsonObjectPtr aResponse, ErrorPtr aError)> RequestDoneCB;
  typedef boost::function<void (JsonObjectPtr aData)> InitFeatureCB;

  class LethdApi : public P44Obj
  {
    SocketCommPtr apiServer;
    JsonCommPtr connection;
    TextViewPtr message;
    FaderPtr fader;
    InitFeatureCB initFeature;

  public:
    LethdApi(TextViewPtr aMessage, FaderPtr aFader, InitFeatureCB aInitFeature);
    void start(const char* aApiPort);
    void send(int aValue);

  private:
    SocketCommPtr apiConnectionHandler(SocketCommPtr aServerSocketComm);
    void apiRequestHandler(JsonCommPtr aConnection, ErrorPtr aError, JsonObjectPtr aRequest);
    void requestHandled(JsonCommPtr aConnection, JsonObjectPtr aResponse, ErrorPtr aError);
    void init(JsonObjectPtr aData);
    void now(JsonObjectPtr aData);
    void fade(JsonObjectPtr aData);
    ErrorPtr processRequest(JsonObjectPtr aData);
  };

  typedef boost::intrusive_ptr<LethdApi> LethdApiPtr;

  class LethdApiError : public Error
  {
  public:
    static const char *domain() { return "LehtdApiError"; }
    virtual const char *getErrorDomain() const { return LethdApiError::domain(); };
    LethdApiError() : Error(Error::NotOK) {};

    /// factory method to create string error fprint style
    static ErrorPtr err(const char *aFmt, ...) __printflike(1,2);
  };

} // namespace p44



#endif /* __lethd_lethdapi_hpp__ */
