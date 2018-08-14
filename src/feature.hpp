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

#ifndef __lethd_feature_hpp__
#define __lethd_feature_hpp__

#include "p44utils_common.hpp"

namespace p44 {

  class Feature : public P44Obj
  {
    bool initialized = false;

  public:
    Feature();
    void initialize();
    bool isInitialized();

  private:

  };

  typedef boost::intrusive_ptr<Feature> FeaturePtr;

} // namespace p44



#endif /* __lethd_feature_hpp__ */
