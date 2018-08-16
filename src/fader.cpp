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

#include "fader.hpp"
#include "application.hpp"

using namespace p44;

Fader::Fader(AnalogIoPtr aPwmDimmer) :
  inherited("light"),
  pwmDimmer(aPwmDimmer)
{
  // check for commandline-triggered standalone operation
  if (CmdLineApp::sharedCmdLineApp()->getOption("light")) {
    setInitialized();
  }
}

// MARK: ==== fader API

ErrorPtr Fader::initialize(JsonObjectPtr aInitData)
{
  LOG(LOG_INFO, "initializing fader");
  initOperation();
  return Error::ok();
}


ErrorPtr Fader::processRequest(ApiRequestPtr aRequest)
{
  JsonObjectPtr o = aRequest->getRequest()->get("cmd");
  if (!o) {
    return LethdApiError::err("missing 'cmd'");
  }
  string cmd = o->stringValue();
  if (cmd=="fade") {
    return fade(aRequest);
  }
  if (cmd=="status") {
    JsonObjectPtr answer = JsonObject::newObj();
    answer->add("brightness", JsonObject::newDouble(current()));
    aRequest->sendResponse(answer, ErrorPtr());
    return ErrorPtr();
  }
  return LethdApiError::err("unknown cmd '%s'", cmd.c_str());
}


ErrorPtr Fader::fade(ApiRequestPtr aRequest)
{
  JsonObjectPtr data = aRequest->getRequest();
  double from = current();
  if(data->get("from")) from = data->get("from")->doubleValue();
  double to = data->get("to")->doubleValue();
  MLMicroSeconds t = data->get("t")->int64Value() * MilliSecond;
  MLMicroSeconds start = MainLoop::now();
  if(data->get("start")) MainLoop::unixTimeToMainLoopTime(data->get("start")->int64Value() * MilliSecond);
  fade(from, to, t, start);
  return Error::ok();
}


// MARK: ==== fader operation


void Fader::initOperation()
{
  LOG(LOG_INFO, "initializing fader");
  setInitialized();
}


void Fader::fade(double aFrom, double aTo, MLMicroSeconds aFadeTime, MLMicroSeconds aStartTime)
{
  if(fabs(aFrom - aTo) < 1e-4) return;
  currentValue = aFrom;
  to = aTo;
  dv = (aTo - aFrom) / (aFadeTime / dt);
  ticket.executeOnceAt(boost::bind(&Fader::update, this, _1), aStartTime);
}

void Fader::update(MLTimer &aTimer)
{
  double newValue = currentValue + dv;
  bool done = false;
  if((dv > 0 && newValue >= to) || (dv < 0 && newValue <= to)) {
    newValue = to;
    done = true;
  }
  currentValue = newValue;
  LOG(LOG_DEBUG, "New fader value = %.1f", currentValue);
  pwmDimmer->setValue(brightnessToPWM(currentValue));
  if(!done) MainLoop::currentMainLoop().retriggerTimer(aTimer, dt);
}

double Fader::current()
{
  return currentValue;
}

double Fader::brightnessToPWM(double aBrightness)
{
  return 100*((exp(aBrightness*4/1)-1)/(exp(4)-1));
}
