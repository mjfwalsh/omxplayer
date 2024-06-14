//
// The MIT License (MIT)
//
// Copyright (c) 2020 Michael Walsh
//
// Based on pngview by Andrew Duncan
// https://github.com/AndrewFromMelbourne/raspidmx/tree/master/pngview
// Copyright (c) 2013 Andrew Duncan
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#include <bcm_host.h>

#include "DispmanxLayer.h"
#include "utils/simple_geometry.h"

DISPMANX_DISPLAY_HANDLE_T DispmanxLayer::s_display;
int DispmanxLayer::s_layer;
Rect DispmanxLayer::s_screen_rect;
bool DispmanxLayer::s_is_fullscreen;

void DispmanxLayer::openDisplay(int display_num, int layer, Rect screen_rect)
{
  // Open display
  s_display = vc_dispmanx_display_open(display_num);
  if(s_display == 0)
    throw "Dispamnx Error: Failed to open display layer\n"
    "(Note: omxplayer will not run if the kms driver is enabled)";

  // set layer
  s_layer = layer;

  // set s_screen_rect rectangle
  if(screen_rect.width > 0 && screen_rect.height > 0)
  {
    s_screen_rect.set(screen_rect);
    s_is_fullscreen = false;
  }
  else
  {
    // Get s_screen_rect info
    DISPMANX_MODEINFO_T screen_info;
    int result = vc_dispmanx_display_get_info(s_display, &screen_info);
    if(result != 0)
      throw "Dispamnx Error: Failed to get s_screen_rect dimensions";

    s_screen_rect.set(0, 0, screen_info.width, screen_info.height);
    s_is_fullscreen = true;
  }

  atexit(DispmanxLayer::closeDisplay);
}

Rect DispmanxLayer::getScreenDimensions()
{
  return s_screen_rect;
}

// calculates the output rectangle of the video on the screen
Rect DispmanxLayer::GetVideoPort(float video_aspect_ratio, int aspect_mode)
{
  // Calculate position of view port
  Rect view_port = s_screen_rect;
  if(s_is_fullscreen)
  {
    float screen_aspect_ratio = (float)s_screen_rect.width / s_screen_rect.height;
    if(aspect_mode <= 1 && video_aspect_ratio != screen_aspect_ratio) {
      if(video_aspect_ratio > screen_aspect_ratio) {
        view_port.height = s_screen_rect.width * video_aspect_ratio;
      } else {
        view_port.width = s_screen_rect.height * video_aspect_ratio;
      }
    }
  }

  // adjust width and height so they are divisible by 16
  view_port.width = (view_port.width + 8) & ~15;
  view_port.height = (view_port.height + 8) & ~15;
  view_port.x += (s_screen_rect.width - view_port.width) / 2;
  view_port.y += (s_screen_rect.height - view_port.height) / 2;

  return view_port;
}

void DispmanxLayer::closeDisplay()
{
  int result = vc_dispmanx_display_close(s_display);
  if(result != 0)
    throw "Dispamnx Error: Failed to close display layer";
}

