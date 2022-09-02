#pragma once

template<class T>
class Singleton
{
public:
    static T* Instance()
    {
        static T _instance;
        return &_instance;
    }

    Singleton(Singleton const&) = delete;
    Singleton& operator=(Singleton const&) = delete;

protected:
    Singleton() {}
    ~Singleton() {}
};
