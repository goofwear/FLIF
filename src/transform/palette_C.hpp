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
#include <set>


class ColorRangesPaletteC final : public ColorRanges {
protected:
    const ColorRanges *ranges;
    int nb_colors[4];
public:
    ColorRangesPaletteC(const ColorRanges *rangesIn, const int nb[4]) : ranges(rangesIn) { for (int i=0; i<4; i++) nb_colors[i] = nb[i]; }
    bool isStatic() const { return true; }
    int numPlanes() const { return ranges->numPlanes(); }

    ColorVal min(int p) const { return 0; }
    ColorVal max(int p) const { return nb_colors[p]; }
    void minmax(const int p, const prevPlanes &pp, ColorVal &minv, ColorVal &maxv) const {
         minv=0; maxv=nb_colors[p];
    }

};


template <typename IO>
class TransformPaletteC : public Transform<IO> {
protected:
    std::vector<ColorVal> CPalette_vector[4];
    std::vector<ColorVal> CPalette_inv_vector[4];

public:
    bool init(const ColorRanges *) {
        return true;
    }

    const ColorRanges *meta(Images& images, const ColorRanges *srcRanges) {
        int nb[4] = {};
        v_printf(4,"[");
        for (int i=0; i<srcRanges->numPlanes(); i++) {
            nb[i] = CPalette_vector[i].size()-1;
            if (i>0) v_printf(4,",");
            v_printf(4,"%i",nb[i]);
//            if (nb[i] < 64) nb[i] <<= 2;
        }
        v_printf(4,"]");
        return new ColorRangesPaletteC(srcRanges, nb);
    }

    void invData(Images& images) const {
        for (Image& image : images) {
         for (int p=0; p<image.numPlanes(); p++) {
//          const int stretch = (CPalette_vector[p].size()>64 ? 0 : 2);
          image.undo_make_constant_plane(p);
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
//                image.set(p,r,c, CPalette_vector[p][image(p,r,c) >> stretch]);
                image.set(p,r,c, CPalette_vector[p][image(p,r,c)]);
            }
          }
         }
        }
    }

#if HAS_ENCODER
    bool process(const ColorRanges *srcRanges, const Images &images) {
        std::set<ColorVal> CPalette;
        bool nontrivial=false;
        for (int p=0; p<srcRanges->numPlanes(); p++) {
         if (p==3) CPalette.insert(0); // ensure that A=0 is still A=0 even if image does not contain zero-alpha pixels
         for (const Image& image : images) {
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                CPalette.insert(image(p,r,c));
            }
          }
         }
         if ((int)CPalette.size() <= srcRanges->max(p)-srcRanges->min(p)) nontrivial = true;
         if (CPalette.size() < 10) {
           // add up to 10 shades of gray
           ColorVal prev=0;
           for (ColorVal c : CPalette) {
             if (c > prev+1) CPalette_vector[p].push_back((c+prev)/2);
             CPalette_vector[p].push_back(c);
             prev=c;
             nontrivial=true;
           }
         } else {
           for (ColorVal c : CPalette) CPalette_vector[p].push_back(c);
         }
         CPalette.clear();
         CPalette_inv_vector[p].resize(srcRanges->max(p)+1);
         for (unsigned int i=0; i<CPalette_vector[p].size(); i++) CPalette_inv_vector[p][CPalette_vector[p][i]]=i;
        }
        return nontrivial;
    }
    void data(Images& images) const {
        for (Image& image : images) {
         for (int p=0; p<image.numPlanes(); p++) {
//          const int stretch = (CPalette_vector[p].size()>64 ? 0 : 2);
          for (uint32_t r=0; r<image.rows(); r++) {
            for (uint32_t c=0; c<image.cols(); c++) {
                ColorVal P = CPalette_inv_vector[p][image(p,r,c)];
//                for (ColorVal x : CPalette_vector[p]) {if (x==image(p,r,c)) break; else P++;}
//                image.set(p,r,c, P << stretch);
                image.set(p,r,c, P);
            }
          }
         }
        }
    }
    void save(const ColorRanges *srcRanges, RacOut<IO> &rac) const {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacOut<IO>, 18> coder(rac);
        for (int p=0; p<srcRanges->numPlanes(); p++) {
          coder.write_int(0, srcRanges->max(p)-srcRanges->min(p), CPalette_vector[p].size()-1);
          ColorVal min=srcRanges->min(p);
          int remaining=CPalette_vector[p].size()-1;
          for (unsigned int i=0; i<CPalette_vector[p].size(); i++) {
            coder.write_int(0, srcRanges->max(p)-min-remaining, CPalette_vector[p][i]-min);
            min = CPalette_vector[p][i]+1;
            remaining--;
          }
        }
    }
#endif
    bool load(const ColorRanges *srcRanges, RacIn<IO> &rac) {
        SimpleSymbolCoder<FLIFBitChanceMeta, RacIn<IO>, 18> coder(rac);
        for (int p=0; p<srcRanges->numPlanes(); p++) {
          unsigned int nb = coder.read_int(0, srcRanges->max(p)-srcRanges->min(p)) + 1;
          ColorVal min=srcRanges->min(p);
          int remaining = nb-1;
          for (unsigned int i=0; i<nb; i++) {
            CPalette_vector[p].push_back(min + coder.read_int(0, srcRanges->max(p)-min-remaining));
            min = CPalette_vector[p][i]+1;
            remaining--;
          }
        }
        return true;
    }
};
