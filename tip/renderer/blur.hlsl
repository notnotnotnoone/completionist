struct V { float4 position:SV_POSITION; };
V VS(uint id:SV_VertexID){V v;float2 p=float2((id<<1)&2,id&2);v.position=float4(p*float2(2,-2)+float2(-1,1),0,1);return v;}
Texture2D source:register(t0);SamplerState linearClamp:register(s0);
cbuffer Blur:register(b0){float2 direction;float2 size;float radius;float sigma;float2 blurReserved;float4 weights[20];}
float4 PS(V v):SV_TARGET{float2 uv=v.position.xy/size;float4 c=0;[unroll]for(int i=-36;i<=36;i++){int n=i+36;if(abs(i)<=radius)c+=source.SampleLevel(linearClamp,uv+direction*float(i)/size,0)*weights[n/4][n%4];}return c;}

cbuffer Lens:register(b1){float2 outputSize;float2 panelSize;float2 panelOrigin;float cornerRadius;float displacement;float2 lensPadding;float2 lensReserved;float4 tint;}
float4 LensPS(V v):SV_TARGET{
    float2 p=v.position.xy-panelOrigin;
    float2 halfSize=panelSize*0.5;
    float2 q=abs(p-halfSize)-(halfSize-cornerRadius);
    float distance=length(max(q,0))+min(max(q.x,q.y),0)-cornerRadius;
    float edge=saturate(0.5-distance);
    float2 centered=(p-halfSize)/max(halfSize,1);
    float2 inward=normalize(float2(centered.x,centered.y*0.85)+float2(0.0001,0.0001));
    float profile=saturate(1.0-length(centered));
    float2 offset=inward*profile*displacement/max(panelSize,1);
    float2 uv=saturate((p+panelOrigin+offset*panelSize)/outputSize);
    float4 base=source.SampleLevel(linearClamp,uv,0);
    float2 dispersion=float2(0.0012,0.0007)*profile;
    float red=source.SampleLevel(linearClamp,saturate(uv+dispersion),0).r;
    float blue=source.SampleLevel(linearClamp,saturate(uv-dispersion),0).b;
    float3 glass=lerp(base.rgb,float3(red,base.g,blue),0.16);
    float reflection=pow(saturate(1.0-abs(centered.y+0.28)),5.0)*0.12;
    float meniscus=pow(saturate(1.0-abs(distance+1.6)),2.0)*0.07;
    float shadow=exp(-pow(max(distance+5.0,0.0)/8.0,2.0))*0.12;
    glass=lerp(glass,tint.rgb,tint.a);
    glass+=reflection+meniscus;
    float alpha=edge;
    return float4(glass*alpha,alpha)+float4(0,0,0,shadow*(1.0-alpha));
}
