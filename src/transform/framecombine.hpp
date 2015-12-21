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

#include <vector>

#include "transform.hpp"


class ColorRangesFC final : public ColorRanges {
protected:
    ColorVal numPrevFrames;
    ColorVal alpha_min;
    ColorVal alpha_max;
    const ColorRanges *ranges;
public:
    ColorRangesFC(const ColorVal pf, const ColorVal amin, const ColorVal amax, const ColorRanges *rangesIn) : numPrevFrames(pf), alpha_min(amin), alpha_max(amax), ranges(rangesIn) {}
    bool isStatic() const { return false; }
    int numPlanes() const { return 5; }
    ColorVal min(int p) const { if (p<3) return ranges->min(p); else if (p==3) return alpha_min; else return 0; }
    ColorVal max(int p) const { if (p<3) return ranges->max(p); else if (p==3) return alpha_max; else return numPrevFrames; }
    void minmax(const int p, const prevPlanes &pp, ColorVal &mi, ColorVal &ma) const {
        if (p >= 3) { mi=min(p); ma=max(p); }
        else ranges->minmax(p, pp, mi, ma);
    }
};

template <typename IO>
class TransformFrameCombine : public Transform<IO> {
protected:
    bool was_flat;
    bool was_grayscale;
    int max_lookback;
    int user_max_lookback;
    int nb_frames;

    bool undo_redo_during_decode() { return true; }

    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) {
//        if (max_lookback >= (int)images.size()) { e_printf("Bad value for FRA lookback\n"); exit(4);}
        assert(max_lookback < (int)images.size());
        was_grayscale = srcRanges->numPlanes() < 2;
        was_flat = srcRanges->numPlanes() < 4;
        for (unsigned int fr=0; fr<images.size(); fr++) {
            Image& image = images[fr];
            image.ensure_frame_lookbacks();
        }
        int lookback = (int)images.size()-1;
        if (lookback > max_lookback) lookback=max_lookback;
        return new ColorRangesFC(lookback, (srcRanges->numPlanes() == 4 ? srcRanges->min(3) : 255), (srcRanges->numPlanes() == 4 ? srcRanges->max(3) : 255), srcRanges);
    }

    bool load(const ColorRanges *, RacIn<IO> &rac) {
        SimpleSymbolCoder<SimpleBitChance, RacIn<IO>, 18> coder(rac);
        max_lookback = coder.read_int(1, nb_frames-1);
        v_printf(5,"[%i]",max_lookback);
        return true;
    }

#ifdef HAS_ENCODER
    void save(const ColorRanges *, RacOut<IO> &rac) const {
        SimpleSymbolCoder<SimpleBitChance, RacOut<IO>, 18> coder(rac);
        coder.write_int(1,nb_frames-1,max_lookback);
    }

// a heuristic to figure out if this is going to help (it won't help if we introduce more entropy than what is eliminated)
    bool process(const ColorRanges *srcRanges, const Images &images) {
        if (images.size() < 2) return false;
        int nump=images[0].numPlanes();
        nb_frames = images.size();
        int64_t pixel_cost = 1;
        for (int p=0; p<nump; p++) pixel_cost *= (1 + srcRanges->max(p) - srcRanges->min(p));
        // pixel_cost is roughly the cost per pixel (number of different values a pixel can take)
        if (pixel_cost < 16) {v_printf(7,", no_FRA[pixels_too_cheap:%i]", pixel_cost); return false;} // pixels are too cheap, no point in trying to save stuff
        std::vector<uint64_t> found_pixels(images.size(), 0);
        uint64_t new_pixels=0;
        max_lookback=1;
        if (user_max_lookback == -1) user_max_lookback = images.size()-1;
        for (int fr=1; fr < (int)images.size(); fr++) {
            const Image& image = images[fr];
            for (uint32_t r=0; r<image.rows(); r++) {
                for (uint32_t c=image.col_begin[r]; c<image.col_end[r]; c++) {
                    new_pixels++;
                    for (int prev=1; prev <= fr; prev++) {
                        if (prev>user_max_lookback) break;
                        bool identical=true;
                        if (image.alpha_zero_special && nump>3 && image(3,r,c) == 0 && images[fr-prev](3,r,c) == 0) identical=true;
                        else
                        for (int p=0; p<nump; p++) {
                          if(image(p,r,c) != images[fr-prev](p,r,c)) { identical=false; break;}
                        }
                        if (identical) { found_pixels[prev]++; new_pixels--; if (prev>max_lookback) max_lookback=prev; break;}
                    }
                }
            }
        }
        if (images.size() > 2) v_printf(7,", trying_FRA(at -1: %llu, at -2: %llu, new: %llu)",(long long unsigned) found_pixels[1],(long long unsigned) found_pixels[2], (long long unsigned) new_pixels);
        if (max_lookback>256) max_lookback=256;
        for(int i=1; i <= max_lookback; i++) {
            v_printf(8,"at lookback %i: %llu pixels\n",-i, found_pixels[i]);
            if (found_pixels[i] <= new_pixels/200 || i>pixel_cost) {max_lookback=i-1; break;}
            found_pixels[0] += found_pixels[i];
        }
        for(int i=max_lookback+1; i<(int)images.size(); i++) {
            if (found_pixels[i] > new_pixels/200 && i<pixel_cost) {max_lookback=i; found_pixels[0] += found_pixels[i];}
            else new_pixels += found_pixels[i];
        }

        return (found_pixels[0] * pixel_cost > new_pixels * (2 + max_lookback));
    };
    void data(Images &images) const {
        for (int fr=1; fr < (int)images.size(); fr++) {
            uint32_t ipixels=0;
            Image& image = images[fr];
            for (uint32_t r=0; r<image.rows(); r++) {
                for (uint32_t c=image.col_begin[r]; c<image.col_end[r]; c++) {
                    for (int prev=1; prev <= fr; prev++) {
                        if (prev>max_lookback) break;
                        bool identical=true;
                        if (image.alpha_zero_special && image(3,r,c) == 0 && images[fr-prev](3,r,c) == 0) identical=true;
                        else
                        for (int p=0; p<4; p++) {
                          if(image(p,r,c) != images[fr-prev](p,r,c)) { identical=false; break;}
                        }
                        if (identical) {image.set(4,r,c, prev); ipixels++; break;}
                    }
                }
            }
//            printf("frame %i: found %u pixels from previous frames\n", fr, ipixels);
        }
    }
#endif

    void configure(int setting) { user_max_lookback=nb_frames=setting; }
    void invData(Images &images) const {
        // most work has to be done on the fly in the decoder, this is just some cleaning up
        for (Image& image : images) image.drop_frame_lookbacks();
        if (was_flat) for (Image& image : images) image.drop_alpha();
        if (was_grayscale) for (Image& image : images) image.drop_color();
    }
};
