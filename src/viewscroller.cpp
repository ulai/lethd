//
//  Copyright (c) 2018 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "viewscroller.hpp"

using namespace p44;


// MARK: ===== ViewScroller


ViewScroller::ViewScroller() :
  scrollOffsetX(0),
  scrollOffsetY(0)
{
}


ViewScroller::~ViewScroller()
{
}



void ViewScroller::clear()
{
  // just delegate
  scrolledView->clear();
}


bool ViewScroller::step()
{
  bool complete = inherited::step();
  if (scrolledView && !scrolledView->step()) {
    complete = false;
  }
  return complete;
}


bool ViewScroller::isDirty()
{
  if (inherited::isDirty()) return true; // dirty anyway
  if (scrolledView) return scrolledView->isDirty();
  return false;
}


void ViewScroller::updated()
{
  inherited::updated();
  if (scrolledView) scrolledView->updated();
}


PixelColor ViewScroller::contentColorAt(int aX, int aY)
{
  if (!scrolledView) return transparent;
  // Note: implementation aims to be efficient at integer scroll offsets in either or both directions
  int sampleOffsetX = (int)lround(scrollOffsetX);
  int sampleOffsetY = (int)lround(scrollOffsetY);
  int subSampleOffsetX = 1;
  int subSampleOffsetY = 1;
  int outsideWeightX = (scrollOffsetX-sampleOffsetX)*255;
  if (outsideWeightX<0) { outsideWeightX *= -1; subSampleOffsetX = -1; }
  int outsideWeightY = (scrollOffsetY-sampleOffsetY)*255;
  if (outsideWeightY<0) { outsideWeightY *= -1; subSampleOffsetY = -1; }
  sampleOffsetX += aX;
  sampleOffsetY += aY;
  PixelColor samp = scrolledView->colorAt(sampleOffsetX, sampleOffsetY);
  if (outsideWeightX!=0) {
    // X Subsampling (and possibly also Y, checked below)
    mixinPixel(samp, scrolledView->colorAt(sampleOffsetX+subSampleOffsetX, sampleOffsetY), outsideWeightX);
    // check if ALSO parts from other pixels in Y direction needed
    if (outsideWeightY!=0) {
      // subsample the Y side neigbours
      PixelColor neighbourY = scrolledView->colorAt(sampleOffsetX, sampleOffsetY+subSampleOffsetY);
      mixinPixel(neighbourY, scrolledView->colorAt(sampleOffsetX+subSampleOffsetX, sampleOffsetY+subSampleOffsetY), outsideWeightX);
      // combine with Y main
      mixinPixel(samp, neighbourY, outsideWeightY);
    }
  }
  else if (outsideWeightY!=0) {
    // only Y subsampling
    mixinPixel(samp, scrolledView->colorAt(sampleOffsetX, sampleOffsetY+subSampleOffsetY), outsideWeightY);
  }
  return samp;
}
