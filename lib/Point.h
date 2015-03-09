#ifndef POINT_H
#define POINT_H 1

#include "Common/Options.h"
#include <stdio.h>
#include <vector>
#include <string>
#include <cstring> // for memcpy
#define NUM_SLOT 4 // Need to be changed to adjust automatically

class Point
{
    public:
        double x, y, z;
        double mass;
        Point () {}
        explicit Point(double x, 
                double y, double z, double mass)
            :x(x), y(y), z(z), mass(mass){
                memcpy(m_point, &x, sizeof x);
                memcpy(m_point + sizeof(double), &y, sizeof y);
                memcpy(m_point + sizeof(double)*2, &z, sizeof z);
                memcpy(m_point + sizeof(double)*3, &mass, sizeof mass);
            }

        static unsigned serialSize() {return NUM_BYTES;}
        //copy data to outgoing buffer to other procs
        //unroll struct into one dimension array
        size_t serialize(void* dest) 
        {
            memcpy(dest, m_point, sizeof m_point);
            return sizeof m_point;
        }

        //copy data from incoming buffer to Point
        //roll things into struct
        size_t unserialize(const void* src)
        {
            //This is now ugly, should be done through a function
            //I may want to change it to vector and use vector to hold 
            //things together
            memcpy(m_point, src, sizeof m_point);

            memcpy(&x, m_point, sizeof x);
            memcpy(&y, m_point + sizeof(double), sizeof y);
            memcpy(&z, m_point + sizeof(double) * 2, sizeof z);
            memcpy(&mass, m_point + sizeof(double) * 3, sizeof mass);

            return sizeof m_point;
        }
        static const unsigned NUM_BYTES =  sizeof(double) * NUM_SLOT;
        //DEBUG
        void print_m_point();
        void print_cord();
        unsigned getCode() const;
    protected:
        char m_point[NUM_BYTES];
};
#endif

