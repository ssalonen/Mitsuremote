#include <Arduino.h>
#include "utils.h"

bool streq(const char *a, const char *b)
{
    if (a == NULL)
    {
        return b == NULL;
    }
    else if (b == NULL)
    {
        return a == NULL;
    }
    else
    {
        return strcmp(a, b) == 0;
    }
}
