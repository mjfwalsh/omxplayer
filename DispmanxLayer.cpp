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

#include <assert.h>
#include <bcm_host.h>

#include "DispmanxLayer.h"
#include "utils/simple_geometry.h"

#define ELEMENT_CHANGE_LAYER          (1<<0)
#define ELEMENT_CHANGE_OPACITY        (1<<1)
#define ELEMENT_CHANGE_DEST_RECT      (1<<2)
#define ELEMENT_CHANGE_SRC_RECT       (1<<3)
#define ELEMENT_CHANGE_MASK_RESOURCE  (1<<4)
#define ELEMENT_CHANGE_TRANSFORM      (1<<5)

DISPMANX_DISPLAY_HANDLE_T DispmanxLayer::s_display;
int DispmanxLayer::s_layer;

void DispmanxLayer::openDisplay(int display_num, int layer)
{
	bcm_host_init();

	// Open display
	s_display = vc_dispmanx_display_open(display_num);
	assert(s_display != 0);

	// set layer
	s_layer = layer;
}

Dimension DispmanxLayer::getScreenDimensions()
{
	// Get screen info
	DISPMANX_MODEINFO_T screen_info;
	int result = vc_dispmanx_display_get_info(s_display, &screen_info);
	assert(result == 0);

	return {screen_info.width, screen_info.height};
}

void DispmanxLayer::closeDisplay()
{
	int result = vc_dispmanx_display_close(s_display);
	assert(result == 0);
}

DispmanxLayer::DispmanxLayer(int bytesperpixel, Rectangle dest_rect, Dimension src_image,
		uint32_t *palette)
{
	// image type
	VC_IMAGE_TYPE_T imagetype;

	switch(bytesperpixel) {
		case 4:		imagetype = VC_IMAGE_ARGB8888;	break;
		case 2:		imagetype = VC_IMAGE_RGBA16;	break;
		case 1:		imagetype = VC_IMAGE_8BPP;		break;
		default:	assert(0);
	}

	if(src_image.width == -1) src_image.width = dest_rect.width;
	if(src_image.height == -1) src_image.height = dest_rect.height;

	// Destination image dimensions should be divisible by 16
	assert(dest_rect.width % 16 == 0);
	assert(dest_rect.height % 16 == 0);

	// image rectangles
	VC_RECT_T srcRect;
	VC_RECT_T dstRect;
	vc_dispmanx_rect_set(&(m_bmpRect), 0, 0, src_image.width, src_image.height);
	vc_dispmanx_rect_set(&(srcRect), 0 << 16, 0 << 16, src_image.width << 16, src_image.height << 16);
	vc_dispmanx_rect_set(&(dstRect), dest_rect.x, dest_rect.y, dest_rect.width, dest_rect.height);

	// Image vars
	m_image_pitch = src_image.width * bytesperpixel;

	// create image resource
	uint vc_image_ptr;
	m_resource = vc_dispmanx_resource_create(
		imagetype,
		src_image.width | (m_image_pitch << 16),
		src_image.height | (src_image.height << 16),
		&vc_image_ptr);
	assert(m_resource != 0);

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

	// Position currently empty image on screen
	m_update = vc_dispmanx_update_start(0);
	assert(m_update != 0);

	VC_DISPMANX_ALPHA_T alpha = { DISPMANX_FLAGS_ALPHA_FROM_SOURCE, 255, 0 };

	m_element = vc_dispmanx_element_add(m_update, s_display, -30,
		&(dstRect), m_resource, &(srcRect),
		DISPMANX_PROTECTION_NONE, &alpha, NULL, DISPMANX_NO_ROTATE);

	assert(m_element != 0);

	int result = vc_dispmanx_update_submit_sync(m_update);
	assert(result == 0);
}

void DispmanxLayer::changeImageLayer(int new_layer)
{
	m_update = vc_dispmanx_update_start(0);
	assert(m_update != 0);

	// change layer to new_layer
	int ret = vc_dispmanx_element_change_attributes(m_update, m_element,
		ELEMENT_CHANGE_LAYER, new_layer, 255, NULL, NULL, 0, DISPMANX_NO_ROTATE);
	assert( ret == 0 );

	ret = vc_dispmanx_update_submit_sync( m_update );
	assert( ret == 0 );
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
	memset(blank, 0, size);

	setImageData(blank, false);

	free(blank);
}


// copy image data to screen and make the element visible
void DispmanxLayer::setImageData(void *image_data, bool show)
{
	// the palette param is ignored
	int result = vc_dispmanx_resource_write_data(m_resource,
		VC_IMAGE_MIN, m_image_pitch, image_data, &(m_bmpRect));

	assert(result == 0);

	result = vc_dispmanx_element_change_source(m_update, m_element, m_resource);

	assert(result == 0);

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
	int result = 0;

	m_update = vc_dispmanx_update_start(0);
	assert(m_update != 0);

	result = vc_dispmanx_element_remove(m_update, m_element);
	assert(result == 0);

	result = vc_dispmanx_update_submit_sync(m_update);
	assert(result == 0);

	result = vc_dispmanx_resource_delete(m_resource);
	assert(result == 0);
}
