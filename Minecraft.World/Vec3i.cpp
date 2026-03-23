#include "stdafx.h"
#include "Vec3i.h"
#include <cmath>

Vec3i::Vec3i(int x, int y, int z) : x(x), y(y), z(z)
{
}

int Vec3i::getX() const
{
    return this->x; 
}

int Vec3i::getY() const
{
    return this->y; 
}

int Vec3i::getZ() const
{
    return this->z; 
}

double Vec3i::distSqr(const Vec3i& other) const
{
    double dx = (double)(this->x - other.getX());
    double dy = (double)(this->y - other.getY());
    double dz = (double)(this->z - other.getZ());
    return dx * dx + dy * dy + dz * dz;
}