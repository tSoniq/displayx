/** @file   DXDemoOpenGLView.h
 *  @brief  Custom OpenGL view used to display raw image data.
 *
 *  Copyright (c) 2010 tSoniq. All rights reserved.
 */

#import <Cocoa/Cocoa.h>
#include <OpenGL/gl.h>

@interface DXDemoOpenGLView : NSOpenGLView
{
    class Texture* _desktopTexture;
    class Texture* _cursorTexture;
    unsigned _cursorX;
    unsigned _cursorY;
    bool _cursorIsVisible;
}

- (void)setBlank;
- (void)setDesktop:(const uint32_t*)bitmap width:(unsigned)width height:(unsigned)height;
- (void)updateDesktop:(const uint32_t*)bitmap regionX:(unsigned)x regionY:(unsigned)y regionWidth:(unsigned)width regionHeight:(unsigned)height;
- (void)setCursor:(uint32_t*)bitmap width:(unsigned)width height:(unsigned)height x:(unsigned)x y:(unsigned)y isVisible:(bool)isVisible;

@end
