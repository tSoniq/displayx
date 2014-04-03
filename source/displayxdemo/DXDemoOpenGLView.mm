/** @file   DXDemoOpenGLView.mm
 *  @brief
 *
 *  Copyright (c) 2010 tSoniq. All rights reserved.
 */

#import <stdlib.h>
#import <AGL/agl.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/glu.h>
#import "DXDemoOpenGLView.h"

/** Helper class used to manage an OpenGL texture.
 */
class Texture
{
    uint32_t* m_textureData;
    uint32_t m_textureWidth;
    uint32_t m_textureHeight;
    GLuint m_textureId;

public:

    Texture() : m_textureData(0), m_textureWidth(0), m_textureHeight(0)
    {
        glEnable(GL_TEXTURE_RECTANGLE_ARB);
        glGenTextures(1, &m_textureId);
    }

    ~Texture()
    {
        initialise();
        glDeleteTextures(1, &m_textureId);
    }

    void initialise()
    {
        if (m_textureData) free(m_textureData);
        m_textureData = 0;
        m_textureWidth = 0;
        m_textureHeight = 0;
    }


    void initialise(const uint32_t* bitmap, unsigned w, unsigned h)
    {
        // Save the texture data.
        if (!m_textureData || m_textureWidth != w || m_textureHeight != h)
        {
            if (m_textureData) free(m_textureData);
            m_textureWidth = w;
            m_textureHeight = h;
            size_t size = m_textureWidth * m_textureHeight * sizeof (uint32_t);
            void* ptr = 0;
            posix_memalign(&ptr, 256, size);
            m_textureData = (uint32_t*)ptr;

            if (size >= 1024*1024)
            {
                // This should allow the last large (1Mbyte or more) texture assigned a small performance improvement.
                glTextureRangeAPPLE(GL_TEXTURE_RECTANGLE_ARB, (GLsizei)size, m_textureData);
            }
        }

        memcpy(m_textureData, bitmap, m_textureWidth * m_textureHeight * sizeof (uint32_t));

        glEnable(GL_TEXTURE_RECTANGLE_ARB);
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_textureId);
        glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8, m_textureWidth, m_textureHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, m_textureData);
    }


    void update(const uint32_t* bitmap, unsigned x, unsigned y, unsigned w, unsigned h)
    {
        if (m_textureData && x < m_textureWidth && y < m_textureHeight)
        {
            if (x + w > m_textureWidth) w = m_textureWidth - x;
            if (y + h > m_textureHeight) h = m_textureHeight - y;

            unsigned index = (x + (y * m_textureWidth));   // Offset to first pixel
            for (unsigned i = 0; i < h; ++i)
            {
                memcpy(&m_textureData[index], &bitmap[index], w * sizeof (uint32_t));
                index += m_textureWidth;
            }

            glEnable(GL_TEXTURE_RECTANGLE_ARB);
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_textureId);
            glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_PRIORITY, 0.0f);
            glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
            glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, m_textureWidth, m_textureHeight, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, m_textureData);
        }
    }


    // Draw the texture over the entire context
    void render()
    {
        if (m_textureId && m_textureData)
        {
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_textureId);

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);

            glColor3f(1, 1, 1);

            glBegin(GL_QUADS);
                glTexCoord2f(0,               0);                   glVertex2f(-1,  1);
                glTexCoord2f(m_textureWidth,  0);                   glVertex2f( 1,  1);
                glTexCoord2f(m_textureWidth,  m_textureHeight);     glVertex2f( 1, -1);
                glTexCoord2f(0,               m_textureHeight);     glVertex2f(-1, -1);
            glEnd();
        }
    }


    // Draw the texture within another.
    void render(int x, int y, const Texture& other)
    {
        if (m_textureId && m_textureData)
        {
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_textureId);
            glPushMatrix();
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            glScalef(1, -1, 1);

            glColor4f(1, 1, 1, 1);

            float x0 = (float(x) / float(other.m_textureWidth)) * 2 - 1;
            float x1 = (float(x+m_textureWidth) / float(other.m_textureWidth)) * 2 - 1;

            float y0 = (float(y) / float(other.m_textureHeight)) * 2 - 1;
            float y1 = (float(y+m_textureHeight) / float(other.m_textureHeight)) * 2 - 1;

            glBegin(GL_QUADS);
                glTexCoord2f(0,               0);                   glVertex2f(x0, y0);
                glTexCoord2f(m_textureWidth,  0);                   glVertex2f(x1, y0);
                glTexCoord2f(m_textureWidth,  m_textureHeight);     glVertex2f(x1, y1);
                glTexCoord2f(0,               m_textureHeight);     glVertex2f(x0, y1);
            glEnd();
            glPopMatrix();
        }
    }
};


@implementation DXDemoOpenGLView

- (id)initWithFrame:(NSRect)frame
{
    _desktopTexture = 0;
    _cursorTexture = 0;

    _cursorX = 0;
    _cursorY = 0;
    _cursorIsVisible = false;

    GLuint attribs[] =
    {
        NSOpenGLPFANoRecovery,
        NSOpenGLPFAWindow,
        NSOpenGLPFAAccelerated,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        0
    };

    NSOpenGLPixelFormat* fmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:(NSOpenGLPixelFormatAttribute*)attribs];

    if (!fmt) NSLog(@"No OpenGL pixel format");

    self = [super initWithFrame:frame pixelFormat:fmt];
    return self;
}


- (void)prepareOpenGL
{
    [super prepareOpenGL];

    CGLContextObj ctx = CGLGetCurrentContext();
    CGLEnable( ctx, kCGLCEMPEngine);

    GLint swapInt = 1;
    [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_PRIORITY, 0.0f);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE);
    glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_PRIORITY, 0.0f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);

    _cursorTexture = new Texture();
    _desktopTexture = new Texture();
}


- (void)drawRect:(NSRect)rect
{
    GLsizei w = (GLsizei)rect.size.width;
    GLsizei h = (GLsizei)rect.size.height;

    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT+GL_DEPTH_BUFFER_BIT+GL_STENCIL_BUFFER_BIT);

    glDisable(GL_BLEND);
    _desktopTexture->render();
    if (_cursorIsVisible)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        _cursorTexture->render(_cursorX, _cursorY, *_desktopTexture);
    }
    [[self openGLContext] flushBuffer];
}


- (void)setBlank
{
    _desktopTexture->initialise();
}


- (void)setDesktop:(const uint32_t*)bitmap width:(unsigned)width height:(unsigned)height
{
    _desktopTexture->initialise(bitmap, width, height);
    [self setNeedsDisplay:YES];
}


- (void)updateDesktop:(const uint32_t*)bitmap regionX:(unsigned)x regionY:(unsigned)y regionWidth:(unsigned)width regionHeight:(unsigned)height
{
    _desktopTexture->update(bitmap, x, y, width, height);
    [self setNeedsDisplay:YES];
}


- (void)setCursor:(uint32_t*)bitmap width:(unsigned)width height:(unsigned)height x:(unsigned)x y:(unsigned)y isVisible:(bool)isVisible
{
    _cursorX = x;
    _cursorY = y;
    _cursorIsVisible = isVisible;
    if (_cursorIsVisible) _cursorTexture->initialise(bitmap, width, height);
    [self setNeedsDisplay:YES];
}


@end
