struct V { float4 position:SV_POSITION; };
V VS(uint id:SV_VertexID){V v;float2 p=float2((id<<1)&2,id&2);v.position=float4(p*float2(2,-2)+float2(-1,1),0,1);return v;}
Texture2D source:register(t0);SamplerState linearClamp:register(s0);
cbuffer Blur:register(b0){float2 direction;float2 size;float radius;float sigma;float2 blurReserved;float4 weights[73];}
float4 PS(V v):SV_TARGET{float2 uv=v.position.xy/size;float4 c=0;[loop]for(int i=-int(radius);i<=int(radius);i++){uint n=uint(i+int(radius));c+=source.SampleLevel(linearClamp,uv+direction*float(i)/size,0)*weights[n/4u][n%4u];}return c;}

cbuffer Lens:register(b1){float2 outputSize;float2 panelSize;float2 panelOrigin;float cornerRadius;float displacement;float dpiScale;float lensReserved;float2 pointer;float4 tint;}
float boxDistance(float2 p,float2 halfSize,float radius){
    float2 q=abs(p)-halfSize+radius;
    return length(max(q,0))+min(max(q.x,q.y),0)-radius;
}
float3 softBackground(float2 pixel){return source.SampleLevel(linearClamp,pixel/outputSize,0).rgb;}
float4 LensPS(V v):SV_TARGET{
    float2 pixel=v.position.xy;
    float2 halfSize=panelSize*0.5;
    float2 p=pixel-panelOrigin-halfSize;
    float d=boxDistance(p,halfSize,cornerRadius);
    float coverage=1-smoothstep(-1.1,1.1,d);
    if(coverage<.001) return 0;
    float stepSize=.5*dpiScale;
    float2 normal=normalize(float2(
        boxDistance(p+float2(stepSize,0),halfSize,cornerRadius)-boxDistance(p-float2(stepSize,0),halfSize,cornerRadius),
        boxDistance(p+float2(0,stepSize),halfSize,cornerRadius)-boxDistance(p-float2(0,stepSize),halfSize,cornerRadius))+.00001);
    // Same meniscus as the approved spike, evaluated in DIP at every display scale.
    float depth=max(-d,0)/dpiScale;
    float bend=exp(-depth/14)*sin(min(depth/28,1)*1.5708);
    float2 shift=-normal*(bend*displacement*1.35+.20*displacement*exp(-depth/55));
    float2 sampleAt=pixel+shift;
    float3 color=softBackground(sampleAt);
    float chroma=bend*displacement*.035;
    color.r=lerp(color.r,softBackground(sampleAt+normal*chroma).r,.22);
    color.b=lerp(color.b,softBackground(sampleAt-normal*chroma).b,.22);
    color=lerp(color,tint.rgb,tint.a);
    float fresnel=exp(-depth/4.5);
    float rim=exp(-pow((depth-2.7)/2.8,2));
    float light=pow(saturate(dot(normal,normalize(float2(-.55,-.85)))),2);
    float counter=pow(saturate(dot(normal,normalize(float2(.6,.8)))),3);
    color=lerp(color,float3(1,1,1),fresnel*(.03+light*.16+counter*.07));
    color=lerp(color,float3(1,1,1),rim*(light*.12+counter*.06));
    color*=1-.045*exp(-pow((depth-8)/4,2));
    if(pointer.x>=0 && pointer.y>=0){
        float glint=pow(saturate(dot(normal,normalize(pointer-pixel+.001))),5);
        color=lerp(color,float3(1,1,1),glint*fresnel*.045);
    }
    color=lerp(color,float3(1,1,1),.025*pow(saturate(1-(pixel.y-panelOrigin.y)/panelSize.y),2));
    return float4(color*coverage,coverage);
}
