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

#ifndef __pixelboardd_viewscroller_hpp__
#define __pixelboardd_viewscroller_hpp__

#include "view.hpp"

namespace p44 {

  /// Smooth scrolling (subpixel resolution)
  class ViewScroller : public View
  {
    typedef View inherited;

  public:

    enum {
      noWrap = 0, /// do not wrap
      wrapX = 0x01, /// wrap in X direction
      wrapY = 0x02, /// wrap in Y direction
    };
    typedef uint8_t ScrollMode;

  private:

    ViewPtr scrolledView;

    double scrollOffsetX; ///< X distance from this view's content origin to the scrolled view's origin
    double scrollOffsetY; ///< Y distance from this view's content origin to the scrolled view's origin
    ScrollMode scrollMode; ///< wrap modes

  protected:

    /// get content pixel color
    /// @param aX content X coordinate
    /// @param aY content Y coordinate
    /// @note aX and aY are NOT guaranteed to be within actual content as defined by contentSizeX/Y
    ///   implementation must check this!
    virtual PixelColor contentColorAt(int aX, int aY) P44_OVERRIDE;

  public :

    /// create view
    ViewScroller();

    virtual ~ViewScroller();

    /// set the to be scrolled view
    /// @param aScrolledView the view of which a part should be shown in this view.
    void setScrolledView(ViewPtr aScrolledView) { scrolledView = aScrolledView; makeDirty(); }

    /// set scroll offsets
    /// @param aOffsetX X direction scroll offset, subpixel distances allowed
    /// @param aOffsetY Y direction scroll offset, subpixel distances allowed
    /// @note the scroll offsets describe the distance from this view's content origin (not its origin on the parent view!)
    ///   to the scrolled view's origin (not its content origin)
    void setOffsets(double aOffsetX, double aOffsetY) { scrollOffsetX = aOffsetX; scrollOffsetY = aOffsetY; makeDirty(); }

    /// @return the current X scroll offset
    double offsetX() { return scrollOffsetX; };

    /// @return the current Y scroll offset
    double offsetY() { return scrollOffsetY; };

    /// set scroll mode
    /// @param aScrollMode set the scroll mode (wraparound options)
    void setScrollMode(ScrollMode aScrollMode) { scrollMode = aScrollMode; makeDirty(); }

    /// clear contents of this view
    virtual void clear() P44_OVERRIDE;

    /// calculate changes on the display, return true if any
    /// @return true if complete, false if step() would like to be called immediately again
    /// @note this is called on the active page at least once per mainloop cycle
    virtual bool step() P44_OVERRIDE;

    /// return if anything changed on the display since last call
    virtual bool isDirty() P44_OVERRIDE;

    /// call when display is updated
    virtual void updated() P44_OVERRIDE;

  };
  typedef boost::intrusive_ptr<ViewScroller> ViewScrollerPtr;

} // namespace p44



#endif /* __pixelboardd_viewscroller_hpp__ */
