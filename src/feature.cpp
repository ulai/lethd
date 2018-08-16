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

#include "feature.hpp"

using namespace p44;

Feature::Feature(const string aName) :
  name(aName),
  initialized(false)
{
}


bool Feature::isInitialized() const
{
  return initialized;
}


ErrorPtr Feature::processRequest(ApiRequestPtr aRequest)
{
  return LethdApiError::err("Feature '%s' cannot understand this request", getName().c_str());
}


void Feature::reset()
{
  initialized = false;
}
