#include <2geom/d2.h>
/* One would think that we would include d2-sbasis.h, however,
 * you cannot actually include it in anything - only d2 may import it.
 * This is due to the trickinesses of template submatching. */

namespace Geom {

SBasis L2(D2<SBasis> const & a, unsigned k) { return sqrt(dot(a, a), k); }

D2<SBasis> multiply(Linear const & a, D2<SBasis> const & b) {
    return D2<SBasis>(multiply(a, b[X]), multiply(a, b[Y]));
}

D2<SBasis> multiply(SBasis const & a, D2<SBasis> const & b) {
    return D2<SBasis>(multiply(a, b[X]), multiply(a, b[Y]));
}

D2<SBasis> truncate(D2<SBasis> const & a, unsigned terms) {
    return D2<SBasis>(truncate(a[X], terms), truncate(a[Y], terms));
}

unsigned sbasis_size(D2<SBasis> const & a) {
    return std::max((unsigned) a[0].size(), (unsigned) a[1].size());
}

//TODO: Is this sensical? shouldn't it be like pythagorean or something?
double tail_error(D2<SBasis> const & a, unsigned tail) {
    return std::max(a[0].tailError(tail), a[1].tailError(tail));
}

Piecewise<D2<SBasis> > sectionize(D2<Piecewise<SBasis> > const &a) {
    Piecewise<SBasis> x = partition(a[0], a[1].cuts), y = partition(a[1], a[0].cuts);
    assert(x.size() == y.size());
    Piecewise<D2<SBasis> > ret;
    for(unsigned i = 0; i < x.size(); i++)
        ret.push_seg(D2<SBasis>(x[i], y[i]));
    ret.cuts.insert(ret.cuts.end(), x.cuts.begin(), x.cuts.end());
    return ret;
}

D2<Piecewise<SBasis> > make_cuts_independent(Piecewise<D2<SBasis> > const &a) {
    D2<Piecewise<SBasis> > ret;
    for(unsigned d = 0; d < 2; d++) {
        for(unsigned i = 0; i < a.size(); i++)
            ret[d].push_seg(a[i][d]);
        ret[d].cuts.insert(ret[d].cuts.end(), a.cuts.begin(), a.cuts.end());
    }
    return ret;
}

Piecewise<D2<SBasis> > rot90(Piecewise<D2<SBasis> > const &M){
  Piecewise<D2<SBasis> > result;
  if (M.empty()) return M;
  result.push_cut(M.cuts[0]);
  for (unsigned i=0; i<M.size(); i++){
    result.push(rot90(M[i]),M.cuts[i+1]);
  }
  return result;
}

Piecewise<SBasis> dot(Piecewise<D2<SBasis> > const &a, 
		      Piecewise<D2<SBasis> > const &b){
  Piecewise<SBasis > result;
  if (a.empty() || b.empty()) return result;
  Piecewise<D2<SBasis> > aa = partition(a,b.cuts);
  Piecewise<D2<SBasis> > bb = partition(b,a.cuts);

  result.push_cut(aa.cuts.front());
  for (unsigned i=0; i<aa.size(); i++){
    result.push(dot(aa.segs[i],bb.segs[i]),aa.cuts[i+1]);
  }
  return result;
}

Piecewise<SBasis> cross(Piecewise<D2<SBasis> > const &a, 
			Piecewise<D2<SBasis> > const &b){
  Piecewise<SBasis > result;
  if (a.empty() || b.empty()) return result;
  Piecewise<D2<SBasis> > aa = partition(a,b.cuts);
  Piecewise<D2<SBasis> > bb = partition(b,a.cuts);

  result.push_cut(aa.cuts.front());
  for (unsigned i=0; i<a.size(); i++){
    result.push(cross(aa.segs[i],bb.segs[i]),aa.cuts[i+1]);
  }
  return result;
}

Piecewise<D2<SBasis> > operator*(Piecewise<D2<SBasis> > const &a, Matrix const &m) {
  Piecewise<D2<SBasis> > result;
  if(a.empty()) return result;
  result.push_cut(a.cuts[0]);
  for (unsigned i = 0; i < a.size(); i++) {
    result.push(a[i] * m, a.cuts[i+1]);
  }
  return result;
}

//if tol>0, only force continuity where the jump is smaller than tol.
Piecewise<D2<SBasis> > force_continuity(Piecewise<D2<SBasis> > const &f, double tol, bool closed)
{
    if (f.size()==0) return f;
    Piecewise<D2<SBasis> > result=f;
    unsigned cur   = (closed)? 0:1;
    unsigned prev  = (closed)? f.size()-1:0;
    while(cur<f.size()){
        Point pt0 = f.segs[prev].at1();
        Point pt1 = f.segs[cur ].at0();
        if (tol<=0 || L2sq(pt0-pt1)<tol*tol){
            pt0 = (pt0+pt1)/2;
            for (unsigned dim=0; dim<2; dim++){
                SBasis &prev_sb=result.segs[prev][dim];
                SBasis &cur_sb =result.segs[cur][dim];
                Coord const c=pt0[dim];
                if (prev_sb.empty()) {
                  prev_sb.push_back(Linear(0.0, c));
                } else {
                  prev_sb[0][1] = c;
                }
                if (cur_sb.empty()) {
                  cur_sb.push_back(Linear(c, 0.0));
                } else {
                  cur_sb[0][0] = c;
                }
            }
        }
        prev = cur++;
    }
    return result;
}

std::vector<Geom::Piecewise<Geom::D2<Geom::SBasis> > > 
split_at_discontinuities (Geom::Piecewise<Geom::D2<Geom::SBasis> > const & pwsbin, double tol)
{
    using namespace Geom;
    std::vector<Piecewise<D2<SBasis> > > ret;
    unsigned piece_start = 0;
    for (unsigned i=0; i<pwsbin.segs.size(); i++){
        if (i==(pwsbin.segs.size()-1) || L2(pwsbin.segs[i].at1()- pwsbin.segs[i+1].at0()) > tol){
            Piecewise<D2<SBasis> > piece;
            piece.cuts.push_back(pwsbin.cuts[piece_start]);
            for (unsigned j = piece_start; j<i+1; j++){
                piece.segs.push_back(pwsbin.segs[j]);
                piece.cuts.push_back(pwsbin.cuts[j+1]);
            }
            ret.push_back(piece);
            piece_start = i+1;
        }
    }
    return ret;
}

static void set_first_point(Piecewise<D2<SBasis> > &f, Point a){
    if ( f.empty() ){
        f.concat(Piecewise<D2<SBasis> >(D2<SBasis>(Linear(a[X]),Linear(a[Y]))));
        return;
    }
    for (unsigned dim=0; dim<2; dim++){
        if (f.segs.front()[dim].size() == 0){
            f.segs.front()[dim].push_back(Linear(a[dim],0));
        }else{
            f.segs.front()[dim][0][0] = a[dim];
        }
    }
}
static void set_last_point(Piecewise<D2<SBasis> > &f, Point a){
    if ( f.empty() ){
        f.concat(Piecewise<D2<SBasis> >(D2<SBasis>(Linear(a[X]),Linear(a[Y]))));
        return;
    }
    for (unsigned dim=0; dim<2; dim++){
        if (f.segs.back()[dim].size() == 0){
            f.segs.back()[dim].push_back(Linear(0,a[dim]));
        }else{
            f.segs.back()[dim][0][1] = a[dim];
        }
    }
}

std::vector<Piecewise<D2<SBasis> > > fuse_nearby_ends(std::vector<Piecewise<D2<SBasis> > > const &f, double tol){

    if ( f.size()==0 ) return f;
    std::vector<Piecewise<D2<SBasis> > > result;
    std::vector<std::vector<unsigned> > pre_result;
    for (unsigned i=0; i<f.size(); i++){
        bool inserted = false;
        Point a = f[i].firstValue();
        Point b = f[i].lastValue();
        for (unsigned j=0; j<pre_result.size(); j++){
            Point aj = f.at(pre_result[j].back()).lastValue();
            Point bj = f.at(pre_result[j].front()).firstValue();
            if ( L2(a-aj) < tol ) {
                pre_result[j].push_back(i);
                inserted = true;
                break;
            }
            if ( L2(b-bj) < tol ) {
                pre_result[j].insert(pre_result[j].begin(),i);
                inserted = true;
                break;
            }
        }
        if (!inserted) {
            pre_result.push_back(std::vector<unsigned>());
            pre_result.back().push_back(i);
        }
    }
    for (unsigned i=0; i<pre_result.size(); i++){
        Piecewise<D2<SBasis> > comp;
        for (unsigned j=0; j<pre_result[i].size(); j++){
            Piecewise<D2<SBasis> > new_comp = f.at(pre_result[i][j]);
            if ( j>0 ){
                set_first_point( new_comp, comp.segs.back().at1() );
            }
            comp.concat(new_comp);
        }
        if ( L2(comp.firstValue()-comp.lastValue()) < tol ){
            //TODO: check sizes!!!
            set_last_point( comp, comp.segs.front().at0() ); 
        }
        result.push_back(comp);
    }
    return result;
    return f;
}


}  // namespace Geom


/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :
