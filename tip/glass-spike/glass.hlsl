// THROWAWAY material study. All refraction samples the demo's own background.
Texture2D backdrop : register(t0);
Texture2D glyphs : register(t1);
Texture2D blurredBackdrop : register(t2);
SamplerState linearClamp : register(s0);
cbuffer Params : register(b0) {
    float2 viewport; float2 menuPosition;
    float strength; float time; float dark; float plain;
    float collapsed; float pointerX; float pointerY; float padding;
};
struct Vertex { float4 position : SV_POSITION; float2 uv : TEXCOORD; };
Vertex VS(uint id : SV_VertexID) {
    Vertex v;
    v.uv = float2((id << 1) & 2, id & 2);
    v.position = float4(v.uv * float2(2,-2) + float2(-1,1),0,1);
    return v;
}
float boxDistance(float2 p, float2 halfSize, float radius) {
    float2 q = abs(p) - halfSize + radius;
    return length(max(q,0)) + min(max(q.x,q.y),0) - radius;
}
float3 background(float2 p) { return backdrop.SampleLevel(linearClamp,p/viewport,0).rgb; }
float3 softBackground(float2 p) { return blurredBackdrop.SampleLevel(linearClamp,p/viewport,0).rgb; }
float3 glass(float2 pixel, float2 origin, float2 size, float3 base) {
    float2 p = pixel-origin-size*.5;
    float radius = min(26,size.y*.45);
    float d = boxDistance(p,size*.5,radius);
    float coverage = 1-smoothstep(-1.1,1.1,d);
    float shadowDistance = max(boxDistance(p-float2(0,5),size*.5,radius),0);
    float shadow = exp(-shadowDistance*shadowDistance/420)*.12*(1-coverage);
    base *= 1-shadow;
    if (coverage < .001) return base;
    float2 normal = normalize(float2(
        boxDistance(p+float2(.5,0),size*.5,radius)-boxDistance(p-float2(.5,0),size*.5,radius),
        boxDistance(p+float2(0,.5),size*.5,radius)-boxDistance(p-float2(0,.5),size*.5,radius)) + .00001);
    float depth = max(-d,0);
    // Thick rounded meniscus, tapering to a almost-flat readable center.
    float bend = exp(-depth/14)*sin(min(depth/28,1)*1.5708);
    float2 shift = -normal*(bend*strength*1.35 + .20*strength*exp(-depth/55));
    if (plain > .5) shift = 0;
    float2 sampleAt = pixel+shift;
    float3 tint = lerp(float3(.984,.988,.973),float3(.078,.110,.094),dark);
    // Refract a fully convolved image, without sparse taps or sharp channel samples.
    float3 color = softBackground(sampleAt);
    if (plain < .5) {
        float chroma = bend*strength*.035;
        color.r = lerp(color.r,softBackground(sampleAt+normal*chroma).r,.22);
        color.b = lerp(color.b,softBackground(sampleAt-normal*chroma).b,.22);
    }
    color = lerp(color,tint,plain > .5 ? .70 : lerp(.24,.35,dark));
    if (plain < .5) {
        float fresnel = exp(-depth/4.5);
        float rim = exp(-pow((depth-2.7)/2.8,2));
        float light = pow(saturate(dot(normal,normalize(float2(-.55,-.85)))),2);
        float counter = pow(saturate(dot(normal,normalize(float2(.6,.8)))),3);
        // Blend reflections toward light instead of adding clipped white outlines.
        color = lerp(color,float3(1,1,1),fresnel*(.03+light*.16+counter*.07));
        color = lerp(color,float3(1,1,1),rim*(light*.12+counter*.06));
        color *= 1-.045*exp(-pow((depth-8)/4,2));
        float2 pointer = float2(pointerX,pointerY)-origin-size*.5;
        float glint = pow(saturate(dot(normal,normalize(pointer-p+.001))),5);
        color = lerp(color,float3(1,1,1),glint*fresnel*.045);
        // Broad, low-contrast reflection across the top face.
        color = lerp(color,float3(1,1,1),.025*pow(saturate(1-(pixel.y-origin.y)/size.y),2));
    }
    return lerp(base,color,coverage);
}
float4 PS(Vertex v) : SV_TARGET {
    float2 pixel = v.uv*viewport;
    float3 color = background(pixel);
    float2 size = float2(350,286);
    color = glass(pixel,menuPosition,size,color);
    float2 local = pixel-menuPosition;
    bool inside = local.x>14 && local.x<336 && local.y>14 && local.y<272;
    float3 ink = lerp(float3(.086,.133,.110),float3(.894,.922,.902),dark);
    float3 accent = lerp(float3(.671,.247,.518),float3(.753,.094,.553),dark);
    if (inside) {
        float row = boxDistance(local-float2(175,56),float2(160,21),12);
        color = lerp(color,accent,(1-smoothstep(-1,1,row))*.10);
        float divider = exp(-pow((local.y-174)/.8,2));
        color = lerp(color,ink,divider*.08);
        float a = local.y < 237 ? glyphs.SampleLevel(linearClamp,local/float2(350,286),0).r : 0;
        color = lerp(color,ink,a);
        // Simulated connection / activity accents use existing Evergreen roles.
        float activityDot = 1-smoothstep(2.2,3.3,length(local-float2(23,198)));
        color = lerp(color,accent,activityDot*.85);
        float progress = 1-smoothstep(-.6,.6,boxDistance(local-float2(19+153*frac(time/5),265),float2(153*frac(time/5),1),1));
        color = lerp(color,accent,progress*.6);
    }
    float2 dock = float2(28,viewport.y-108);
    float2 dockSize = float2(190,collapsed>.5 ? 42 : 78);
    color = glass(pixel,dock,dockSize,color);
    float2 q = pixel-dock;
    float connectionDot = 1-smoothstep(2.2,3.4,length(q-float2(19,21)));
    color = lerp(color,lerp(float3(.122,.420,.271),float3(.373,.796,.557),dark),connectionDot);
    // Reuse the glyph atlas's info labels in the compact dock.
    if(q.x>32 && q.x<175 && q.y>8 && q.y<33) {
        float a = glyphs.SampleLevel(linearClamp,float2(q.x-16,q.y+8)/float2(350,286),0).r;
        color = lerp(color,ink,a*.85);
    }
    if(collapsed<.5 && q.x>12 && q.x<175 && q.y>40 && q.y<67) {
        float a = glyphs.SampleLevel(linearClamp,float2(q.x+1,q.y+195)/float2(350,286),0).r;
        color = lerp(color,ink,a*.60);
    }
    return float4(saturate(color),1);
}
