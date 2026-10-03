struct V { float4 position:SV_POSITION; };
V VS(uint id:SV_VertexID){V v;float2 p=float2((id<<1)&2,id&2);v.position=float4(p*float2(2,-2)+float2(-1,1),0,1);return v;}
Texture2D source:register(t0);SamplerState linearClamp:register(s0);
cbuffer Blur:register(b0){float2 direction;float2 size;float4 weights[10];}
float4 PS(V v):SV_TARGET{float2 uv=v.position.xy/size;float4 c=0;[unroll]for(int i=-18;i<=18;i++){int n=i+18;c+=source.SampleLevel(linearClamp,uv+direction*float(i)/size,0)*weights[n/4][n%4];}return c;}
