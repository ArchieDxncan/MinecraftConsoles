#pragma once

class Vec3i
{
protected: 
    int x;
    int y;
    int z;

public:
    Vec3i(int x, int y, int z);
    virtual ~Vec3i() = default;

    int getX() const;
    int getY() const;
    int getZ() const;

    
    double distSqr(const Vec3i& other) const; 
};