/*
 FLIF - Free Lossless Image Format
 Copyright (C) 2010-2015  Jon Sneyers & Pieter Wuille, LGPL v3+

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include "flif-interface-private_common.hpp"

FLIF_IMAGE::FLIF_IMAGE() { }

#pragma pack(push,1)
struct FLIF_RGBA {
    uint8_t r,g,b,a;
};
#pragma pack(pop)

void FLIF_IMAGE::write_row_RGBA8(uint32_t row, const void* buffer, size_t buffer_size_bytes) {
    if(buffer_size_bytes < image.cols() * sizeof(FLIF_RGBA))
        return;

    const FLIF_RGBA* buffer_rgba = reinterpret_cast<const FLIF_RGBA*>(buffer);

    if(image.numPlanes() >= 3) {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            image.set(0, row, c, buffer_rgba[c].r);
            image.set(1, row, c, buffer_rgba[c].g);
            image.set(2, row, c, buffer_rgba[c].b);
        }
    }
    if(image.numPlanes() >= 4) {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            image.set(3, row, c, buffer_rgba[c].a);
        }
    }
}

void FLIF_IMAGE::read_row_RGBA8(uint32_t row, void* buffer, size_t buffer_size_bytes) {
    if(buffer_size_bytes < image.cols() * sizeof(FLIF_RGBA))
        return;

    FLIF_RGBA* buffer_rgba = reinterpret_cast<FLIF_RGBA*>(buffer);
    int rshift = 0;
    int lshift = 1;
    ColorVal m=image.max(0);
    while (m > 255) { rshift++; m = m >> 1; } // in case the image has bit depth higher than 8
    while (m * ((1 << lshift)-1) < 255) { lshift++; } // in case the image has bit depth lower than 8

    if(image.numPlanes() >= 3) {
        // color
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].r = ((image(0, row, c) >> rshift) * ((1<<lshift)-1)) & 0xFF;
            buffer_rgba[c].g = ((image(1, row, c) >> rshift) * ((1<<lshift)-1)) & 0xFF;
            buffer_rgba[c].b = ((image(2, row, c) >> rshift) * ((1<<lshift)-1)) & 0xFF;
        }
    } else {
        // grayscale
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].r =
            buffer_rgba[c].g =
            buffer_rgba[c].b = ((image(0, row, c) >> rshift) * ((1<<lshift)-1)) & 0xFF;
        }
    }
    if(image.numPlanes() >= 4) {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].a = ((image(3, row, c) >> rshift) * ((1<<lshift)-1)) & 0xFF;
        }
    } else {
        for (size_t c = 0; c < (size_t) image.cols(); c++) {
            buffer_rgba[c].a = 0xFF;  // fully opaque
        }
    }
}

//=============================================================================

/*!
Notes about the C interface:

Only use types known to C.
Use types that are unambiguous across all compilers, like uint32_t.
Each function must have it's call convention set.
Exceptions must be caught no matter what.

*/

//=============================================================================


extern "C" {

FLIF_DLLEXPORT FLIF_IMAGE* FLIF_API flif_create_image(uint32_t width, uint32_t height) {
    try
    {
        std::unique_ptr<FLIF_IMAGE> image(new FLIF_IMAGE());
        image->image.init(width, height, 0, 255, 4);
        return image.release();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT void FLIF_API flif_destroy_image(FLIF_IMAGE* image) {
    // delete should never let exceptions out
    delete image;
}

FLIF_DLLEXPORT uint32_t FLIF_API flif_image_get_width(FLIF_IMAGE* image) {
    try
    {
        return image->image.cols();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT uint32_t FLIF_API flif_image_get_height(FLIF_IMAGE* image) {
    try
    {
        return image->image.rows();
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT uint8_t FLIF_API flif_image_get_nb_channels(FLIF_IMAGE* image) {
    try
    {
        int nb = image->image.numPlanes();
        if (nb > 4) nb = 4; // there could be an extra plane for FRA
        return nb;
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT uint32_t FLIF_API flif_image_get_frame_delay(FLIF_IMAGE* image) {
    try
    {
        return image->image.frame_delay;
    }
    catch(...) {}
    return 0;
}

FLIF_DLLEXPORT void FLIF_API flif_image_write_row_RGBA8(FLIF_IMAGE* image, uint32_t row, const void* buffer, size_t buffer_size_bytes) {
    try
    {
        image->write_row_RGBA8(row, buffer, buffer_size_bytes);
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_image_read_row_RGBA8(FLIF_IMAGE* image, uint32_t row, void* buffer, size_t buffer_size_bytes) {
    try
    {
        image->read_row_RGBA8(row, buffer, buffer_size_bytes);
    }
    catch(...) {}
}

FLIF_DLLEXPORT void FLIF_API flif_free_memory(void* buffer) {
    delete [] reinterpret_cast<uint8_t*>(buffer);
}

} // extern "C"
