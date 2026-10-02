// Exercise the production topology conversion without a GPU.
#include "cross2d/platforms/gl2/ps5_triangles.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
using namespace c2d;
struct V { int id; float x,y,u,v; uint32_t color; };
static void check(PrimitiveType type, const std::vector<V>& src, const std::vector<int>& expected) {
    std::vector<V> dst;
    ps5ExpandTriangles(src,type,dst);
    assert(dst.size()==expected.size());
    assert(dst.size()==ps5TriangleVertexCount(type,src.size()));
    for(std::size_t i=0;i<dst.size();++i)
        assert(!std::memcmp(&dst[i],&src[expected[i]],sizeof(V)));
}
int main() {
    std::vector<V> v;
    for(int i=0;i<6;++i) v.push_back({i,float(i%2),float(i/2),float(i)/7,float(i)/9,0xff001122u+unsigned(i)});
    check(TriangleStrip,v,{0,1,2,2,1,3,2,3,4,4,3,5});
    check(TriangleFan,v,{0,1,2,0,2,3,0,3,4,0,4,5});
    check(Triangles,v,{0,1,2,3,4,5});
    check(Lines,v,{0,1,2,3,4,5});
    for(auto type:{TriangleStrip,TriangleFan}) {
        for(unsigned n=0;n<3;++n) check(type,std::vector<V>(v.begin(),v.begin()+n),{});
        check(type,std::vector<V>(v.begin(),v.begin()+3),{0,1,2});
    }
    // A fan's repeated closing vertex and degenerate strips must remain present.
    v[5]=v[1];v[3]=v[2];
    check(TriangleStrip,v,{0,1,2,2,1,3,2,3,4,4,3,5});
    check(TriangleFan,v,{0,1,2,0,2,3,0,3,4,0,4,5});
    puts("PASS production fan/strip expansion: winding indices, primitive order, last provoking vertex, complete vertex attributes, small/degenerate inputs, unchanged lists");
}
