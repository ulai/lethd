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

using namespace p44;

Neuron::Neuron(LEDChainCommPtr aledChain, NeuronMeasureCB aNeuronMesure, NeuronSpikeCB aNeuronSpike)
{
  ledChain = aledChain;
  neuronMeasure = aNeuronMesure;
  neuronSpike = aNeuronSpike;
}

void Neuron::start(double aMovingAverageCount, double aThreshold)
{
  movingAverageCount = aMovingAverageCount;
  threshold = aThreshold;
  ticketMeasure.executeOnce(boost::bind(&Neuron::measure, this, _1));
}

void Neuron::fire(double aValue)
{
  if(spikeState == SpikeFiring) return;
  neuronSpike(aValue);
  pos = 0;
  spikeState = SpikeFiring;
  ticketAnimateAxon.executeOnce(boost::bind(&Neuron::animateAxon, this, _1));
}

void Neuron::measure(MLTimer &aTimer)
{
  double value = neuronMeasure();
  avg = (avg * (movingAverageCount - 1) + value) / movingAverageCount;
  if(avg > threshold) fire(avg);
  ticketMeasure.executeOnce(boost::bind(&Neuron::measure, this, _1), 10 * MilliSecond);
}

void Neuron::animateAxon(MLTimer &aTimer)
{
  pos++;
  for(int i = 0; i < 50; i++) {
    if(ledChain) ledChain->setColorXY(i, 0, i == pos ? 255 : 0, 0, 0);
  }
  if(ledChain) ledChain->show();
  if(pos < 50) {
    ticketAnimateAxon.executeOnce(boost::bind(&Neuron::animateAxon, this, _1), 10 * MilliSecond);
  } else {
    spikeState = SpikeIdle;
  }
}
