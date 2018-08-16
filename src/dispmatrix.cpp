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

#include "dispmatrix.hpp"
#include "application.hpp"


#define LED_MODULE_COLS 74
#define LED_MODULE_ROWS 7
#define LED_MODULE_BORDER_LEFT 1
#define LED_MODULE_BORDER_RIGHT 1


using namespace p44;


// MARK: ===== DispPanel

DispPanel::DispPanel(const string aChainName, int aOffsetX, int aRows, int aCols, int aBorderLeft, int aBorderRight, int aOrientation) :
  offsetX(aOffsetX),
  rows(aRows),
  cols(aCols),
  borderLeft(aBorderLeft),
  borderRight(aBorderRight),
  orientation(aOrientation)
{
  // create chain driver
  chain = LEDChainCommPtr(new LEDChainComm(LEDChainComm::ledtype_ws281x, aChainName, rows*cols, cols, false, true));
  chain->begin();
  // create views
  message = TextViewPtr(new TextView);
  message->setFrame(0, 0, 2000, 7);
  message->setBackGroundColor(transparent);
  message->setWrapMode(View::wrapX);
  dispView = ViewScrollerPtr(new ViewScroller);
  dispView->setFrame(0, 0, cols-borderLeft-borderRight, rows);
  dispView->setFullFrameContent();
  dispView->setOrientation(orientation);
  dispView->setBackGroundColor(black); // not transparent!
  dispView->setScrolledView(message);

  // %%% for now
  message->setText("Hello World +++ ");
  dispView->startScroll(0.25, 0, 20*MilliSecond);

  // position main view
  dispView->setOffsetX(-offsetX);
  // show operation status: dim green in first LED (if invisible), dim blue in last LED (if invisible)
  if (borderLeft>0) {
    chain->setColorXY(0, 0, 0, 100, 0);
  }
  if (borderRight>0) {
    chain->setColorXY(cols-1, rows-1, 0, 0, 100);
  }
  chain->show();
}


DispPanel::~DispPanel()
{
  chain->end();
}



#define MAX_STEP_INTERVAL (20*MilliSecond)


MLMicroSeconds DispPanel::step()
{
  MLMicroSeconds nextCall = Infinite;
  if (dispView) {
    do {
      nextCall = dispView->step();
    } while (nextCall==0);
    updateDisplay();
  }
  return nextCall;
}


void DispPanel::updateDisplay()
{
  if (dispView && dispView->isDirty()) {
    for (int x=borderRight; x<cols-borderLeft; x++) {
      for (int y=0; y<rows; y++) {
        PixelColor p = dispView->colorAt(x-borderRight, y);
        PixelColor dp = dimmedPixel(p, p.a);
        chain->setColorXY(x, y, dp.r, dp.g, dp.b);
      }
    }
    chain->show();
    dispView->updated();
  }
}


void DispPanel::setOffsetX(double aOffsetX)
{
  if (dispView) dispView->setOffsetX(aOffsetX+offsetX);
}



// MARK: ===== DispMatrix


DispMatrix::DispMatrix(const string aChainName1, const string aChainName2, const string aChainName3) :
  inherited("text"),
  usedPanels(0)
{
  // save chain names
  chainNames[0] = aChainName1;
  chainNames[1] = aChainName2;
  chainNames[2] = aChainName3;
  // check for commandline-triggered standalone operation
  string cfg;
  if (CmdLineApp::sharedCmdLineApp()->getStringOption("dispmatrix", cfg)) {
    int numCols = LED_MODULE_COLS;
    int numRows = LED_MODULE_ROWS;
    sscanf(cfg.c_str(), "%d,%d", &numCols, &numRows);
    // instantiate a single panel
    panels[usedPanels] = DispPanelPtr(new DispPanel(chainNames[usedPanels], 0, numRows, numCols, LED_MODULE_BORDER_LEFT, LED_MODULE_BORDER_RIGHT, View::right));
    usedPanels++;
    initOperation();
  }
}


void DispMatrix::reset()
{
  stepTicket.cancel();
  for (int i=0; i<usedPanels; ++i) {
    panels[i].reset();
  }
  inherited::reset();
}


