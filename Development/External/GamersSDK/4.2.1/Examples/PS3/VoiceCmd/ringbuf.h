/*   SCE CONFIDENTIAL                                       */
/*   PLAYSTATION(R)3 Programmer Tool Runtime Library 110.006 */
/*   Copyright (C) 2006 Sony Computer Entertainment Inc.    */
/*   All Rights Reserved.                                   */

#ifndef _RINGBUFFER_H
#define _RINGBUFFER_H

#undef SP_MIN2
#undef SP_MAX2
#undef SP_MIN
#undef SP_MAX
#undef SP_SWAP
#undef SP_DIM

/* for fixed type, min & max without branch jumps */
#define SP_MIN2(a, b)  ((a) ^ (((a)^(b)) & (((a) < (b)) - 1)))
#define SP_MAX2(a, b)  ((a) ^ (((a)^(b)) & (((a) > (b)) - 1)))

/* for float type, min & max with branch jump */
#define SP_MIN(a,b)  ((a) > (b) ? (b) : (a))
#define SP_MAX(a,b)  ((a) < (b) ? (b) : (a))

#define SP_SWAP(a,b,temp) do { (temp) = (a), (a) = (b), (b) = (temp); } while (0)
#define SP_DIM(array)  (sizeof(array)/sizeof((array)[0]))

//--------------------------------------------------------------------------
// read-write-safe ring buffer implementation: does not use mutex protection
// the writer thread changes pointer <buf_free>, 
// the reader thread changes pointer <buf_full>
class RingBuffer
{
public:
    RingBuffer(void)
    {
        buffer = NULL;
        buf_size = buf_full = buf_free = 0;
    }

    void Init(char* szBuf, int bytes)
    {
        buffer = szBuf;
        buf_size = bytes;
        buf_full = buf_free = 0;
    }

    void Reset(void)
    {
        buf_full = buf_free = 0;
    }

    int  DataSize(void)
    {
        return (buf_free - buf_full);
    }

    int Write(char* data, int len)
    {
        if (len <= 0) return len;
        int data_size = buf_size - (buf_free - buf_full);
        if (len > data_size)
          len = data_size;
        data_size = buf_size - (buf_free % buf_size);
        if (data_size > len)
          data_size = len;
        memcpy(buffer + (buf_free % buf_size), data, data_size);
        if (data_size != len)
          memcpy(buffer, data + data_size, len - data_size);
        buf_free += len;
        return len;
    }

    int Read(char* data, int max_bytes)
    {
        int result = buf_free - buf_full;
        if (result > max_bytes)
            result = max_bytes;
        int chunk = buf_size - (buf_full % buf_size);
        if (chunk > result)
            chunk = result;
        memcpy(data, buffer + (buf_full % buf_size), chunk);
        if (chunk != result)
            memcpy(data + chunk, buffer, result - chunk);
        buf_full += result;
        return result;
    }

    void ResetByWriter(void)
    {
        buf_free = buf_full;
    }

    void ResetByReader(void)
    {
        buf_full = buf_free;
    }

private:
    char*   buffer;
    int     buf_size;
    int     buf_full;
    int     buf_free;
};
///////////////////////////////////////////////////////////////////////////////////
#endif //_RINGBUFFER_H
