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

#include "../image/image.hpp"
#include "../image/color_range.hpp"
#include "transform.hpp"
#include <tuple>
#ifdef HAS_ENCODER
#include <set>
#endif

#define MAX_PALETTE_SIZE 30000

class ColorRangesPalette final : public ColorRanges {
protected:
    const ColorRanges *ranges;
    int nb_colors;
public:
    ColorRangesPalette(const ColorRanges *rangesIn, const int nb) : ranges(rangesIn), nb_colors(nb) { }
    bool isStatic() const { return false; }
    int numPlanes() const { return ranges->numPlanes(); }

    ColorVal min(int p) const { if (p<3) return 0; else return ranges->min(p); }
    ColorVal max(int p) const { switch(p) {
                                        case 0: return 0;
                                        case 1: return nb_colors-1;
                                        case 2: return 0;
                                        default: return ranges->max(p);
                                         };
                              }
    void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const {
         if (p==1) { minv=0; maxv=nb_colors-1; return;}
         else if (p<3) { minv=0; maxv=0; return;}
         else ranges->minmax(p,pp,minv,maxv);
    }

};


template <typename IO>
class TransformPalette : public Transform<IO> {
protected:
    typedef std::tuple<ColorVal,ColorVal,ColorVal> Color;
    std::vector<Color> Palette_vector;
    unsigned int max_palette_size;
    bool ordered_palette;

public:
    void configure(const int setting) {
        if (setting>0) { ordered_palette=true; max_palette_size = setting;}
        else {ordered_palette=false; max_palette_size = -setting;}
    }
    bool init(const ColorRanges *srcRanges) {
        if (srcRanges->numPlanes() < 3) return false;
        if (srcRanges->max(0) == 0 && srcRanges->max(2) == 0 &&
            srcRanges->numPlanes() > 3 && srcRanges->min(3) == 1 && srcRanges->max(3) == 1) return false; // already did PLA!
//        if (srcRanges->min(0) < 0 || srcRanges->min(1) < 0 || srcRanges->min(2) < 0) return false;
        if (srcRanges->min(1) == srcRanges->max(1)
         && srcRanges->min(2) == srcRanges->max(2)) return false;  // probably grayscale/monochrome, better not use palette then
        return true;
    }

    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) {
        for (Image& image : images) image.palette=true;
        return new ColorRangesPalette(srcRanges, Palette_vector.size());
    }
    void invData(Images& images) const {
        for (Image& image : images) {
          image.undo_make_constant_plane(0);
          image.undo_make_constant_plane(1); // only needed when there is only one palette color, so plane 1 is also constant
          image.undo_make_constant_plane(2);
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                int P=image(1,r,c);
                image.set(0,r,c, std::get<0>(Palette_vector[P]));
                image.set(1,r,c, std::get<1>(Palette_vector[P]));
                image.set(2,r,c, std::get<2>(Palette_vector[P]));
            }
          }
          image.palette=false;
        }
    }

#ifdef HAS_ENCODER
    bool process(const ColorRanges *, const Images &images) {
        if (ordered_palette) {
          std::set<Color> Palette;
          for (const Image& image : images)
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                int Y=image(0,r,c), I=image(1,r,c), Q=image(2,r,c);
                if (image.alpha_zero_special && image.numPlanes()>3 && image(3,r,c)==0) continue;
                Palette.insert(Color(Y,I,Q));
                if (Palette.size() > max_palette_size) return false;
            }
          }
          for (Color c : Palette) Palette_vector.push_back(c);
        } else {
          for (const Image& image : images)
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                int Y=image(0,r,c), I=image(1,r,c), Q=image(2,r,c);
                if (image.alpha_zero_special && image.numPlanes()>3 && image(3,r,c)==0) continue;
                Color C(Y,I,Q);
                bool found=false;
                for (Color c : Palette_vector) if (c==C) {found=true; break;}
                if (!found) {
                    Palette_vector.push_back(C);
                    if (Palette_vector.size() > max_palette_size) return false;
                }
            }
          }
        }
