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

#include "neuron.hpp"
#include "application.hpp"
#include <boost/algorithm/string.hpp>

using namespace p44;

Neuron::Neuron(const string aLedChain1Name, const string aLedChain2Name, AnalogIoPtr aSensor, NeuronSpikeCB aNeuronSpike) :
  inherited("neuron")
{
  ledChainName = aLedChain1Name;
  sensor = aSensor;
  neuronSpike = aNeuronSpike;
  // check for commandline-triggered standalone operation
  string s;
  if (CmdLineApp::sharedCmdLineApp()->getStringOption("neuron", s)) {
    initOperation();
    std::vector<std::string> neuronOptions;
    boost::split(neuronOptions, s, boost::is_any_of(","), boost::token_compress_on);
    int movingAverageCount = atoi(neuronOptions[0].c_str());
    int threshold = atoi(neuronOptions[1].c_str());
    start(movingAverageCount, threshold);
  }
}


// MARK: ==== neuron API


ErrorPtr Neuron::initialize(JsonObjectPtr aInitData)
{
  initOperation();
  JsonObjectPtr o;
  if (!aInitData->get("movingAverageCount", o, true)) {
    return LethdApiError::err("missing 'movingAverageCount'");
  }
  double movingAverageCount = o->doubleValue();
  if (!aInitData->get("threshold", o, true)) {
    return LethdApiError::err("missing 'threshold'");
  }
  double threshold = o->doubleValue();
  // start
  start(movingAverageCount, threshold);
  setInitialized();
  return Error::ok();
}


ErrorPtr Neuron::processRequest(ApiRequestPtr aRequest)
{
  JsonObjectPtr o = aRequest->getRequest()->get("cmd");
  if (!o) {
    return LethdApiError::err("missing 'cmd'");
  }
  string cmd = o->stringValue();
  if (cmd=="fire") {
    return fire(aRequest);
  }
  return LethdApiError::err("unknown cmd '%s'", cmd.c_str());
}


ErrorPtr Neuron::fire(ApiRequestPtr aRequest)
{
  fire();
  return Error::ok();
}


// MARK: ==== neuron operation


void Neuron::initOperation()
{
  LOG(LOG_INFO, "initializing neuron");
  ledChain = LEDChainCommPtr(new LEDChainComm(LEDChainComm::ledtype_ws281x, ledChainName, 100));
  ledChain->begin();
  ledChain->show();
  setInitialized();
}


void Neuron::start(double aMovingAverageCount, double aThreshold)
{
  movingAverageCount = aMovingAverageCount;
  threshold = aThreshold;
  ticketMeasure.executeOnce(boost::bind(&Neuron::measure, this, _1));
}

void Neuron::fire(double aValue)
{
  if(axonState == AxonFiring) return;
  neuronSpike(aValue);
  pos = 0;
  axonState = AxonFiring;
  ticketAnimateAxon.executeOnce(boost::bind(&Neuron::animateAxon, this, _1));
}

void Neuron::measure(MLTimer &aTimer)
{
  double value = sensor->value();
  avg = (avg * (movingAverageCount - 1) + value) / movingAverageCount;
  if(avg > threshold) fire(avg);
  ticketMeasure.executeOnce(boost::bind(&Neuron::measure, this, _1), 10 * MilliSecond);
}

void Neuron::animateAxon(MLTimer &aTimer)
{
  for(int i = 0; i < numAxonLeds; i++) {
    if(ledChain) ledChain->setColorXY(i, 0, i == pos ? 255 : 0, 0, 0);
  }
  if(ledChain) ledChain->show();
  if(pos++ < numAxonLeds) {
    ticketAnimateAxon.executeOnce(boost::bind(&Neuron::animateAxon, this, _1), 10 * MilliSecond);
  } else {
    axonState = AxonIdle;
  }
}

void Neuron::animateBody(MLTimer &aTimer)
{
  for(int i = 0; i < numBodyLeds; i++) {
    if(ledChain) ledChain->setColorXY(i, 0, i == pos ? 255 : 0, 0, 0);
  }
  if(ledChain) ledChain->show();
  if(pos++ < numAxonLeds) {
    ticketAnimateAxon.executeOnce(boost::bind(&Neuron::animateAxon, this, _1), 10 * MilliSecond);
  } else {
    axonState = AxonIdle;
  }
}