DispmanxLayer::DispmanxLayer(int bytesperpixel, Rect dest_rect, Dimension src_image,
    uint32_t *palette)
{
  // image type
  VC_IMAGE_TYPE_T imagetype;

  switch(bytesperpixel) {
    case 4:    imagetype = VC_IMAGE_ARGB8888;  break;
    case 2:    imagetype = VC_IMAGE_RGBA16;  break;
    case 1:    imagetype = VC_IMAGE_8BPP;    break;
    default:  throw "Dispamnx Error: Unsupported image type";
  }

  if(src_image.width == -1) src_image.width = dest_rect.width;
  if(src_image.height == -1) src_image.height = dest_rect.height;

  // Destination image dimensions should be divisible by 16
  if(dest_rect.width % 16 != 0 || dest_rect.height % 16 != 0)
    throw "Dispamnx Error: Layer must be divisible by 16";

  // image rectangles
  VC_RECT_T srcRect;
  VC_RECT_T dstRect;
  vc_dispmanx_rect_set(&m_bmpRect, 0, 0, src_image.width, src_image.height);
  vc_dispmanx_rect_set(&srcRect, 0, 0, src_image.width << 16, src_image.height << 16);
  vc_dispmanx_rect_set(&dstRect, dest_rect.x, dest_rect.y, dest_rect.width, dest_rect.height);

  // Image vars
  m_image_pitch = src_image.width * bytesperpixel;

  // create image resource
  uint vc_image_ptr;
  m_resource = vc_dispmanx_resource_create(
    imagetype,
    src_image.width | (m_image_pitch << 16),
    src_image.height | (src_image.height << 16),
    &vc_image_ptr);
  if(m_resource == 0)
    throw "Dispamnx Error: Failed to create resource";

  // set palette is necessary
  if(imagetype == VC_IMAGE_8BPP) {
    int new_palette[256];  // ARGB

    if(palette == NULL) {
      new_palette[0] = 0x00000000; // transparent background
      new_palette[1] = 0xFF000000; // black outline
      new_palette[2] = 0xFFFFFFFF; // white text
      new_palette[3] = 0xFF7F7F7F; // gray
    } else {
      int h = 0;
      int alpha = 0x0;
      for(int i = 0; i < 16; i++) {
        for(int j = 0; j < 16; j++) {
          new_palette[h++] = alpha | palette[j];
        }
        alpha += 0x11000000;
      }
    }

    vc_dispmanx_resource_set_palette( m_resource, new_palette, 0, sizeof new_palette );
  }

  // Position currently empty image on s_screen_rect
  m_update = vc_dispmanx_update_start(0);
  if(m_update == 0)
    throw "Dispamnx Error: vc_dispmanx_update_start failed";

  VC_DISPMANX_ALPHA_T alpha = { DISPMANX_FLAGS_ALPHA_FROM_SOURCE, 255, 0 };

  m_element = vc_dispmanx_element_add(m_update, s_display, s_layer - 1,
    &dstRect, m_resource, &srcRect,
    DISPMANX_PROTECTION_NONE, &alpha, NULL, DISPMANX_NO_ROTATE);

  if(m_element == 0)
    throw "Dispamnx Error: vc_dispmanx_element_add failed";

  int result = vc_dispmanx_update_submit_sync(m_update);
  if(result != 0)
    throw "Dispamnx Error: vc_dispmanx_update_submit_sync failed";
}

void DispmanxLayer::changeImageLayer(int new_layer)
{
  m_update = vc_dispmanx_update_start(0);
  if(m_update == 0)
    throw "Dispamnx Error: vc_dispmanx_update_start failed";

  // change layer to new_layer
  int ret = vc_dispmanx_element_change_layer(m_update, m_element, new_layer);
  if( ret != 0 )
    throw "Dispamnx Error: vc_dispmanx_element_change_attributes failed";

  ret = vc_dispmanx_update_submit_sync(m_update);
  if( ret != 0 )
    throw "Dispamnx Error: vc_dispmanx_update_submit_sync failed";
}

void DispmanxLayer::hideElement()
{
  if(m_element_is_hidden) return;
  changeImageLayer(s_layer - 1);
  m_element_is_hidden = true;
}

void DispmanxLayer::showElement()
{
  if(!m_element_is_hidden) return;
  changeImageLayer(s_layer + 1);
  m_element_is_hidden = false;
}

void DispmanxLayer::clearImage()
{
  int size = m_image_pitch * m_bmpRect.height;

  void *blank = (void *)malloc(size);
  if(!blank) return;

  memset(blank, 0, size);
  setImageData(blank, false);
  free(blank);
}


// copy image data to s_screen_rect and make the element visible
void DispmanxLayer::setImageData(void *image_data, bool show)
{
  // the palette param is ignored
  int result = vc_dispmanx_resource_write_data(m_resource,
    VC_IMAGE_MIN, m_image_pitch, image_data, &m_bmpRect);

  if(result != 0)
    throw "Dispamnx Error: vc_dispmanx_resource_write_data failed";

  result = vc_dispmanx_element_change_source(m_update, m_element, m_resource);

  if(result != 0)
    throw "Dispamnx Error: vc_dispmanx_element_change_source failed";

  if(show) showElement();
}

const int& DispmanxLayer::getSourceWidth()
{
  return m_bmpRect.width;
}

const int& DispmanxLayer::getSourceHeight()
{
  return m_bmpRect.height;
}

DispmanxLayer::~DispmanxLayer()
{
  m_update = vc_dispmanx_update_start(0);
  vc_dispmanx_element_remove(m_update, m_element);
  vc_dispmanx_update_submit_sync(m_update);
  vc_dispmanx_resource_delete(m_resource);
}
