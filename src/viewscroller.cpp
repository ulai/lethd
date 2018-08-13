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
  scrollOffsetX_milli(0),
  scrollOffsetY_milli(0),
  scrollStepX_milli(0),
  scrollStepY_milli(0),
  scrollSteps(0),
  scrollStepInterval(Never),
  nextScrollStepAt(Never)
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


#define BUSYWAIT_TIME_TO_STEP (1*MilliSecond)

bool ViewScroller::step()
{
  bool complete = inherited::step();
  if (scrolledView && !scrolledView->step()) {
    complete = false;
  }
  // scroll
  if (scrollSteps!=0 && scrollStepInterval>0) {
    // scrolling
    MLMicroSeconds now = MainLoop::now();
    MLMicroSeconds next = nextScrollStepAt-now; // time to next step
    if (next>0) {
      if (next<BUSYWAIT_TIME_TO_STEP) {
        // we want to be called quickly again when next step is near
        complete = false;
      }
    }
    else {
      // execute all step(s) pending
      // Note: will catch up in case step() was not called often enough
      while (next<=0) {
        if (next<-10*MilliSecond) {
          LOG(LOG_DEBUG, "ViewScroller: Warning: precision below 10mS: %lld uS after precise time", next);
        }
        // perform step
        scrollOffsetX_milli += scrollStepX_milli;
        scrollOffsetY_milli += scrollStepY_milli;
        makeDirty();
        // limit coordinate increase in wraparound scroll view
        if (scrolledView) {
          WrapMode wm = scrolledView->getWrapMode();
          if (wm&wrapX) {
            long csx_milli = scrolledView->getContentSizeX()*1000;
            if ((wm&wrapXmax) && scrollOffsetX_milli>=csx_milli)
              scrollOffsetX_milli-=csx_milli;
            if ((wm&wrapXmin) && scrollOffsetX_milli<0)
              scrollOffsetX_milli+=csx_milli;
          }
          if (wm&wrapY) {
            long csy_milli = scrolledView->getContentSizeY()*1000;
            if ((wm&wrapYmax) && scrollOffsetY_milli>=csy_milli)
              scrollOffsetY_milli-=csy_milli;
            if ((wm&wrapYmin) && scrollOffsetY_milli<0)
              scrollOffsetY_milli+=csy_milli;
          }
        }
        // check scroll end
        if (scrollSteps>0) {
          scrollSteps--;
          if (scrollSteps==0) {
            // scroll ends here
            if (scrollCompletedCB) {
              SimpleCB cb = scrollCompletedCB;
              scrollCompletedCB = NULL;
              cb(); // may set up another callback already
            }
            break;
          }
        }
        // advance to next step
        next += scrollStepInterval;
        nextScrollStepAt += scrollStepInterval;
        if (next<0) {
          LOG(LOG_INFO, "ViewScroller: needs to catch-up steps -> call step() more often!");
        }
      }
    }
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
  int sampleOffsetX = (int)((scrollOffsetX_milli+(scrollOffsetX_milli>0 ? 500 : -500))/1000);
  int sampleOffsetY = (int)((scrollOffsetY_milli+(scrollOffsetY_milli>0 ? 500 : -500))/1000);
  int subSampleOffsetX = 1;
  int subSampleOffsetY = 1;
  int outsideWeightX = (int)((scrollOffsetX_milli-(long)sampleOffsetX*1000)*255/1000);
  if (outsideWeightX<0) { outsideWeightX *= -1; subSampleOffsetX = -1; }
  int outsideWeightY = (int)((scrollOffsetY_milli-(long)sampleOffsetY*1000)*255/1000);
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


void ViewScroller::startScroll(double aStepX, double aStepY, MLMicroSeconds aInterval, long aNumSteps, MLMicroSeconds aStartTime, SimpleCB aCompletedCB)
{
  scrollStepX_milli = aStepX*1000;
  scrollStepY_milli = aStepY*1000;
  scrollStepInterval = aInterval;
  scrollSteps = aNumSteps;
  nextScrollStepAt = aStartTime==Never ? MainLoop::now() : aStartTime;
  scrollCompletedCB = aCompletedCB;
}


void ViewScroller::stopScroll()
{
  // no more steps
  scrollSteps = 0;
}

