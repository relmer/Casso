#pragma once



typedef unsigned char   byte;
typedef unsigned short  word;



struct Flags
{
    byte c : 1;
    byte z : 1;
    byte i : 1;
    byte d : 1;
    byte b : 1;
    byte v : 1;
    byte n : 1;
};


union Status
{
    byte    status;
    Flags   flags;
};



class Cpu
{
public:
    void Reset ();

protected:
    word    pc;
    word    sp;

    Status  status;
};

