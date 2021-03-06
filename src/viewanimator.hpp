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

#ifndef __pixelboardd_viewanimator_hpp__
#define __pixelboardd_viewanimator_hpp__

#include "view.hpp"

namespace p44 {


  class AnimationStep
  {
  public:
    ViewPtr view;
    MLMicroSeconds fadeInTime;
    MLMicroSeconds showTime;
    MLMicroSeconds fadeOutTime;
  };


  
  class ViewAnimator : public View
  {
    typedef View inherited;

    typedef std::vector<AnimationStep> SequenceVector;

    SequenceVector sequence; ///< sequence
    bool repeating; ///< set if current animation is repeating
    int currentStep; ///< current step in running animation
    SimpleCB completedCB; ///< called when one animation run is done
    ViewPtr currentView; ///< current view

    enum {
      as_begin,
      as_show,
      as_fadeout
    } animationState;
    MLMicroSeconds lastStateChange;

  public :

    /// create view stack
    ViewAnimator();

    virtual ~ViewAnimator();

    /// add animation step view to list of animation steps
    /// @param aView the view to add
    void pushStep(ViewPtr aView, MLMicroSeconds aShowTime, MLMicroSeconds aFadeInTime=0, MLMicroSeconds aFadeOutTime=0);

    /// start animating
    /// @param aRepeat if set, animation will repeat
    /// @param aCompletedCB called when animation sequence ends (if repeating, it is called multiple times)
    void startAnimation(bool aRepeat, SimpleCB aCompletedCB = NULL);

    /// stop animation
    /// @note: completed callback will not be called
    void stopAnimation();

    /// clear all steps
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

  private:

    MLMicroSeconds stepAnimation();

  };
  typedef boost::intrusive_ptr<ViewAnimator> ViewAnimatorPtr;

} // namespace p44



#endif /* __pixelboardd_viewanimator_hpp__ */