DispMatrix::~DispMatrix()
{
  reset();
}


// MARK: ==== dispmatrix API


ErrorPtr DispMatrix::initialize(JsonObjectPtr aInitData)
{
  LOG(LOG_INFO, "initializing dispmatrix");
  reset();
  if (!aInitData->isType(json_type_array)) {
    return LethdApiError::err("init data must be array of panel specs");
  }
  for (int i = 0; i<aInitData->arrayLength(); ++i) {
    JsonObjectPtr panelCfg = aInitData->arrayGet(i);
    int rows = LED_MODULE_ROWS;
    int visiblecols = LED_MODULE_COLS;
    int borderLeft = LED_MODULE_BORDER_LEFT;
    int borderRight = LED_MODULE_BORDER_RIGHT;
    int orientation = View::right;
    int offsetX = 0;
    // configure
    JsonObjectPtr o;
    // - usually
    if (panelCfg->get("cols", o)) {
      visiblecols = o->int32Value();
    }
    if (panelCfg->get("orientation", o)) {
      orientation = o->int32Value();
    }
    if (panelCfg->get("offset", o)) {
      offsetX = o->int32Value();
    }
    // - special cases
    if (panelCfg->get("rows", o)) {
      rows = o->int32Value();
    }
    if (panelCfg->get("borderleft", o)) {
      borderLeft = o->int32Value();
    }
    if (panelCfg->get("borderright", o)) {
      borderRight = o->int32Value();
    }
    // now create panel
    if (usedPanels>=numChains) {
      return LethdApiError::err("cannot create more than %d display panels", numChains);
    }
    int cols = visiblecols+borderLeft+borderRight;
    panels[usedPanels] = DispPanelPtr(new DispPanel(chainNames[usedPanels], offsetX, rows, cols, borderLeft, borderRight, orientation));
    usedPanels++;
  }
  initOperation();
  return Error::ok();
}



#define MIN_SCROLL_STEP_INTERVAL (20*MilliSecond)

#define FOR_EACH_PANEL(m) for(int i=0; i<usedPanels; ++i) { panels[i]->m; }


