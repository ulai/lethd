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

#ifndef __lethd_neuron_hpp__
#define __lethd_neuron_hpp__

#include "feature.hpp"
#include "ledchaincomm.hpp"

namespace p44 {

  typedef boost::function<double ()> NeuronMeasureCB;
  typedef boost::function<void (double)> NeuronSpikeCB;

  class Neuron : public Feature
  {
    LEDChainCommPtr ledChain;
    NeuronMeasureCB neuronMeasure;
    NeuronSpikeCB neuronSpike;

    double movingAverageCount = 20;
    double threshold = 250;
    double avg = 0;

    enum SensorState { SensorIdle, SensorHit };
    SensorState sensorState = SensorIdle;
    enum SpikeState { SpikeIdle, SpikeFiring };
    SpikeState spikeState = SpikeIdle;

    MLTicket ticketMeasure;
    MLTicket ticketAnimateAxon;

    int pos = 0;

  public:
    Neuron(LEDChainCommPtr aledChain, NeuronMeasureCB neuronMeasure, NeuronSpikeCB neuronSpike);
    void start(double aMovingAverageCount, double aThreshold);
    void fire(double aValue = 0);

  private:
    void measure(MLTimer &aTimer);
    void animateAxon(MLTimer &aTimer);

  };

  typedef boost::intrusive_ptr<Neuron> NeuronPtr;

} // namespace p44



#endif /* __lethd_neuron_hpp__ */
