#ifndef REN_COMPLEX
#define REN_COMPLEX

namespace ren
{
float2 cadd(float2 a, float2 b)
{
    return a + b;
}

float2 csub(float2 a, float2 b)
{
    return a - b;
}

float2 cmul(float2 a, float2 b)
{
    return float2(a.x * b.x - a.y * b.y, a.y * b.x + a.x * b.y);
}

float2 cdiv(float2 a, float2 b)
{
    float r = a.x * b.x + a.y * b.y;
    float i = a.y * b.x - a.x * b.y;
    float d = b.x * b.x + b.y * b.y;
    return float2((r/d),(i/d));
}

float2 cconjugate(float2 a)
{
    return float2(a.x, -a.y);
}

float2 cpolar(float r, float phi)
{
    float2 result;
    sincos(phi, result.y, result.x);
    return r * result;
}
}

#endif
