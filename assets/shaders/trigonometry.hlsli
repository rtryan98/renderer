#ifndef REN_TRIGONOMETRY
#define REN_TRIGONOMETRY

namespace ren
{
float sech(float x)
{
    return 1. / cosh(x);
}

float csch(float x)
{
    return 1. / sinh(x);
}
}

#endif