ErrorPtr DispMatrix::processRequest(ApiRequestPtr aRequest)
{
  JsonObjectPtr data = aRequest->getRequest();
  JsonObjectPtr o = data->get("cmd");
  if (o) {
    // decode commands
    string cmd = o->stringValue();
    if (cmd=="stopscroll") {
      FOR_EACH_PANEL(dispView->stopScroll());
      return Error::ok();
    }
    else if (cmd=="startscroll") {
      double stepx = 0.25;
      double stepy = 0;
      long steps = -1; // forever
      MLMicroSeconds interval = 20*MilliSecond;
      MLMicroSeconds start = Never; // right away
      if (data->get("stepx", o)) {
        stepx = o->doubleValue();
      }
      if (data->get("stepy", o)) {
        stepy = o->doubleValue();
      }
      if (data->get("steps", o)) {
        steps = o->int64Value();
      }
      if (data->get("interval", o)) {
        interval = o->doubleValue()*MilliSecond;
      }
      if (data->get("start", o)) {
        start = MainLoop::unixTimeToMainLoopTime(o->int64Value());
      }
      if (interval<MIN_SCROLL_STEP_INTERVAL) interval = MIN_SCROLL_STEP_INTERVAL;
      FOR_EACH_PANEL(dispView->startScroll(stepx, stepy, interval, steps, start));
      return Error::ok();
    }
    else if (cmd=="fade") {
      int to = 255;
      MLMicroSeconds t = 300*MilliSecond;
      if (data->get("to", o)) {
        to = o->doubleValue()*2.55;
      }
      if (data->get("t", o)) {
        t = o->doubleValue()*MilliSecond;
      }
      FOR_EACH_PANEL(dispView->fadeTo(to, t));
      return Error::ok();
    }
    else if (cmd=="status") {
      JsonObjectPtr answer = JsonObject::newObj();
      if (usedPanels>0) {
        DispPanelPtr p = panels[0];
        if (p->message) {
          answer->add("text", JsonObject::newString(p->message->getText()));
          answer->add("color", JsonObject::newString(pixelToWebColor(p->message->getTextColor())));
          answer->add("spacing", JsonObject::newInt32(p->message->getTextSpacing()));
          answer->add("backgroundcolor", JsonObject::newString(pixelToWebColor(p->message->getBackGroundColor())));
        }
        if (p->dispView) {
          answer->add("brightness", JsonObject::newDouble((double)p->dispView->getAlpha()/2.55));
          answer->add("scrolloffsetx", JsonObject::newDouble(p->dispView->getOffsetX()));
          answer->add("scrolloffsety", JsonObject::newDouble(p->dispView->getOffsetY()));
          answer->add("scrollstepx", JsonObject::newDouble(p->dispView->getStepX()));
          answer->add("scrollstepy", JsonObject::newDouble(p->dispView->getStepY()));
          answer->add("scrollsteptime", JsonObject::newDouble(p->dispView->getScrollStepInterval()/MilliSecond));
          answer->add("unixtime", JsonObject::newInt64(MainLoop::unixtime()));
        }
      }
      aRequest->sendResponse(answer, ErrorPtr());
      return ErrorPtr();
    }
    return LethdApiError::err("unknown cmd '%s'", cmd.c_str());
  }
  else {
    // decode properties
    if (data->get("text", o)) {
      string msg = o->stringValue();
      FOR_EACH_PANEL(message->setText(msg));
    }
    if (data->get("color", o)) {
      PixelColor p = webColorToPixel(o->stringValue());
      FOR_EACH_PANEL(message->setTextColor(p));
    }
    if (data->get("backgroundcolor", o)) {
      PixelColor p = webColorToPixel(o->stringValue());
      FOR_EACH_PANEL(message->setBackGroundColor(p));
    }
    if (data->get("spacing", o)) {
      int spacing = o->int32Value();
      FOR_EACH_PANEL(message->setTextSpacing(spacing));
    }
    if (data->get("offsetx", o)) {
      double offs = o->int32Value();
      FOR_EACH_PANEL(setOffsetX(offs));
    }
    if (data->get("offsety", o)) {
      double offs = o->int32Value();
      FOR_EACH_PANEL(dispView->setOffsetY(offs));
    }
    return Error::ok();
  }
}


// MARK: ==== dispmatrix operation

void DispMatrix::initOperation()
{
  stepTicket.executeOnce(boost::bind(&DispMatrix::step, this, _1));
  setInitialized();
}


#define MAX_STEP_INTERVAL (20*MilliSecond)

void DispMatrix::step(MLTimer &aTimer)
{
  MLMicroSeconds nextCall = Infinite;
  for (int i=0; i<usedPanels; ++i) {
    MLMicroSeconds n = panels[i]->step();
    if (nextCall<0 || (n>0 && n<nextCall)) {
      nextCall = n;
    }
  }
  MLMicroSeconds now = MainLoop::now();
  if (nextCall<0 || nextCall-now>MAX_STEP_INTERVAL) {
    nextCall = now+MAX_STEP_INTERVAL;
  }
  MainLoop::currentMainLoop().retriggerTimer(aTimer, nextCall, 0, MainLoop::absolute);
}



