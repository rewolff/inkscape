#ifndef INKSCAPE_LPE_LATTICE2_H
#define INKSCAPE_LPE_LATTICE2_H

/** \file
 * LPE <lattice2> implementation, see lpe-lattice2.cpp.
 */

/*
 * Authors:
 *   Johan Engelen
 *   Steren Giannini
 *   Noé Falzon
 *   Victor Navez
 *   ~suv
 *   Jabiertxo Arraiza
*
* Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "live_effects/effect.h"
#include "live_effects/parameter/enum.h"
#include "live_effects/parameter/point.h"
#include "live_effects/lpegroupbbox.h"

namespace Gtk {
class Expander;
}

namespace Inkscape {
namespace LivePathEffect {

class LPELattice2 : public Effect, GroupBBoxEffect {
public:

    LPELattice2(LivePathEffectObject *lpeobject);
    ~LPELattice2() override;

    Geom::Piecewise<Geom::D2<Geom::SBasis> > doEffect_pwd2 (Geom::Piecewise<Geom::D2<Geom::SBasis> > const & pwd2_in) override;

    void resetDefaults(SPItem const* item) override;

    void doBeforeEffect(SPLPEItem const* lpeitem) override;

    Gtk::Widget * newWidget() override;

    void calculateCurve(Geom::Point a,Geom::Point b, SPCurve *c, bool horizontal, bool move);

    void vertical(PointParam &paramA,PointParam &paramB, Geom::Line vert);

    void horizontal(PointParam &paramA,PointParam &paramB,Geom::Line horiz);

    void setDefaults();

    void onExpanderChanged();

    void resetGrid();

protected:
    void addCanvasIndicators(SPLPEItem const */*lpeitem*/, std::vector<Geom::PathVector> &hp_vec) override;
private:

    BoolParam horizontal_mirror;
    BoolParam vertical_mirror;
    BoolParam perimetral;
    BoolParam live_update;
    PointParam grid_point_0;
    PointParam grid_point_1;
    PointParam grid_point_2;
    PointParam grid_point_3;
    PointParam grid_point_4;
    PointParam grid_point_5;
    PointParam grid_point_6;
    PointParam grid_point_7;
    PointParam grid_point_8x9;
    PointParam grid_point_10x11;
    PointParam grid_point_12;
    PointParam grid_point_13;
    PointParam grid_point_14;
    PointParam grid_point_15;
    PointParam grid_point_16;
    PointParam grid_point_17;
    PointParam grid_point_18;
    PointParam grid_point_19;
    PointParam grid_point_20x21;
    PointParam grid_point_22x23;
    PointParam grid_point_24x26;
    PointParam grid_point_25x27;
    PointParam grid_point_28x30;
    PointParam grid_point_29x31;
    PointParam grid_point_32x33x34x35;

    bool expanded;
    Gtk::Expander * expander;

    LPELattice2(const LPELattice2&) = delete;
    LPELattice2& operator=(const LPELattice2&) = delete;
};

} //namespace LivePathEffect
} //namespace Inkscape

#endif