//        printf("Palette size: %lu\n",Palette.size());
        return true;
    }
    void data(Images& images) const {
//        printf("TransformPalette::data\n");
        for (Image& image : images) {
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                Color C(image(0,r,c), image(1,r,c), image(2,r,c));
                ColorVal P=0;
                for (Color c : Palette_vector) {if (c==C) break; else P++;}
//                image.set(0,r,c, 0);
                image.set(1,r,c, P);
//                image.set(2,r,c, 0);
            }
          }
          image.make_constant_plane(0,0);
          image.make_constant_plane(2,0);
        }
    }
    void save(const ColorRanges *srcRanges, RacOut<IO> &rac) const {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coder(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coderY(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coderI(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coderQ(rac);
        coder.write_int(1, MAX_PALETTE_SIZE, Palette_vector.size());
//        printf("Saving %lu colors: ", Palette_vector.size());
        prevPlanes pp(2);
        int sorted=(ordered_palette? 1 : 0);
        coder.write_int(0, 1, sorted);
        if (sorted) {
            Color min(srcRanges->min(0), srcRanges->min(1), srcRanges->min(2));
            Color max(srcRanges->max(0), srcRanges->max(1), srcRanges->max(2));
            Color prev(-1,-1,-1);
            for (Color c : Palette_vector) {
                ColorVal Y=std::get<0>(c);
                coderY.write_int(std::get<0>(min), std::get<0>(max), Y);
                pp[0]=Y; srcRanges->minmax(1,pp,std::get<1>(min), std::get<1>(max));
                ColorVal I=std::get<1>(c);
                coderI.write_int((std::get<0>(prev) == Y ? std::get<1>(prev): std::get<1>(min)), std::get<1>(max), I);
                pp[1]=I; srcRanges->minmax(2,pp,std::get<2>(min), std::get<2>(max));
                coderQ.write_int(std::get<2>(min), std::get<2>(max), std::get<2>(c));
                std::get<0>(min) = std::get<0>(c);
                prev = c;
            }
        } else {
            ColorVal min, max;
            for (Color c : Palette_vector) {
                ColorVal Y=std::get<0>(c);
                srcRanges->minmax(0,pp,min,max);
                coderY.write_int(min,max,Y);
                pp[0]=Y; srcRanges->minmax(1,pp,min,max);
                ColorVal I=std::get<1>(c);
                coderI.write_int(min, max, I);
                pp[1]=I; srcRanges->minmax(2,pp,min,max);
                coderQ.write_int(min, max, std::get<2>(c));
//                printf("YIQ(%i,%i,%i)\t", std::get<0>(c), std::get<1>(c), std::get<2>(c));
            }
        }
//        printf("\nSaved palette of size: %lu\n",Palette_vector.size());
        v_printf(5,"[%lu]",Palette_vector.size());
        if (!ordered_palette) v_printf(5,"Unsorted");
    }
#endif
    bool load(const ColorRanges *srcRanges, RacIn<IO> &rac) {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coder(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coderY(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coderI(rac);
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coderQ(rac);
        long unsigned size = coder.read_int(1, MAX_PALETTE_SIZE);
//        printf("Loading %lu colors: ", size);
        prevPlanes pp(2);
        int sorted = coder.read_int(0,1);
        if (sorted) {
            Color min(srcRanges->min(0), srcRanges->min(1), srcRanges->min(2));
            Color max(srcRanges->max(0), srcRanges->max(1), srcRanges->max(2));
            Color prev(-1,-1,-1);
            for (unsigned int p=0; p<size; p++) {
                ColorVal Y=coderY.read_int(std::get<0>(min), std::get<0>(max));
                pp[0]=Y; srcRanges->minmax(1,pp,std::get<1>(min), std::get<1>(max));
                ColorVal I=coderI.read_int((std::get<0>(prev) == Y ? std::get<1>(prev): std::get<1>(min)), std::get<1>(max));
                pp[1]=I; srcRanges->minmax(2,pp,std::get<2>(min), std::get<2>(max));
                ColorVal Q=coderQ.read_int(std::get<2>(min), std::get<2>(max));
                Color c(Y,I,Q);
                Palette_vector.push_back(c);
                std::get<0>(min) = std::get<0>(c);
                prev = c;
            }
        } else {
            ColorVal min, max;
            for (unsigned int p=0; p<size; p++) {
                srcRanges->minmax(0,pp,min,max);
                ColorVal Y=coderY.read_int(min,max);
                pp[0]=Y; srcRanges->minmax(1,pp,min,max);
                ColorVal I=coderI.read_int(min,max);
                pp[1]=I; srcRanges->minmax(2,pp,min,max);
                ColorVal Q=coderQ.read_int(min,max);
                Color c(Y,I,Q);
                Palette_vector.push_back(c);
//                printf("YIQ(%i,%i,%i)\t", std::get<0>(c), std::get<1>(c), std::get<2>(c));
            }
        }
//        printf("\nLoaded palette of size: %lu\n",Palette_vector.size());
        v_printf(5,"[%lu]",Palette_vector.size());
        return true;
    }
};