/*
    else if (aUri=="brightness") {
      if (aIsAction) {
        // set
        if (message && aData->get("text", o)) {
          // text brightness
          dispView->setAlpha(o->doubleValue()/100*255);
        }
        if (pwmDimmer && aData->get("light", o)) {
          // light brightness
          pwmDimmer->setValue(o->doubleValue());
        }
      }
      // get
      JsonObjectPtr answer = JsonObject::newObj();
      if (message) answer->add("text", JsonObject::newInt32((double)(message->getAlpha())/255.0*100));
      if (pwmDimmer) answer->add("light", JsonObject::newDouble(pwmDimmer->value()));
      aRequestDoneCB(answer, ErrorPtr());
      return true;
    }
    else if (aUri=="text") {
      if (aIsAction) {
        bool doneSomething = false;
        if (aData->get("message", o)) {
          if (message) message->setText(o->stringValue());
          doneSomething = true;
        }
        if (aData->get("scrolloffsetx", o)) {
          if (dispView) dispView->setOffsetX(o->doubleValue());
          doneSomething = true;
        }
        if (aData->get("scrolloffsety", o)) {
          if (dispView) dispView->setOffsetY(o->doubleValue());
          doneSomething = true;
        }
        if (aData->get("scrollstepx", o)) {
          scrollStepX = o->doubleValue();
          if (dispView) dispView->startScroll(scrollStepX, scrollStepY, scrollStepInterval, numScrollSteps);
          doneSomething = true;
        }
        if (aData->get("scrollstepy", o)) {
          scrollStepY = o->doubleValue();
          if (dispView) dispView->startScroll(scrollStepX, scrollStepY, scrollStepInterval, numScrollSteps);
          doneSomething = true;
        }
        if (aData->get("scrollsteptime", o)) {
          scrollStepInterval = o->doubleValue()*MilliSecond;
          if (dispView) dispView->startScroll(scrollStepX, scrollStepY, scrollStepInterval, numScrollSteps);
          doneSomething = true;
        }
        if (aData->get("scrollsteps", o)) {
          int s = o->int32Value();
          if (dispView) {
            if (s==0) {
              dispView->stopScroll();
            }
            else {
              numScrollSteps = s;
              dispView->startScroll(scrollStepX, scrollStepY, scrollStepInterval, numScrollSteps);
            }
          }
          doneSomething = true;
        }
        if (aData->get("scrollstart", o)) {
          MLMicroSeconds start = MainLoop::unixTimeToMainLoopTime(o->int64Value());
          if (dispView) dispView->startScroll(scrollStepX, scrollStepY, scrollStepInterval, numScrollSteps, start);
          doneSomething = true;
        }
        if (aData->get("color", o)) {
          PixelColor p = webColorToPixel(o->stringValue());
          if (p.a==255) p.a = message->getAlpha();
          message->setTextColor(p);
          doneSomething = true;
        }
        if (aData->get("backgroundcolor", o)) {
          PixelColor p = webColorToPixel(o->stringValue());
          if (p.a==255) p.a = message->getAlpha();
          message->setBackGroundColor(p);
          doneSomething = true;
        }
        if (aData->get("spacing", o)) {
          message->setTextSpacing(o->int32Value());
          doneSomething = true;
        }
        if (aData->get("orientation", o)) {
          ledOrientation = o->int32Value();
          dispView->setOrientation(ledOrientation);
          doneSomething = true;
        }
        if (aData->get("startx", o)) {
          // start x offset
          dispView->setContentOffset(-o->int32Value(), 0);
          doneSomething = true;
        }
      }
      // get
      JsonObjectPtr answer = JsonObject::newObj();
      if (message) {
        answer->add("text", JsonObject::newString(message->getText()));
        answer->add("color", JsonObject::newString(pixelToWebColor(message->getTextColor())));
        answer->add("spacing", JsonObject::newInt32(message->getTextSpacing()));
        answer->add("backgroundcolor", JsonObject::newString(pixelToWebColor(message->getBackGroundColor())));
      }
      if (dispView) {
        answer->add("scrolloffsetx", JsonObject::newDouble(dispView->getOffsetX()));
        answer->add("scrolloffsety", JsonObject::newDouble(dispView->getOffsetY()));
        answer->add("scrollstepx", JsonObject::newDouble(dispView->getStepX()));
        answer->add("scrollstepy", JsonObject::newDouble(dispView->getStepY()));
        answer->add("scrollsteptime", JsonObject::newDouble(dispView->getScrollStepInterval()/MilliSecond));
        answer->add("unixtime", JsonObject::newInt64(MainLoop::unixtime()));
      }
      aRequestDoneCB(answer, ErrorPtr());
      return true;
    }
    else if (aUri=="inputs") {
      if (!aIsAction) {
        JsonObjectPtr answer = JsonObject::newObj();
        if (sensor0) answer->add("sensor0", JsonObject::newInt32(sensor0->value()));
        if (sensor1) answer->add("sensor1", JsonObject::newInt32(sensor1->value()));
        aRequestDoneCB(answer, ErrorPtr());
        return true;
      }
    }
 */
