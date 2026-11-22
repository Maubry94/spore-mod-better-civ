#pragma once
#include "TradeRouteGeometry.h"
#include "engine/ReferenceCode.h"
#include <array>
#include <queue>
#include <vector>

namespace BetterCiv::SeaRibbon {
using TradeRoutes::Point;
using TradeRoutes::Path;
using TradeRoutes::Length;
using TradeRoutes::Distance;
using TradeRoutes::Finite;

inline bool ValidateNative(const unsigned char* base) {
    if (!base) return false;
    const struct {uint32_t address,size,hash;} ranges[]={
        {0xb8cbb1,280,0xcf258726}, // copy positions, synthesize a direct segment for an empty result
        {0xb86b6b,37,0xa338f8e3},  // ribbon draw exits for fewer than two points
        {0xb8ce75,17,0xbe656dba},  // forward draw and reverse
        {0xb856c0,18,0x184460bf},  // reverse exits for a singleton
        {0xb8cf43,11,0x854955b5}   // backward draw
    };
    for(const auto& r:ranges) {
        uint32_t hash=0;
        if(!Native::HashReferenceCode(base+r.address-0x400000,r.size,0,{},hash) || hash!=r.hash) return false;
    }
    return true;
}

inline float Dot(const Point& a,const Point& b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Point Cross(const Point& a,const Point& b) {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
inline Point Direction(const Point& p) {
    const float length=Length(p);
    return std::isfinite(length) && length>0.0001f ? p*(1.0f/length) : Point{0,0,0};
}
enum class Surface { Water,Land,Unknown };
enum class Result { Pending,Found,NoPath,Unknown };
constexpr float pi=3.14159265f;
constexpr float sample_spacing=3;
constexpr unsigned max_samples=8192;

template<class Height>
Surface Water(const Point& p,float sea,Height height) {
    const float h=height(p);
    if (!std::isfinite(h) || h<100 || h>10000) return Surface::Unknown;
    return h<=sea-0.15f ? Surface::Water : Surface::Land;
}
// The renderer inserts midpoints between submitted vertices. Bound that
// interpolation as well as the terrain queries, even for near-antipodal ports.
template<class Height>
Surface Segment(const Point& a,const Point& b,float sea,Height height,unsigned& budget,Path* output=nullptr) {
    if (!Finite(a) || !Finite(b) || !std::isfinite(sea) || sea<100 || sea>10000 ||
        Length(a)<100 || Length(b)<100) return Surface::Unknown;
    const auto from=Direction(a),to=Direction(b);
    const float cosine=std::clamp(Dot(from,to),-1.0f,1.0f);
    const float angle=std::atan2(Length(Cross(from,to)),cosine);
    if (angle>pi-0.001f) return Surface::Land; // an ambiguous arc needs explicit waypoints
    const unsigned steps=(std::max)(1u,unsigned(std::ceil(sea*angle/sample_spacing)));
    if (steps+1>budget || (output && output->size()+steps+1>TradeRoutes::kMaxPoints)) return Surface::Unknown;
    const auto tangent=Direction(to-from*cosine);
    for (unsigned j=0;j<=steps;++j) {
        Point p;
        if (!j) p=from*sea;
        else if (j==steps) p=to*sea;
        else {
            const float t=angle*j/steps;
            p=(from*std::cos(t)+tangent*std::sin(t))*sea;
        }
        --budget;
        const auto s=Water(p,sea,height);
        if (s!=Surface::Water) return s;
        if (output && (j || output->empty())) output->push_back(p);
    }
    return Surface::Water;
}
template<class Height>
Surface Inspect(const Path& input,float sea,Height height) {
    if (input.size()<2 || input.size()>TradeRoutes::kMaxPoints) return Surface::Unknown;
    unsigned budget=max_samples;
    for (size_t i=1;i<input.size();++i) {
        const auto s=Segment(input[i-1],input[i],sea,height,budget);
        if (s!=Surface::Water) return s;
    }
    return Surface::Water;
}

// Only the display job owns this graph. The simulator retains the real ship
// route and its orders. Generate this latitude/longitude grid once per process.
constexpr int columns=48,rows=25,vertices=rows*columns;
inline int Index(int row,int column) { return row*columns+(column+columns)%columns; }
inline const std::array<Point,vertices>& Grid() {
    static const auto grid=[] {
        std::array<Point,vertices> dirs{};
        for (int r=0;r<rows;++r) for (int c=0;c<columns;++c) {
            const float lat=pi*(float(r)/(rows-1)-0.5f),lon=2*pi*c/columns;
            dirs[Index(r,c)]={std::cos(lat)*std::cos(lon),std::cos(lat)*std::sin(lon),std::sin(lat)};
        }
        return dirs;
    }();
    return grid;
}

class Planner {
    struct Visit {
        int vertex;
        float estimate;
        bool operator<(const Visit& other) const { return estimate>other.estimate; }
    };
    Point start{},finish{};
    float sea=0,reach=0;
    std::array<float,vertices> distance{};
    std::array<int,vertices> parent{};
    std::array<bool,vertices> closed{};
    std::priority_queue<Visit> open;
    unsigned expanded=0;
    bool unknown=false;

    template<class Height>
    bool Anchor(const Point& source,Height height,Point& out) {
        const auto up=Direction(source);
        if (!Finite(source) || Length(source)<100) { unknown=true;return false; }
        const auto right=Direction(Cross(up,std::fabs(up.z)<0.8f?Point{0,0,1}:Point{0,1,0}));
        const auto forward=Cross(up,right);
        // Some stock graphical dock endpoints are just above the shoreline.
        for (const float radius:{0.f,3.f,7.f,14.f,24.f}) for (int i=0;i<(radius?12:1);++i) {
            const float angle=i*pi/6;
            const auto p=Direction(up*sea+(right*std::cos(angle)+forward*std::sin(angle))*radius)*sea;
            const auto s=Water(p,sea,height);
            if (s==Surface::Unknown) {unknown=true;return false;}
            if (s==Surface::Water) {out=p;return true;}
        }
        return false;
    }
    template<class Height>
    bool Edge(const Point& a,const Point& b,Height height) {
        unsigned budget=max_samples;
        const auto status=Segment(a,b,sea,height,budget);
        if(status==Surface::Unknown) unknown=true;
        return status==Surface::Water;
    }
    template<class Height>
    Result Emit(const Path& corners,Height height,Path& result) {
        Path output;
        unsigned budget=max_samples;
        for(size_t i=1;i<corners.size();++i)
            if(Segment(corners[i-1],corners[i],sea,height,budget,&output)!=Surface::Water) return Result::Unknown;
        result.swap(output);return Result::Found;
    }
public:
    template<class Height>
    Result Begin(const Point& a,const Point& b,float level,Height height,Path& out) {
        sea=level;unknown=false;expanded=0;open={};distance.fill(1e30f);parent.fill(-1);closed.fill(false);
        if(!std::isfinite(sea) || sea<100 || sea>10000) return Result::Unknown;
        if(!Anchor(a,height,start) || !Anchor(b,height,finish)) return unknown?Result::Unknown:Result::NoPath;
        if(Edge(start,finish,height)) return Emit(Path{start,finish},height,out);
        if(unknown) return Result::Unknown;
        reach=(std::min)(160.f,2.2f*pi*sea/(rows-1));
        const auto up=Direction(start);
        const int row=int(std::lround((std::asin(std::clamp(up.z,-1.f,1.f))/pi+0.5f)*(rows-1)));
        const int col=int(std::lround(std::atan2(up.y,up.x)*columns/(2*pi)))+columns;
        for(int r=(std::max)(0,row-2);r<=(std::min)(rows-1,row+2);++r)
            for(int dc=-2;dc<=2;++dc) {
                const int idx=Index(r,col+dc);
                const Point pos=Grid()[idx]*sea;
                const float d=Distance(start,pos);
                if(d<=reach && Edge(start,pos,height)) {
                    distance[idx]=d;open.push({idx,d+Distance(pos,finish)});
                }
            }
        return unknown?Result::Unknown:Result::Pending;
    }
    template<class Height>
    Result Step(Height height,Path& out) {
        // Advance one job in short batches. Never do an exhaustive planet
        // search in a draw call. Stops after at most one visit per graph node.
        for(int n=0;n<6 && !open.empty();) {
            const int here=open.top().vertex;open.pop();
            if(closed[here]) continue;
            closed[here]=true;++n;++expanded;
            const Point pos=Grid()[here]*sea;
            if(Distance(pos,finish)<=reach && Edge(pos,finish,height)) {
                Path corners{finish};
                for(int p=here;p>=0;p=parent[p]) corners.push_back(Grid()[p]*sea);
                corners.push_back(start);
                std::reverse(corners.begin(),corners.end());
                return Emit(corners,height,out);
            }
            const int row=here/columns,col=here%columns;
            for(int dr=-1;dr<=1;++dr) for(int dc=-1;dc<=1;++dc) {
                if ((!dr&&!dc) || row+dr<0 || row+dr>=rows) continue;
                const int next=Index(row+dr,col+dc);
                if(closed[next]) continue;
                const auto dest=Grid()[next]*sea;
                const float cost=distance[here]+Distance(pos,dest);
                if(cost>=distance[next] || !Edge(pos,dest,height)) continue;
                parent[next]=here;distance[next]=cost;open.push({next,cost+Distance(dest,finish)});
            }
            if(unknown) return Result::Unknown;
        }
        return open.empty() || expanded>=vertices ? Result::NoPath : Result::Pending;
    }
};
}
