#include "src/System.h"

void setup()
{
    System.begin();
}

void loop()
{
    System.update();
    // NO delay() - breaks audio timing!
}
