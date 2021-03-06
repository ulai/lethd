//
//  Copyright (c) 2016-2018 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#ifndef __pixelboardd_viewstack_hpp__
#define __pixelboardd_viewstack_hpp__

#include "view.hpp"

namespace p44 {

  class ViewStack : public View
  {
    typedef View inherited;

    typedef std::list<ViewPtr> ViewsList;

    ViewsList viewStack;

  public :

    /// create view stack
    ViewStack();

    virtual ~ViewStack();

    /// push view onto top of stack
    /// @param aView the view to push in front of all other views
    void pushView(ViewPtr aView);

    /// remove topmost view
    void popView();

    /// remove specific view
    /// @param aView the view to remove from the stack
    void removeView(ViewPtr aView);


    /// clear stack, means remove all views
    virtual void clear() P44_OVERRIDE;

    /// calculate changes on the display, return time of next change
    /// @return Infinite if there is no immediate need to call step again, otherwise mainloop time of when to call again latest
    /// @note this must be called as demanded by return value, and after making changes to the view
    virtual MLMicroSeconds step() P44_OVERRIDE;

    /// return if anything changed on the display since last call
    virtual bool isDirty() P44_OVERRIDE;

    /// call when display is updated
    virtual void updated() P44_OVERRIDE;

  protected:

    /// get content pixel color
    /// @param aX content X coordinate
    /// @param aY content Y coordinate
    /// @note aX and aY are NOT guaranteed to be within actual content as defined by contentSizeX/Y
    ///   implementation must check this!
    virtual PixelColor contentColorAt(int aX, int aY) P44_OVERRIDE;

  };
  typedef boost::intrusive_ptr<ViewStack> ViewStackPtr;

} // namespace p44



#endif /* __pixelboardd_viewstack_hpp__ */
