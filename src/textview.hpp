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

#ifndef __pixelboardd_textview_hpp__
#define __pixelboardd_textview_hpp__

#include "p44utils_common.hpp"

#include "view.hpp"

namespace p44 {

  class TextView : public View
  {
    typedef View inherited;

    // text parameters
    PixelColor textColor;

    // text rendering
    string text; ///< internal representation of text
    int textSpacing; ///< pixels between characters
    string textPixelCols; ///< string of text column bytes

  public :

    TextView();

    virtual ~TextView();

    /// set new text
    /// @note: sets the content size of the view according to the text
    void setText(const string aText);

    /// get current text
    string getText() const { return text; }

    /// set new text color
    void setTextColor(PixelColor aTextColor) { textColor = aTextColor; makeDirty(); }

    /// get text color
    PixelColor getTextColor() const { return textColor; }

    /// set new text color
    void setTextSpacing(int aTextSpacing) { textSpacing = aTextSpacing; renderText(); }

    /// get text color
    int getTextSpacing() const { return textSpacing; }

    /// clear contents of this view
    virtual void clear() P44_OVERRIDE;

  protected:

    /// get content color at X,Y
    virtual PixelColor contentColorAt(int aX, int aY) P44_OVERRIDE;

  private:

    void renderText();

  };
  typedef boost::intrusive_ptr<TextView> TextViewPtr;


} // namespace p44



#endif /* __pixelboardd_textview_hpp__ */
