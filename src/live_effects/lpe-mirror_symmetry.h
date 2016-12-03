#ifndef INKSCAPE_LPE_MIRROR_SYMMETRY_H
#define INKSCAPE_LPE_MIRROR_SYMMETRY_H

/** \file
 * LPE <mirror_symmetry> implementation: mirrors a path with respect to a given line.
 */
/*
 * Authors:
 *   Maximilian Albert
 *   Johan Engelen
 *   Jabiertxof
 *
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 * Copyright (C) Maximilin Albert 2008 <maximilian.albert@gmail.com>
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#include <gtkmm.h>
#include "live_effects/effect.h"
#include "live_effects/parameter/parameter.h"
#include "live_effects/parameter/point.h"
#include "live_effects/parameter/path.h"
#include "live_effects/parameter/enum.h"
#include "live_effects/lpegroupbbox.h"

namespace Inkscape {
namespace LivePathEffect {

namespace MS {
// we need a separate namespace to avoid clashes with LPEPerpBisector
class KnotHolderEntityCenterMirrorSymmetry;
}

enum LpeAction {
    LPE_ERASE = 0,
    LPE_TO_OBJECTS,
    LPE_VISIBILITY
};

enum ModeType {
    MT_V,
    MT_H,
    MT_FREE,
    MT_X,
    MT_Y,
    MT_END
};

class LPEMirrorSymmetry : public Effect, GroupBBoxEffect {
public:
    LPEMirrorSymmetry(LivePathEffectObject *lpeobject);
    virtual ~LPEMirrorSymmetry();
    virtual void doOnApply (SPLPEItem const* lpeitem);
    virtual void doBeforeEffect (SPLPEItem const* lpeitem);
    virtual void transform_multiply(Geom::Affine const& postmul, bool set);
    virtual Geom::PathVector doEffect_path (Geom::PathVector const & path_in);
    virtual void doOnRemove (SPLPEItem const* /*lpeitem*/);
    virtual void doOnVisibilityToggled(SPLPEItem const* /*lpeitem*/);
    void processObjects(LpeAction lpe_action);
    /* the knotholder entity classes must be declared friends */
    friend class MS::KnotHolderEntityCenterMirrorSymmetry;
    void createPhantom(SPLPEItem *origin, Glib::ustring id, bool styling);
    void createClone(SPLPEItem *origin, Geom::Affine transform, Glib::ustring id);
    void addKnotHolderEntities(KnotHolder *knotholder, SPDesktop *desktop, SPItem *item);

protected:
    virtual void addCanvasIndicators(SPLPEItem const *lpeitem, std::vector<Geom::PathVector> &hp_vec);

private:
    EnumParam<ModeType> mode;
    BoolParam discard_orig_path;
    BoolParam fuse_paths;
    BoolParam oposite_fuse;
    BoolParam split;
    BoolParam split_sensitive;
    BoolParam split_style;
    PointParam start_point;
    PointParam end_point;
    Geom::Line line_separation;
    Geom::Point previous_center;
    Geom::Point center_point;

    LPEMirrorSymmetry(const LPEMirrorSymmetry&);
    LPEMirrorSymmetry& operator=(const LPEMirrorSymmetry&);
};

} //namespace LivePathEffect
} //namespace Inkscape

#endif
