#pragma once
#include <Spore/MathUtils.h>
#include <EASTL/vector.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace BetterCiv::TradeRoutes {
using Point = Math::Vector3;
using Path = eastl::vector<Point>;
constexpr unsigned kMaxPoints = 4096;
inline bool Finite(const Point& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
inline float Length(const Point& p) { return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z); }
inline float Distance(const Point& a,const Point& b) { return Length(a-b); }
inline uint64_t HashBytes(uint64_t h,const void* data,size_t size) {
    const auto* bytes=static_cast<const unsigned char*>(data);
    for(size_t i=0;i<size;++i) h=(h^bytes[i])*1099511628211ull;
    return h;
}
inline uint64_t HashPath(const Path& path) {
    return HashBytes(14695981039346656037ull,path.data(),path.size()*sizeof(Point));
}

// Geometry for a DISPLAY ribbon only. Do not use this as a vehicle pathfinder.
// Resample chords on the terrain so a long segment cannot skip a lake or cliff.
template<class Height>
bool GroundPath(const Path& input,float sea,Height height,Path& output,const char*& reason,float* surface_error=nullptr) {
    output.clear(); reason="invalid-path";
    if(surface_error) *surface_error=0;
    if(input.size()<2 || input.size()>kMaxPoints || !std::isfinite(sea) || sea<100) return false;
    Point previous{}; bool have_previous=false;
    for(size_t i=0;i<input.size();++i) {
        const auto a=i?input[i-1]:input[i], b=input[i];
        if(!Finite(a)||!Finite(b)||Length(a)<100||Length(b)<100) return false;
        const float length=Distance(a,b);
        if(!std::isfinite(length)||length>10000) return false;
        const unsigned steps=i?unsigned(std::ceil(length/4.0f)):1u;
        if(steps>kMaxPoints || output.size()+steps>kMaxPoints) {reason="path-limit";return false;}
        for(unsigned j=1;j<=steps;++j) {
            auto raw=a+(b-a)*(float(j)/float(steps));
            const float radius=Length(raw);
            if(!std::isfinite(radius)||radius<100) return false;
            raw=raw*(1.0f/radius);
            const float ground=height(raw);
            if(!std::isfinite(ground)||ground<100) {reason="invalid-terrain";return false;}
            if(ground<=sea+0.15f) {reason="water-crossing";return false;}
            if(surface_error) *surface_error=(std::max)(*surface_error,std::fabs(radius-ground));
            const auto projected=raw*(ground+0.35f);
            if(have_previous) {
                const float radial=std::fabs(Length(previous)-Length(projected));
                const float chord=Distance(previous,projected);
                const float tangent=std::sqrt((std::max)(0.0f,chord*chord-radial*radial));
                if(radial>2.0f && radial>1.3f*tangent) {reason="steep-crossing";return false;}
            }
            output.push_back(projected); previous=projected;have_previous=true;
        }
    }
    if(output.size()<2) return false;
    reason="ground-path";return true;
}
struct Ends { Point center; float inner=0,outer=0; };
inline bool NearEdge(const Point& p,const Ends& city) {
    const float d=Distance(p,city.center);
    return std::isfinite(d) && d>=(std::max)(5.0f,city.inner*0.5f) && d<=city.outer+35.0f;
}
inline bool HasCityEnds(const Path& path,const Ends& a,const Ends& b) {
    if(path.size()<2) return false;
    return (NearEdge(path.front(),a)&&NearEdge(path.back(),b)) ||
           (NearEdge(path.front(),b)&&NearEdge(path.back(),a));
}
struct Record {
    uint64_t city1=0,city2=0,topology=0,source_hash=0,path_hash=0,seen=0;
    unsigned attempt=0; bool pending=false;
    Path replacement;
};
struct State {
    Record records[64]; uint64_t next_scan=0,next_search=0;unsigned lines=0;
    void ResetSession() { *this={}; }
    Record& Get(uint64_t a,uint64_t b,uint64_t now) {
        auto* oldest=&records[0];
        for(auto& r:records) {
            if(r.city1==a&&r.city2==b) {r.seen=now;return r;}
            if(r.seen<oldest->seen) oldest=&r;
        }
        *oldest={};oldest->city1=a;oldest->city2=b;oldest->seen=now;return *oldest;
    }
};
}
