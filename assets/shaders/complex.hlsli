#ifndef REN_COMPLEX
#define REN_COMPLEX

namespace ren
{
template<typename T>
vector<T, 2> cadd(vector<T, 2> a, vector<T, 2> b)
{
    return a + b;
}

template<typename T>
vector<T, 2> csub(vector<T, 2> a, vector<T, 2> b)
{
    return a - b;
}

template<typename T>
vector<T, 2> cmul(vector<T, 2> a, vector<T, 2> b)
{
    return vector<T, 2>(a.x * b.x - a.y * b.y, a.y * b.x + a.x * b.y);
}

template<typename T>
vector<T, 2> cdiv(vector<T, 2> a, vector<T, 2> b)
{
    float r = a.x * b.x + a.y * b.y;
    float i = a.y * b.x - a.x * b.y;
    float d = b.x * b.x + b.y * b.y;
    return vector<T, 2>((r/d),(i/d));
}

template<typename T>
vector<T, 2> cconjugate(vector<T, 2> a)
{
    return vector<T, 2>(a.x, -a.y);
}

template<typename T>
vector<T, 2> cpolar(T r, T phi)
{
    vector<T, 2> result;
    sincos(phi, result.y, result.x);
    return r * result;
}

template<typename T>
vector<T, 2> cmuli(vector<T, 2> a)
{
    return vector<T, 2>(-a.y, a.x);
}
}

#endif
